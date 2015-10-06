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

#include <api/PrivateChatInfo.h>
#include <api/common/Serializer.h>
#include <web-server/JsonUtil.h>

#include <airdcpp/PrivateChat.h>

namespace webserver {
	PrivateChatInfo::PrivateChatInfo(Session* aSession, const PrivateChatPtr& aChat) : 
		SubApiModule(aSession, aChat->getUser()->getCID().toBase32()), chat(aChat) {

		chat->addListener(this);

		subscriptions["private_chat_message"];
		subscriptions["private_chat_status"];

		METHOD_HANDLER("messages", ApiRequest::METHOD_GET, (NUM_PARAM), false, PrivateChatInfo::handleGetMessages);
		METHOD_HANDLER("message", ApiRequest::METHOD_POST, (), true, PrivateChatInfo::handlePostMessage);

		METHOD_HANDLER("read", ApiRequest::METHOD_POST, (), false, PrivateChatInfo::handleSetRead);
	}

	PrivateChatInfo::~PrivateChatInfo() {
		chat->removeListener(this);
	}

	api_return PrivateChatInfo::handleSetRead(ApiRequest& aRequest) throw(exception) {
		chat->setRead();
		return websocketpp::http::status_code::ok;
	}

	api_return PrivateChatInfo::handleGetMessages(ApiRequest& aRequest) throw(exception) {
		auto j = Serializer::serializeFromEnd(
			aRequest.getRangeParam(0),
			chat->getCache().getMessages(),
			Serializer::serializeMessage);

		aRequest.setResponseBody(j);
		return websocketpp::http::status_code::ok;
	}

	api_return PrivateChatInfo::handlePostMessage(ApiRequest& aRequest) throw(exception) {
		decltype(auto) requestJson = aRequest.getRequestBody();

		auto message = JsonUtil::getField<string>("message", requestJson, false);
		auto thirdPerson = JsonUtil::getOptionalField<bool>("third_person", requestJson);

		string error;
		if (!chat->sendPrivateMessage(message, error, thirdPerson ? *thirdPerson : false)) {
			aRequest.setResponseErrorStr(error);
			return websocketpp::http::status_code::internal_server_error;
		}

		return websocketpp::http::status_code::ok;
	}

	void PrivateChatInfo::on(PrivateChatListener::PrivateMessage, PrivateChat*, const ChatMessagePtr& aMessage) noexcept {
		if (!subscriptions["private_chat_message"]) {
			return;
		}

		send("private_chat_message", Serializer::serializeChatMessage(aMessage));
	}

	void PrivateChatInfo::on(PrivateChatListener::StatusMessage, PrivateChat*, const string& aMessage, uint8_t aSeverity) noexcept {
		if (!subscriptions["private_chat_status"]) {
			return;
		}

		send("private_chat_status", {
			{ "text", aMessage },
			{ "severity", aSeverity }
		});
	}
}