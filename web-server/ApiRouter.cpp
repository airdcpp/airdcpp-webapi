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
#include <web-server/ApiRouter.h>

#include <web-server/ApiRequest.h>
#include <web-server/WebUserManager.h>
#include <web-server/WebSocket.h>

#include <client/File.h>
#include <client/Util.h>
#include <client/StringTokenizer.h>

#include <sstream>

namespace webserver {
	using namespace dcpp;

	ApiRouter::ApiRouter() {
	}

	ApiRouter::~ApiRouter() {

	}

	api_return ApiRouter::handleSocketRequest(const string& aRequestBody, WebSocketPtr& aSocket, json& response_, 
		string& error_, bool aIsSecure) {

		dcdebug("Received socket request: %s\n", aRequestBody.c_str());
		bool authenticated = aSocket->getSession() != nullptr;

		json j = json::parse(aRequestBody);
		auto cb = j.find("callback_id");
		if (cb != j.end()) {
			response_["callback_id"] = cb.value();
		}

		response_["data"] = json();
		ApiRequest apiRequest(j["path"], j["method"], response_["data"], error_);
		apiRequest.parseSocketRequestJson(j);
		apiRequest.setSession(aSocket->getSession());

		if (!apiRequest.validate(false, error_)) {
			return websocketpp::http::status_code::bad_request;
		}

		// Special case because we may not have a session yet
		if (apiRequest.getApiModule() == "session") {
			return handleSessionRequest(apiRequest, aIsSecure, aSocket);
		}

		if (!authenticated) {
			error_ = "Not authorized";
			return websocketpp::http::status_code::unauthorized;
		}

		return aSocket->getSession()->handleRequest(apiRequest);
	}

	websocketpp::http::status_code::value ApiRouter::handleRequest(const string& aRequestPath,
		const SessionPtr& aSession, const string& aRequestBody, json& output_, string& error_,
		bool aIsSecure, const string& aRequestMethod) noexcept {

		dcdebug("Received HTTP request: %s\n", aRequestBody.c_str());
		try {
			ApiRequest apiRequest(aRequestPath, aRequestMethod, output_, error_);
			if (!apiRequest.validate(false, error_)) {
				return websocketpp::http::status_code::bad_request;
			}

			apiRequest.parseHttpRequestJson(aRequestBody);
			apiRequest.setSession(aSession);

			// Special case because we may not have the session yet
			if (apiRequest.getApiModule() == "session") {
				return handleSessionRequest(apiRequest, aIsSecure);
			}

			// Require auth for all other modules
			if (!aSession) {
				error_ = "Not authorized";
				return websocketpp::http::status_code::unauthorized;
			}

			// Require using the same protocol that was used for logging in
			if (aSession->isSecure() != aIsSecure) {
				error_ = "Protocol mismatch";
				return websocketpp::http::status_code::not_acceptable;
			}

			auto ret = aSession->handleRequest(apiRequest);
			if (ret != websocketpp::http::status_code::ok) {
				//output_ = apiRequest.get
			}
			return ret;
		} catch (const std::exception& e) {
			error_ = "Parsing failed: " + string(e.what());
			return websocketpp::http::status_code::bad_request;
		}

		return websocketpp::http::status_code::ok;
	}

	websocketpp::http::status_code::value ApiRouter::handleSessionRequest(ApiRequest& aRequest, bool aIsSecure, const WebSocketPtr& aSocket) throw(exception) {
		if (aRequest.getApiVersion() != 0) {
			aRequest.setResponseError("Invalid API version");
			return websocketpp::http::status_code::precondition_failed;
		}

		if (aRequest.getApiModuleSection() == "auth") {
			if (aRequest.getMethod() == ApiRequest::METHOD_POST) {
				return sessionApi.handleLogin(aRequest, aIsSecure, aSocket);
			} else if (aRequest.getMethod() == ApiRequest::METHOD_DELETE) {
				return sessionApi.handleLogout(aRequest, aSocket);
			}
		} else if (aRequest.getApiModuleSection() == "socket") {
			return sessionApi.handleSocketConnect(aRequest, aIsSecure, aSocket);
		}

		aRequest.setResponseError("Invalid command");
		return websocketpp::http::status_code::bad_request;
	}
}