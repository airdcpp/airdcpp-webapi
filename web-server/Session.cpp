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
#include <web-server/Session.h>
#include <web-server/ApiRequest.h>

#include <api/LogApi.h>
#include <api/MainApi.h>
#include <api/QueueApi.h>
#include <api/SearchApi.h>

#include <client/TimerManager.h>

namespace webserver {
#define ADD_MODULE(name, type) (apiHandlers.emplace(name, LazyModuleWrapper([] { return make_unique<type>(); })))

	Session::Session(WebUserPtr& aUser, const string& aToken, bool aIsSecure) : 
		user(aUser), token(aToken), started(GET_TICK()), lastActivity(lastActivity), secure(aIsSecure) {

		ADD_MODULE("log", LogApi);
		ADD_MODULE("main", MainApi);
		ADD_MODULE("queue", QueueApi);
		ADD_MODULE("search", SearchApi);
	}

	Session::~Session() {
		//apiHandlers.clear();
	}

	ApiModule* Session::getModule(const string& aModule) {
		auto h = apiHandlers.find(aModule);
		return h != apiHandlers.end() ? h->second.get() : nullptr;
	}

	websocketpp::http::status_code::value Session::handleRequest(ApiRequest& aRequest) throw(exception) {
		auto h = apiHandlers.find(aRequest.getApiModule());
		if (h != apiHandlers.end()) {
			if (aRequest.getApiVersion() != h->second->getVersion()) {
				aRequest.setResponseError("Invalid API version");
				return websocketpp::http::status_code::precondition_failed;
			}

			return h->second->handleRequest(aRequest);
		}

		aRequest.setResponseError("Section not found");
		return websocketpp::http::status_code::not_found;
	}

	void Session::setSocket(const WebSocketPtr& aSocket) noexcept {
		for (auto& h : apiHandlers | map_values) {
			h->setSocket(aSocket);
		}
	}
}