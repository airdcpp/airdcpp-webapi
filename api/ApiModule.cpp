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

#include <api/ApiModule.h>

namespace webserver {
	ApiModule::ApiModule() {
		METHOD_HANDLER("listener", ApiRequest::METHOD_POST, (STR_PARAM), false, ApiModule::handleSubscribe);
		METHOD_HANDLER("listener", ApiRequest::METHOD_DELETE, (STR_PARAM), false, ApiModule::handleUnsubscribe);
	}

	ApiModule::~ApiModule() {
		setSocket(nullptr);
	}

	void ApiModule::setSocket(const WebSocketPtr& aSocket) noexcept {
		if (!aSocket) {
			// Disable all subscriptions
			for (auto& s : subscriptions) {
				s.second = false;
			}

			onSocketRemoved();
		}

		socket = aSocket;
	}

	bool ApiModule::RequestHandler::matchParams(const ApiRequest::RequestParamList& aParams) const noexcept {
		if (aParams.size() != params.size()) {
			return false;
		}

		for (auto i = 0; i < aParams.size(); i++) {
			if (!params[i].match(aParams[i])) {
				return false;
			}
		}

		return true;
	}

	api_return ApiModule::handleRequest(ApiRequest& aRequest) throw(exception) {
		// Find section
		auto i = requestHandlers.find(aRequest.getApiModuleSection());
		if (i == requestHandlers.end()) {
			aRequest.setResponseError("Invalid API section");
			return websocketpp::http::status_code::bad_request;
		}

		const auto& sectionHandlers = i->second;

		// Find method handlers
		/*auto methodHandlers = sectionHandlers.equal_range(aRequest.getMethod());
		if (methodHandlers.first == methodHandlers.second) {
			aRequest.setResponseError("Method not supported for this API section");
			return websocketpp::http::status_code::bad_request;
		}*/

		bool hasParamMatch = false; // for better error reporting

		// Match parameters
		auto handler = boost::find_if(sectionHandlers, [&](const RequestHandler& aHandler) {
			auto matchesParams = aHandler.matchParams(aRequest.getParameters());
			if (!matchesParams) {
				return false;
			}

			if (aHandler.method == aRequest.getMethod()) {
				return true;
			}

			hasParamMatch = true;
			return false;
		});

		if (handler == sectionHandlers.end()) {
			if (hasParamMatch) {
				aRequest.setResponseError("Method not supported for this command");
			} else {
				aRequest.setResponseError("Invalid parameters for this API section");
			}

			return websocketpp::http::status_code::bad_request;
		}

		// Check JSON payload
		if (handler->requireJson && !aRequest.hasRequestBody()) {
			aRequest.setResponseError("JSON body required");
			return websocketpp::http::status_code::bad_request;
		}

		// Exact params could be removed from the request...

		return handler->f(aRequest);
	}

	api_return ApiModule::handleSubscribe(ApiRequest& aRequest) throw(exception) {
		if (!socket) {
			aRequest.setResponseError("Socket required");
			return websocketpp::http::status_code::precondition_required;
		}

		auto i = subscriptions.find(aRequest.getStringParam(0));
		if (i == subscriptions.end()) {
			aRequest.setResponseError("No such subscription: " + aRequest.getStringParam(0));
			return websocketpp::http::status_code::not_found;
		}

		i->second = true;
		return websocketpp::http::status_code::ok;
	}

	api_return ApiModule::handleUnsubscribe(ApiRequest& aRequest) throw(exception) {
		auto i = subscriptions.find(aRequest.getStringParam(0));
		if (i != subscriptions.end()) {
			i->second = false;
			return websocketpp::http::status_code::ok;
		}

		return websocketpp::http::status_code::not_found;
	}

	bool ApiModule::send(const json& aJson) {
		// Ensure that the socket won't be deleted while sending the message...
		auto s = socket;
		if (!s) {
			return false;
		}

		s->sendPlain(aJson.dump(4));
		return true;
	}

	bool ApiModule::send(const string& aSubscription, const json& aJson) {
		json j;
		j["event"] = aSubscription;
		j["data"] = aJson;

		return send(j);
	}
}