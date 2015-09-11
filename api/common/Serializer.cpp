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

//#include <client/AdcHub.h>
#include <client/AirUtil.h>
#include <client/Bundle.h>
#include <client/Client.h>
#include <client/ClientManager.h>
#include <client/ChatMessage.h>
#include <client/DirectoryListing.h>
#include <client/OnlineUser.h>
#include <client/QueueItem.h>
#include <client/QueueManager.h>
#include <client/SearchManager.h>

namespace webserver {
	json Serializer::serializeUser(const UserPtr& aUser) noexcept {
		return{
			{ "cid", aUser->getCID().toBase32() },
			{ "nicks", Util::listToString(ClientManager::getInstance()->getHubNames(aUser->getCID())) }
		};
	}

	json Serializer::serializeHintedUser(const HintedUser& aUser) noexcept {
		return {
			{ "cid", aUser.user->getCID().toBase32() },
			{ "nicks", ClientManager::getInstance()->getFormatedNicks(aUser) },
			{ "hub_url", aUser.hint },
			{ "hub_names", ClientManager::getInstance()->getFormatedHubNames(aUser) },
		};
	}

	/*json Serializer::serializeChatMessage(const ChatMessage& aMessage) noexcept {
		return {
			{ "text", aMessage.format() },
			{ "from", serializeOnlineUser(aMessage.from) },
			{ "to", serializeOnlineUser(aMessage.to) },
			{ "reply_to", serializeOnlineUser(aMessage.replyTo) },
			{ "time", aMessage.timestamp }
		};
	}

	json Serializer::serializeOnlineUser(const OnlineUserPtr& aUser) noexcept {
		return {
			{ "cid", aUser->getUser()->getCID().toBase32() },
			{ "nick", aUser->getIdentity().getNick() },
			{ "hub_url", aUser->getHubUrl() },
		};
	}

	json Serializer::serializeClient(const Client* aClient) noexcept {
		return {
			{ "name", aClient->getHubName() },
			{ "hub_url", aClient->getHubUrl() },
			{ "description", aClient->getHubDescription() },
			//{ "hub_url", aClient-> },
		};
	}*/

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
}