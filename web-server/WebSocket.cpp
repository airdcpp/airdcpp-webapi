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
#include <web-server/WebSocket.h>

#include <client/Util.h>

namespace webserver {
	WebSocket::WebSocket(bool aIsSecure, websocketpp::connection_hdl aHdl/*, ApiModule* aHandler*/) :
		secure(aIsSecure), hdl(aHdl) {

	}

	WebSocket::~WebSocket() {
		if (handler)
			handler->setSocket(nullptr);
	}

	void WebSocket::sendApiResponse(json& aJson, const string& error, websocketpp::http::status_code::value code) {
		if (aJson.find("callback_id") != aJson.end()) {
			aJson["code"] = code;
			if (code != websocketpp::http::status_code::ok) {
				aJson["error"] = error;
			}

			if (aJson["data"] == nullptr) {
				aJson.erase("data");
			}

			sendPlain(aJson.dump());
		}
	}

	bool WebSocket::auth(ApiRequest& aRequest, const WebSocketPtr& aPointer) {
		auto h = aRequest.getSession()->getModule(aRequest.getApiModule());
		if (!h) {
			aRequest.setResponseError("Invalid API module");
			return false;
		}

		if (aRequest.getApiVersion() != h->getVersion()) {
			aRequest.setResponseError("Invalid API version");
			return false;
		}

		handler = h;
		handler->setSocket(aPointer);
		session = aRequest.getSession();
		return true;
	}

	void WebSocket::sendPlain(const string& aMsg) {
		//dcdebug("WebSocket::send: %s\n", aMsg.c_str());
		try {
			if (secure) {
				tlsServer->send(hdl, aMsg, websocketpp::frame::opcode::text);
			} else {
				plainServer->send(hdl, aMsg, websocketpp::frame::opcode::text);
			}

		} catch (const std::exception& e) {
			dcdebug("WebSocket::send failed: %s", e.what());
		}
	}

	api_return WebSocket::handle(ApiRequest& aRequest) noexcept {
		return handler->handleRequest(aRequest);
	}

	void WebSocket::close(websocketpp::close::status::value aCode, const string& aMsg) {
		try {
			if (secure) {
				tlsServer->close(hdl, aCode, aMsg);
			}
			else {
				plainServer->close(hdl, aCode, aMsg);
			}
		} catch (const std::exception& e) {
			dcdebug("WebSocket::close failed: %s", e.what());
		}
	}
}