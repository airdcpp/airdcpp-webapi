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

#include <api/common/Deserializer.h>
#include <api/common/Serializer.h>

#include <airdcpp/ScopedFunctor.h>

namespace webserver {
	PrivateChatApi::PrivateChatApi(Session* aSession) : ApiModule(aSession) {

		MessageManager::getInstance()->addListener(this);

		subscriptions["chat_session_created"];
		subscriptions["chat_session_removed"];
		subscriptions["chat_session_updated"];

		METHOD_HANDLER("sessions", ApiRequest::METHOD_GET, (), false, PrivateChatApi::handleGetThreads);

		METHOD_HANDLER("session", ApiRequest::METHOD_DELETE, (CID_PARAM), false, PrivateChatApi::handleDeleteChat);
		METHOD_HANDLER("session", ApiRequest::METHOD_POST, (), true, PrivateChatApi::handlePostChat);

		MODULE_HANDLER("session", CID_PARAM, PrivateChatApi::handleChat);

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
			aRequest.setResponseErrorStr("Chat session not found");
			return websocketpp::http::status_code::not_found;
		}

		aRequest.popParam();
		return chat->handleRequest(aRequest);
	}

	api_return PrivateChatApi::handlePostChat(ApiRequest& aRequest) throw(exception) {
		auto c = MessageManager::getInstance()->addChat(Deserializer::deserializeHintedUser(aRequest.getRequestBody()), false);
		if (!c) {
			aRequest.setResponseErrorStr("Chat session exists");
			return websocketpp::http::status_code::bad_request;
		}

		return websocketpp::http::status_code::ok;
	}

	api_return PrivateChatApi::handleDeleteChat(ApiRequest& aRequest) throw(exception) {
		auto chat = findChat(aRequest.getStringParam(0));
		if (!chat) {
			aRequest.setResponseErrorStr("Chat session not found");
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
				retJson.push_back(serializeChat(c->getChat()));
			}
		}

		aRequest.setResponseBody(retJson);
		return websocketpp::http::status_code::ok;
	}

	void PrivateChatApi::on(MessageManagerListener::ChatRemoved, const PrivateChatPtr& aChat) noexcept {
		{
			WLock l(cs);
			chats.erase(aChat->getUser()->getCID());
		}

		aChat->removeListener(this);
		if (!subscriptions["chat_session_removed"]) {
			return;
		}

		send("chat_session_removed", {
			{ "cid", aChat->getUser()->getCID().toBase32() }
		});
	}

	void PrivateChatApi::on(MessageManagerListener::ChatCreated, const PrivateChatPtr& aChat, bool aReceivedMessage) noexcept {
		auto chatInfo = make_shared<PrivateChatInfo>(session, aChat);

		{
			WLock l(cs);
			chats.emplace(aChat->getUser()->getCID(), chatInfo);
		}

		aChat->addListener(this);

		if (!subscriptions["chat_session_created"]) {
			return;
		}

		send("chat_session_created", serializeChat(aChat));
	}

	json PrivateChatApi::serializeChat(const PrivateChatPtr& aChat) noexcept {
		return {
			{ "user", Serializer::serializeHintedUser(aChat->getHintedUser()) },
			{ "ccpm_state", serializeCCPMState(aChat->getCCPMState()) },
			{ "unread_count", aChat->getCache().countUnread() }
		};
	}

	json PrivateChatApi::serializeCCPMState(uint8_t aState) noexcept {
		return {
			{ "id", aState },
			{ "str", PrivateChat::ccpmStateToString(aState) }
		};
	}

	void PrivateChatApi::on(PrivateChatListener::Close, PrivateChat* aChat) noexcept {

	}

	void PrivateChatApi::on(PrivateChatListener::UserUpdated, PrivateChat* aChat) noexcept {
		onSessionUpdated(aChat, {
			{ "user", Serializer::serializeHintedUser(aChat->getHintedUser()) }
		});
	}

	void PrivateChatApi::on(PrivateChatListener::PMStatus, PrivateChat* aChat, uint8_t aSeverity) noexcept {

	}

	void PrivateChatApi::on(PrivateChatListener::CCPMStatusUpdated, PrivateChat* aChat) noexcept {
		onSessionUpdated(aChat, {
			{ serializeCCPMState(aChat->getCCPMState()) }
		});
	}

	void PrivateChatApi::on(PrivateMessage, PrivateChat* aChat, const ChatMessagePtr& aMessage) noexcept {
		if (aMessage->getRead()) {
			return;
		}

		sendUnread(aChat);
	}

	void PrivateChatApi::on(PrivateChatListener::MessagesRead, PrivateChat* aChat) noexcept {
		sendUnread(aChat);
	}

	void PrivateChatApi::sendUnread(PrivateChat* aChat) noexcept {
		onSessionUpdated(aChat, {
			{ "unread_count", aChat->getCache().countUnread() }
		});
	}

	void PrivateChatApi::onSessionUpdated(const PrivateChat* aChat, const json& aData) noexcept {
		if (!subscriptions["chat_session_updated"]) {
			return;
		}

		send("chat_session_updated", {
			{ "id", aChat->getUser()->getCID().toBase32() },
			{ "properties", aData }
		});
	}

	PrivateChatInfo::List PrivateChatApi::getUsers() {
		PrivateChatInfo::List ret;
		boost::range::copy(chats | map_values, back_inserter(ret));
		return ret;
	}
}