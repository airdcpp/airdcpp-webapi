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

#include <api/PrivateChatApi.h>
//#include <api/SearchUtils.h>

#include <api/common/Deserializer.h>
#include <api/common/Serializer.h>

#include <airdcpp/ScopedFunctor.h>

namespace webserver {
	PrivateChatApi::PrivateChatApi(Session* aSession) : ApiModule(aSession) {

		MessageManager::getInstance()->addListener(this);

		subscriptions["chat_created"];
		subscriptions["chat_removed"];
		subscriptions["chat_updated"];

		METHOD_HANDLER("chats", ApiRequest::METHOD_GET, (), false, PrivateChatApi::handleGetThreads);

		METHOD_HANDLER("chat", ApiRequest::METHOD_DELETE, (CID_PARAM), false, PrivateChatApi::handleDeleteChat);
		METHOD_HANDLER("chat", ApiRequest::METHOD_POST, (), true, PrivateChatApi::handlePostChat);

		MODULE_HANDLER("chat", CID_PARAM, PrivateChatApi::handleChat);

		auto rawChats = MessageManager::getInstance()->getChats();
		for (const auto& c : rawChats) {
			chats.emplace(c.first->getCID(), make_shared<PrivateChatInfo>(session, c.second));
		}
	}

	PrivateChatApi::~PrivateChatApi() {
		MessageManager::getInstance()->removeListener(this);
	}

	PrivateChatInfoPtr PrivateChatApi::findChat(const string& aCidStr) throw(exception) {
		RLock l(cs);
		auto i = chats.find(Deserializer::deserializeCID(aCidStr));
		if (i != chats.end()) {
			return i->second;
		}

		return nullptr;
	}

	api_return PrivateChatApi::handleChat(ApiRequest& aRequest) throw(exception) {
		auto chat = findChat(aRequest.getStringParam(0));
		if (!chat) {
			aRequest.setResponseErrorStr("Chat not found");
			return websocketpp::http::status_code::not_found;
		}

		aRequest.popParam();
		return chat->handleRequest(aRequest);
	}

	api_return PrivateChatApi::handlePostChat(ApiRequest& aRequest) throw(exception) {
		auto c = MessageManager::getInstance()->addChat(Deserializer::deserializeHintedUser(aRequest.getRequestBody()), false);
		if (!c) {
			aRequest.setResponseErrorStr("Chat exists");
			return websocketpp::http::status_code::bad_request;
		}

		return websocketpp::http::status_code::ok;
	}

	api_return PrivateChatApi::handleDeleteChat(ApiRequest& aRequest) throw(exception) {
		auto chat = findChat(aRequest.getStringParam(0));
		if (!chat) {
			aRequest.setResponseErrorStr("Chat not found");
			return websocketpp::http::status_code::not_found;
		}

		MessageManager::getInstance()->removeChat(chat->getChat()->getUser());
		return websocketpp::http::status_code::ok;
	}

	api_return PrivateChatApi::handleGetThreads(ApiRequest& aRequest) throw(exception) {
		json retJson;

		{
			RLock l(cs);
			for (const auto& c : chats | map_values) {
				retJson.push_back(c->serialize());
			}
		}

		aRequest.setResponseBody(retJson);
		return websocketpp::http::status_code::ok;
	}

	void PrivateChatApi::on(MessageManagerListener::ChatRemoved, const PrivateChatPtr& aChat) noexcept {
	
	}

	void PrivateChatApi::on(MessageManagerListener::ChatCreated, const PrivateChatPtr& aChat, bool aReceivedMessage) noexcept {
		if (!subscriptions["chat_created"]) {
			return;
		}

		auto chatInfo = make_shared<PrivateChatInfo>(session, aChat);

		{
			WLock l(cs);
			chats.emplace(aChat->getUser()->getCID(), chatInfo);
		}

		send("chat_created", chatInfo->serialize());
	}

	PrivateChatInfo::List PrivateChatApi::getUsers() {
		PrivateChatInfo::List ret;
		boost::range::copy(chats | map_values, back_inserter(ret));
		return ret;
	}
}