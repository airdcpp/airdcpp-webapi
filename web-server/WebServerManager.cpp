/*
* Copyright (C) 2011-2015 AirDC++ Project
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <web-server/stdinc.h>
#include <web-server/WebServerManager.h>

#include <airdcpp/typedefs.h>

#include <airdcpp/LogManager.h>
#include <airdcpp/SettingsManager.h>

#define CONFIG_NAME "WebServer.xml"
#define CONFIG_DIR Util::PATH_USER_CONFIG

#define HANDSHAKE_TIMEOUT 0 // disabled, affects HTTP downloads

namespace webserver {
	void WebServerManager::startup() {
		WebUserManager::newInstance();

		WebServerManager::getInstance()->start();
	}

	void WebServerManager::shutdown() {
		WebServerManager::getInstance()->stop();

		WebUserManager::deleteInstance();
	}

	using namespace dcpp;
	WebServerManager::WebServerManager() : ios(2) {

	}

	void WebServerManager::start() {
		load();

		// initialize asio with our external io_service rather than an internal one
		endpoint_plain.init_asio(&ios);

		endpoint_plain.set_http_handler(
			std::bind(&WebServerManager::on_http<server_plain>, this, &endpoint_plain, _1, false));
		endpoint_plain.set_message_handler(
			std::bind(&WebServerManager::on_message<server_plain>, this, &endpoint_plain, _1, _2, false));
		endpoint_plain.set_close_handler(std::bind(&WebServerManager::on_close_socket, this, _1));
		endpoint_plain.set_open_handler(std::bind(&WebServerManager::on_open_socket<server_plain>, this, &endpoint_plain, _1, false));


		// Failures (plain)
		endpoint_plain.set_interrupt_handler([](websocketpp::connection_hdl hdl) { dcdebug("Connection interrupted\n"); });
		endpoint_plain.set_fail_handler(std::bind(&WebServerManager::on_failed<server_plain>, this, &endpoint_plain, _1));
		endpoint_plain.set_open_handshake_timeout(HANDSHAKE_TIMEOUT);

		// set up tls endpoint
		endpoint_tls.init_asio(&ios);
		endpoint_tls.set_message_handler(
			std::bind(&WebServerManager::on_message<server_tls>, this, &endpoint_tls, _1, _2, true));

		endpoint_tls.set_close_handler(std::bind(&WebServerManager::on_close_socket, this, _1));
		endpoint_tls.set_open_handler(std::bind(&WebServerManager::on_open_socket<server_tls>, this, &endpoint_tls, _1, true));
		endpoint_tls.set_http_handler(std::bind(&WebServerManager::on_http<server_tls>, this, &endpoint_tls, _1, true));

		// Failures (TLS)
		endpoint_tls.set_fail_handler(std::bind(&WebServerManager::on_failed<server_tls>, this, &endpoint_tls, _1));
		endpoint_tls.set_open_handshake_timeout(HANDSHAKE_TIMEOUT);

		// TLS endpoint has an extra handler for the tls init
		endpoint_tls.set_tls_init_handler(std::bind(&WebServerManager::on_tls_init, this, _1));

		try {
			endpoint_plain.listen(80);
			endpoint_plain.start_accept();
		}
		catch (const websocketpp::exception& e) {
			LogManager::getInstance()->message("Failed to set up plain server: " + string(e.what()), LogManager::LOG_ERROR);
		}

		try {
			endpoint_tls.listen(443);
			endpoint_tls.start_accept();
		} catch (const websocketpp::exception& e) {
			LogManager::getInstance()->message("Failed to set up secure server: " + string(e.what()), LogManager::LOG_ERROR);
		}

		// Start the ASIO io_service run loop running both endpoints
		for (int x = 0; x < 2; ++x) {
			worker_threads.create_thread(boost::bind(&boost::asio::io_service::run, &ios));
		}

	}

	context_ptr WebServerManager::on_tls_init(websocketpp::connection_hdl hdl) {
		//std::cout << "on_tls_init called with hdl: " << hdl.lock().get() << std::endl;
		context_ptr ctx(new boost::asio::ssl::context(boost::asio::ssl::context::tlsv12));

		try {
			ctx->set_options(boost::asio::ssl::context::default_workarounds |
				boost::asio::ssl::context::no_sslv2 |
				boost::asio::ssl::context::no_sslv3 |
				boost::asio::ssl::context::single_dh_use);

			ctx->use_certificate_file(SETTING(TLS_CERTIFICATE_FILE), boost::asio::ssl::context::pem);
			ctx->use_private_key_file(SETTING(TLS_PRIVATE_KEY_FILE), boost::asio::ssl::context::pem);
		} catch (std::exception& e) {
			//std::cout << e.what() << std::endl;
			dcdebug("TLS init failed: %s", e.what());
		}

		return ctx;
	}

	void WebServerManager::disconnectSockets(const string& aMessage) noexcept {
		RLock l(cs);
		for (const auto& socket : sockets | map_values) {
			socket->close(websocketpp::close::status::going_away, aMessage);
		}
	}

	void WebServerManager::stop() {
		// we have an issue if a socket connects instantly after getting disconnected otherwise
		shuttingDown = true;

		disconnectSockets("Shutting down");

		bool hasSockets = false;

		for (;;) {
			{
				RLock l(cs);
				hasSockets = !sockets.empty();
			}

			if (hasSockets) {
				Thread::sleep(50);
			} else {
				break;
			}
		}

		ios.stop();

		while (!ios.stopped())
			Thread::sleep(50);

		save();
	}

	WebServerManager::~WebServerManager() {

	}

	void WebServerManager::logout(const string& aSessionToken) noexcept {
		vector<WebSocketPtr> sessionSockets;

		{
			RLock l(cs);
			boost::algorithm::copy_if(sockets | map_values, back_inserter(sessionSockets),
				[&](const WebSocketPtr& aSocket) {
				return aSocket->getSession() && aSocket->getSession()->getToken() == aSessionToken;
			}
			);
		}

		for (const auto& s : sessionSockets) {
			s->getSession()->onSocketDisconnected();
			s->setSession(nullptr);
		}
	}

	WebSocketPtr WebServerManager::getSocket(const std::string& aSessionToken) noexcept {
		RLock l(cs);
		auto i = find_if(sockets | map_values, [&](const WebSocketPtr& s) {
			return s->getSession() && s->getSession()->getToken() == aSessionToken;
		});

		return i.base() == sockets.end() ? nullptr : *i;
	}

	TimerPtr WebServerManager::addTimer(Timer::CallBack&& aCallBack, time_t aIntervalMillis) noexcept {
		return make_shared<Timer>(move(aCallBack), ios, aIntervalMillis);
	}

	void WebServerManager::on_close_socket(websocketpp::connection_hdl hdl) {
		WebSocketPtr socket = nullptr;

		{
			WLock l(cs);
			auto s = sockets.find(hdl);
			dcassert(s != sockets.end());
			if (s == sockets.end()) {
				return;
			}

			socket = s->second;
			sockets.erase(s);
		}

		if (socket->getSession()) {
			socket->getSession()->onSocketDisconnected();
		}
	}

	void WebServerManager::load() noexcept {
		SimpleXML xml;
		SettingsManager::loadSettingFile(xml, CONFIG_DIR, CONFIG_NAME, true);
		if (xml.findChild("WebServer")) {
			xml.stepIn();
			WebUserManager::getInstance()->load(xml);
			xml.stepOut();
		}
	}

	void WebServerManager::save() const noexcept {
		SimpleXML xml;

		xml.addTag("WebServer");
		xml.stepIn();

		WebUserManager::getInstance()->save(xml);

		xml.stepOut();

		SettingsManager::saveSettingFile(xml, CONFIG_DIR, CONFIG_NAME);
	}
}