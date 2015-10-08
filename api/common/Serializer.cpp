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

#include <api/common/Serializer.h>
#include <api/common/Format.h>

//#include <airdcpp/AdcHub.h>
#include <airdcpp/AirUtil.h>
#include <airdcpp/Bundle.h>
#include <airdcpp/Client.h>
#include <airdcpp/ClientManager.h>
#include <airdcpp/Message.h>
#include <airdcpp/DirectoryListing.h>
#include <airdcpp/GeoManager.h>
#include <airdcpp/OnlineUser.h>
#include <airdcpp/QueueItem.h>
#include <airdcpp/QueueManager.h>
#include <airdcpp/SearchManager.h>

namespace webserver {
	StringSet Serializer::getUserFlags(const UserPtr& aUser) noexcept {
		StringSet ret;
		if (aUser->isSet(User::BOT)) {
			ret.insert("bot");
		}

		if (aUser->isSet(User::FAVORITE)) {
			ret.insert("favorite");
		}

		if (aUser->isSet(User::IGNORED)) {
			ret.insert("ignored");
		}

		if (aUser == ClientManager::getInstance()->getMe()) {
			ret.insert("me");
		}

		if (aUser->isSet(User::NMDC)) {
			ret.insert("nmdc");
		}

		if (!aUser->isOnline()) {
			ret.insert("offline");
		}

		return ret;
	}

	json Serializer::serializeUser(const UserPtr& aUser) noexcept {
		return{
			{ "cid", aUser->getCID().toBase32() },
			{ "nicks", Util::listToString(ClientManager::getInstance()->getHubNames(aUser->getCID())) },
			{ "flags", getUserFlags(aUser) }
		};
	}

	json Serializer::serializeHintedUser(const HintedUser& aUser) noexcept {
		auto flags = getUserFlags(aUser);
		auto user = ClientManager::getInstance()->getOnlineUser(aUser);
		if (aUser.user->isOnline()) {
			auto user = ClientManager::getInstance()->getOnlineUser(aUser);
			if (user) {
				if (user->getIdentity().isAway()) {
					flags.insert("away");
				}

				if (user->getIdentity().isOp()) {
					flags.insert("op");
				}
			}
		}

		return {
			{ "cid", aUser.user->getCID().toBase32() },
			{ "nicks", ClientManager::getInstance()->getFormatedNicks(aUser) },
			{ "hub_url", aUser.hint },
			{ "hub_names", ClientManager::getInstance()->getFormatedHubNames(aUser) },
			{ "flags", flags }
		};
	}

	json Serializer::serializeMessage(const Message& aMessage) noexcept {
		if (aMessage.type == Message::TYPE_CHAT) {
			return{
				{ "chat_message", serializeChatMessage(aMessage.chatMessage) }
			};
		}

		return{
			{ "log_message", serializeLogMessage(aMessage.logMessage) }
		};
	}

	json Serializer::serializeChatMessage(const ChatMessagePtr& aMessage) noexcept {
		return {
			{ "id", aMessage->getId()},
			{ "text", aMessage->getText() },
			{ "from", serializeOnlineUser(aMessage->getFrom()) },
			{ "to", serializeOnlineUser(aMessage->getTo()) },
			{ "reply_to", serializeOnlineUser(aMessage->getReplyTo()) },
			{ "time", aMessage->getTime() },
			{ "is_read", aMessage->getRead() }
		};
	}

	json Serializer::serializeLogMessage(const LogMessagePtr& aMessageData) noexcept {
		return{
			{ "id", aMessageData->getId() },
			{ "text", aMessageData->getText() },
			{ "time", aMessageData->getTime() },
			{ "severity", static_cast<int>(aMessageData->getTime()) }
		};
	}

	json Serializer::serializeOnlineUser(const OnlineUserPtr& aUser) noexcept {
		return {
			{ "cid", aUser->getUser()->getCID().toBase32() },
			{ "nick", aUser->getIdentity().getNick() },
			{ "hub_url", aUser->getHubUrl() },
			{ "hub_name", aUser->getClient().getHubName() }
		};
	}

	json Serializer::serializeClient(const Client* aClient) noexcept {
		return {
			{ "name", aClient->getHubName() },
			{ "hub_url", aClient->getHubUrl() },
			{ "description", aClient->getHubDescription() }
		};
	}

	std::string typeNameToString(const string& aName) {
		switch (aName[0]) {
			case '1': return "audio";
			case '2': return "compressed";
			case '3': return "document";
			case '4': return "executable";
			case '5': return "picture";
			case '6': return "video";
			default: return "other";
		}
	}

	json Serializer::serializeFileType(const string& aPath) noexcept {
		auto ext = Format::formatFileType(aPath);
		auto typeName = SearchManager::getInstance()->getNameByExtension(ext, true);

		return{
			{ "type", typeNameToString(typeName) },
			{ "str", ext }
		};
	}

	json Serializer::serializeFolderType(size_t aFiles, size_t aDirectories) noexcept {
		return{
			{ "type", "directory" },
			{ "str", Format::formatFolderContent(aFiles, aDirectories) },
			{ "files", aFiles },
			{ "directories", aDirectories },
		};
	}

	json Serializer::serializeIp(const string& aIP) noexcept {
		return serializeIp(aIP, GeoManager::getInstance()->getCountry(aIP));
	}

	json Serializer::serializeIp(const string& aIP, const string& aCountryCode) noexcept {
		return{
			{ "str", Format::formatIp(aIP, aCountryCode) },
			{ "country_id", aCountryCode },
			{ "ip", aIP }
		};
	}
}