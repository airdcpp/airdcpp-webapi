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

#ifndef DCPLUSPLUS_DCPP_APIMODULE_H
#define DCPLUSPLUS_DCPP_APIMODULE_H

#include <web-server/stdinc.h>
#include <web-server/ApiRequest.h>

//#include <airdcpp/typedefs.h>
//#include <airdcpp/GetSet.h>
#include <airdcpp/StringMatch.h>

namespace webserver {
	class WebSocket;
	class ApiModule {
	public:
#define NUM_PARAM (StringMatch::getSearch(R"(\d+)", StringMatch::REGEX))
#define TOKEN_PARAM NUM_PARAM
#define TTH_PARAM (StringMatch::getSearch(R"([0-9A-Z]{39})", StringMatch::REGEX))
#define CID_PARAM TTH_PARAM
#define STR_PARAM (StringMatch::getSearch(R"([a-z_]+)", StringMatch::REGEX))
#define EXACT_PARAM(pattern) (StringMatch::getSearch(pattern, StringMatch::EXACT))

#define BRACED_INIT_LIST(...) {__VA_ARGS__}
#define METHOD_HANDLER(section, method, params, requireJson, func) (requestHandlers[section].push_back(ApiModule::RequestHandler(method, requireJson, BRACED_INIT_LIST params, std::bind(&func, this, placeholders::_1))))

		ApiModule();
		virtual ~ApiModule();

		typedef vector<StringMatch> ParamList;
		struct RequestHandler {
			typedef std::vector<RequestHandler> List;
			typedef std::function<api_return(ApiRequest& aRequest)> HandlerFunction;
			RequestHandler(ApiRequest::Method aMethod, bool aRequireJson, ParamList&& aParams, HandlerFunction aFunction) :
				method(aMethod), requireJson(aRequireJson), params(move(aParams)), f(aFunction) { }

			const ApiRequest::Method method;
			const bool requireJson;
			const ParamList params;
			const HandlerFunction f;

			bool matchParams(const ApiRequest::RequestParamList& aParams) const noexcept;
		};

		typedef std::map<std::string , bool> SubscriptionMap;
		typedef std::map<std::string, RequestHandler::List> RequestHandlerMap;

		api_return handleRequest(ApiRequest& aRequest) throw(exception);

		void setSocket(const WebSocketPtr& aSocket) noexcept;

		virtual int getVersion() const noexcept = 0;
		virtual void onSocketRemoved() noexcept { }

		ApiModule(ApiModule&) = delete;
		ApiModule& operator=(ApiModule&) = delete;

		bool send(const json& aJson);
		bool send(const string& aSubscription, const json& aJson);
	protected:
		RequestHandlerMap requestHandlers;
		SubscriptionMap subscriptions;

		api_return handleSubscribe(ApiRequest& aRequest) throw(exception);
		api_return handleUnsubscribe(ApiRequest& aRequest) throw(exception);

		WebSocketPtr socket = nullptr;
	};

	typedef std::unique_ptr<ApiModule> HandlerPtr;
}

#endif