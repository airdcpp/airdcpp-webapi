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

#include <api/SessionApi.h>

#include <web-server/WebSocket.h>
#include <web-server/WebServerManager.h>
#include <web-server/WebUserManager.h>

namespace webserver {
	SessionApi::SessionApi() {
	}

	websocketpp::http::status_code::value SessionApi::handleLogin(ApiRequest& aRequest, bool aIsSecure, const WebSocketPtr& aSocket) throw(exception) {
		decltype(auto) requestJson = aRequest.getRequestBody();
		auto session = WebUserManager::getInstance()->authenticate(requestJson["username"], requestJson["password"], aIsSecure);

		if (!session) {
			aRequest.setResponseErrorStr("Invalid username or password");
			return websocketpp::http::status_code::unauthorized;
		}

		json retJson = {
			{ "token", session->getToken() },
			{ "user", session->getUser()->getUserName() }
		};

		if (aSocket) {
			session->setSocket(aSocket);
			aSocket->setSession(session);
		}

		aRequest.setResponseBody(retJson);
		return websocketpp::http::status_code::ok;
	}

	api_return SessionApi::handleLogout(ApiRequest& aRequest) throw(exception) {
		if (!aRequest.getSession()) {
			aRequest.setResponseErrorStr("Not authorized");
			return websocketpp::http::status_code::unauthorized;
		}

		WebUserManager::getInstance()->logout(aRequest.getSession());
		WebServerManager::getInstance()->logout(aRequest.getSession()->getToken());

		return websocketpp::http::status_code::ok;
	}

	api_return SessionApi::handleSocketConnect(ApiRequest& aRequest, bool aIsSecure, const WebSocketPtr& aSocket) throw(exception) {
		std::string sessionToken = aRequest.getRequestBody()["authorization"];

		SessionPtr session = WebUserManager::getInstance()->getSession(sessionToken);
		if (!session) {
			aRequest.setResponseErrorStr("Invalid session token");
			return websocketpp::http::status_code::bad_request;
		}

		if (session->isSecure() != aIsSecure) {
			aRequest.setResponseErrorStr("Invalid protocol");
			return websocketpp::http::status_code::bad_request;
		}

		session->setSocket(aSocket);
		aSocket->setSession(session);

		return websocketpp::http::status_code::ok;
	}
}