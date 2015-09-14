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

#include <api/LogApi.h>

#include <api/common/Serializer.h>

#include <client/LogManager.h>

namespace webserver {
	LogApi::LogApi() {
		LogManager::getInstance()->addListener(this);

		subscriptions["log_message"];

		METHOD_HANDLER("messages", ApiRequest::METHOD_GET, (NUM_PARAM), false, LogApi::handleGetLog);
	}

	LogApi::~LogApi() {
		LogManager::getInstance()->removeListener(this);
	}

	json LogApi::serializeMessage(const LogMessage& aMessageData) noexcept {
		return{
			{ "id", aMessageData.id },
			{ "text", aMessageData.message },
			{ "time", aMessageData.time },
			{ "severity", static_cast<int>(aMessageData.severity) }
		};
	}

	api_return LogApi::handleGetLog(ApiRequest& aRequest)  throw(exception) {
		auto j = Serializer::serializeFromEnd(
			aRequest.getRangeParam(0),
			LogManager::getInstance()->getLastLogs(),
			serializeMessage);

		aRequest.setResponseBody(j);
		return websocketpp::http::status_code::ok;
	}

	void LogApi::on(LogManagerListener::Message, const LogMessage& aMessageData) noexcept {
		if (!subscriptions["log_message"])
			return;

		send("log_message", serializeMessage(aMessageData));
	}
}