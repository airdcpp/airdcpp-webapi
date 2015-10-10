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

#include <api/HubInfo.h>
#include <api/ApiModule.h>
#include <api/common/Serializer.h>

#include <web-server/JsonUtil.h>

namespace webserver {
	StringList HubInfo::subscriptionList = {
		"hub_updated",
		"hub_chat_message",
		"hub_status_message"
	};

	HubInfo::HubInfo(ParentType* aParentModule, const ClientPtr& aClient) :
		SubApiModule(aParentModule, Util::toString(aClient->getClientId()), subscriptionList), client(aClient) {

		client->addListener(this);

		METHOD_HANDLER("messages", ApiRequest::METHOD_GET, (NUM_PARAM), false, HubInfo::handleGetMessages);
		METHOD_HANDLER("message", ApiRequest::METHOD_POST, (), true, HubInfo::handlePostMessage);

		METHOD_HANDLER("read", ApiRequest::METHOD_POST, (), false, HubInfo::handleSetRead);
	}

	HubInfo::~HubInfo() {
		client->removeListener(this);
	}

	json HubInfo::serializeConnectState(const ClientPtr& aClient) noexcept {
		switch (aClient->getConnectState()) {
			case Client::STATE_CONNECTING:
			case Client::STATE_PROTOCOL:
			case Client::STATE_IDENTIFY:  return "connecting";
			case Client::STATE_VERIFY:  return "password";
			case Client::STATE_NORMAL: return "connected";
			case Client::STATE_DISCONNECTED: {
				if (!aClient->getRedirectUrl().empty()) {
					return "redirect";
				}

				return "disconnected";
			}
		}

		dcassert(0);
		return nullptr;
	}

	api_return HubInfo::handleSetRead(ApiRequest& aRequest) throw(exception) {
		client->setRead();
		return websocketpp::http::status_code::ok;
	}

	api_return HubInfo::handleGetMessages(ApiRequest& aRequest) throw(exception) {
		auto j = Serializer::serializeFromEnd(
			aRequest.getRangeParam(0),
			client->getCache().getMessages(),
			Serializer::serializeMessage);

		aRequest.setResponseBody(j);
		return websocketpp::http::status_code::ok;
	}

	api_return HubInfo::handlePostMessage(ApiRequest& aRequest) throw(exception) {
		decltype(auto) requestJson = aRequest.getRequestBody();

		auto message = JsonUtil::getField<string>("message", requestJson, false);
		auto thirdPerson = JsonUtil::getOptionalField<bool>("third_person", requestJson);

		string error_;
		if (!client->hubMessage(message, error_, thirdPerson ? *thirdPerson : false)) {
			aRequest.setResponseErrorStr(error_);
			return websocketpp::http::status_code::internal_server_error;
		}

		return websocketpp::http::status_code::ok;
	}

	void HubInfo::on(ChatMessage, const Client*, const ChatMessagePtr& aMessage) noexcept {
		if (!aMessage->getRead()) {
			sendUnread();
		}

		if (!subscriptionActive("hub_chat_message")) {
			return;
		}

		send("hub_chat_message", Serializer::serializeChatMessage(aMessage));
	}

	void HubInfo::on(StatusMessage, const Client*, const LogMessagePtr& aMessage, int aFlags) noexcept {
		if (!subscriptionActive("hub_status_message")) {
			return;
		}

		send("hub_status_message", Serializer::serializeLogMessage(aMessage));
	}

	void HubInfo::on(Disconnecting, const Client*) noexcept {

	}

	void HubInfo::on(Redirected, const string&, const ClientPtr& aNewClient) noexcept {

	}

	void HubInfo::on(MessagesRead) noexcept {
		sendUnread();
	}

	void HubInfo::sendUnread() noexcept {
		onHubUpdated({
			{ "unread_count", client->getCache().countUnread() }
		});
	}

	void HubInfo::onHubUpdated(const json& aData) noexcept {
		if (!subscriptionActive("hub_updated")) {
			return;
		}

		send("hub_updated", aData);
	}
}