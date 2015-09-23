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
		string& error_, bool aIsSecure) noexcept {

		dcdebug("Received socket request: %s\n", aRequestBody.c_str());
		bool authenticated = aSocket->getSession() != nullptr;

		try {
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

			return handleRequest(apiRequest, aIsSecure, aSocket);
		} catch (const std::exception& e) {
			error_ = "Parsing failed: " + string(e.what());
			return websocketpp::http::status_code::bad_request;
		}
	}

	websocketpp::http::status_code::value ApiRouter::handleHttpRequest(const string& aRequestPath,
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

			return handleRequest(apiRequest, aIsSecure, nullptr);
		} catch (const std::exception& e) {
			error_ = "Parsing failed: " + string(e.what());
			return websocketpp::http::status_code::bad_request;
		}

		return websocketpp::http::status_code::ok;
	}

	api_return ApiRouter::handleRequest(ApiRequest& aRequest, bool aIsSecure, const WebSocketPtr& aSocket) noexcept {
		// Special case because we may not have the session yet
		if (aRequest.getApiModule() == "session") {
			return handleSessionRequest(aRequest, aIsSecure, aSocket);
		}

		// Require auth for all other modules
		if (!aRequest.getSession()) {
			aRequest.setResponseError("Not authorized");
			return websocketpp::http::status_code::unauthorized;
		}

		// Require using the same protocol that was used for logging in
		if (aRequest.getSession()->isSecure() != aIsSecure) {
			aRequest.setResponseError("Protocol mismatch");
			return websocketpp::http::status_code::not_acceptable;
		}

		int code;
		try {
			code = aRequest.getSession()->handleRequest(aRequest);
		} catch (const ArgumentException& e) {
			aRequest.setResponseError(e.what());
			code = CODE_UNPROCESSABLE_ENTITY;
		} catch (const std::exception& e) {
			aRequest.setResponseError(e.what());
			code = websocketpp::http::status_code::bad_request;
		}

		return static_cast<api_return>(code);
	}

	api_return ApiRouter::handleSessionRequest(ApiRequest& aRequest, bool aIsSecure, const WebSocketPtr& aSocket) throw(exception) {
		if (aRequest.getApiVersion() != 0) {
			aRequest.setResponseError("Invalid API version");
			return websocketpp::http::status_code::precondition_failed;
		}

		if (aRequest.getApiModuleSection() == "auth") {
			if (aRequest.getMethod() == ApiRequest::METHOD_POST) {
				return sessionApi.handleLogin(aRequest, aIsSecure, aSocket);
			} else if (aRequest.getMethod() == ApiRequest::METHOD_DELETE) {
				return sessionApi.handleLogout(aRequest);
			}
		} else if (aRequest.getApiModuleSection() == "socket") {
			return sessionApi.handleSocketConnect(aRequest, aIsSecure, aSocket);
		}

		aRequest.setResponseError("Invalid command");
		return websocketpp::http::status_code::bad_request;
	}
}