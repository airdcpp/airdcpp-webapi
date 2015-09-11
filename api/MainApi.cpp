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

#include <api/MainApi.h>
#include <api/Serializer.h>

#include <client/ChatMessage.h>
#include <client/DirectoryListingManager.h>
#include <client/DownloadManager.h>
#include <client/MessageManager.h>
#include <client/QueueManager.h>
#include <client/SettingsManager.h>
#include <client/UpdateManager.h>

namespace webserver {
	MainApi::MainApi() {
		QueueManager::getInstance()->addListener(this);
		DirectoryListingManager::getInstance()->addListener(this);
		UpdateManager::getInstance()->addListener(this);
		ClientManager::getInstance()->addListener(this);
		MessageManager::getInstance()->addListener(this);
		TimerManager::getInstance()->addListener(this);

		subscriptions["statistics"];
		subscriptions["privatemessage_received"];
		subscriptions["tempitems"];
		subscriptions["filelists"];
		subscriptions["hub_created"];
		subscriptions["updates"];
	}

	MainApi::~MainApi() {
		QueueManager::getInstance()->removeListener(this);
		DirectoryListingManager::getInstance()->removeListener(this);
		UpdateManager::getInstance()->removeListener(this);
		ClientManager::getInstance()->removeListener(this);
		MessageManager::getInstance()->removeListener(this);
		TimerManager::getInstance()->removeListener(this);
	}

	void MainApi::on(TimerManagerListener::Second, uint64_t aTick) noexcept {
		if (!subscriptions["statistics"])
			return;

		json j = {
			{ "session_down", Socket::getTotalDown() },
			{ "session_up", Socket::getTotalUp() },
			{ "speed_down", DownloadManager::getInstance()->getLastDownSpeed() },
			{ "speed_up", DownloadManager::getInstance()->getLastUpSpeed() },
		};

		if (previousStats == j)
			return;

		previousStats = j;
		send("statistics", j);
	}

	void MainApi::on(ClientManagerListener::ClientCreated, Client* c) noexcept {
		if (!subscriptions["hubs"])
			return;

		//send("hub_created", Serializer::serializeClient(c));
	}

	// MessageManagerListener
	void MainApi::on(MessageManagerListener::PrivateMessage, const ChatMessage& aMessage) noexcept{
		if (!subscriptions["privatemessages"])
			return;

		//send("privatemessage_received", Serializer::serializeChatMessage(aMessage));
	}

	// QueueManagerListener
	void MainApi::on(QueueManagerListener::Finished, const QueueItemPtr& qi, const string& dir, const HintedUser& aUser, int64_t aSpeed) noexcept{
		if (!subscriptions["tempitems"])
			return;

	}
	void MainApi::on(QueueManagerListener::BundleRemoved, const BundlePtr& aBundle) noexcept{

	}

	void MainApi::on(DirectoryListingManagerListener::OpenListing, DirectoryListing* aList, const string& aDir, const string& aXML) noexcept{
		if (!subscriptions["filelists"])
			return;

		//send("open_listing", Serializer::serializeDirectoryListing(aList));
	}

	void MainApi::on(DirectoryListingManagerListener::PromptAction, completionF aF, const string& aMessage) noexcept{

	}

	void MainApi::on(UpdateManagerListener::UpdateAvailable, const string& title, const string& message, const string& aVersionString, const string& infoUrl, bool autoUpdate, int build, const string& autoUpdateUrl) noexcept{

	}
	void MainApi::on(UpdateManagerListener::BadVersion, const string& message, const string& url, const string& update, int buildID, bool canAutoUpdate) noexcept{

	}
	void MainApi::on(UpdateManagerListener::UpdateComplete, const string& updater) noexcept{

	}
	void MainApi::on(UpdateManagerListener::UpdateFailed, const string& line) noexcept{

	}
}