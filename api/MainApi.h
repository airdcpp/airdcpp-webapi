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

#ifndef DCPLUSPLUS_DCPP_MAINSOCKET_H
#define DCPLUSPLUS_DCPP_MAINSOCKET_H

#include <web-server/stdinc.h>

#include <api/ApiModule.h>

#include <client/typedefs.h>

#include <client/ClientManagerListener.h>
#include <client/DirectoryListingManagerListener.h>
#include <client/LogManagerListener.h>
#include <client/MessageManager.h>
#include <client/QueueManagerListener.h>
#include <client/UpdateManager.h>
#include <client/TimerManager.h>

namespace webserver {
	class MainApi : public ApiModule, private QueueManagerListener,
		private DirectoryListingManagerListener, private ClientManagerListener, 
		private MessageManagerListener, private UpdateManagerListener, private TimerManagerListener {
	public:
		MainApi();
		~MainApi();

		int getVersion() const noexcept {
			return 0;
		}
		//websocketpp::http::status_code::value handleRequest(ApiRequest& aRequest, const SessionPtr& aSession, const json& aJson, std::string& output_) throw(exception);
	private:
		void on(TimerManagerListener::Second, uint64_t aTick) noexcept;

		// MessageManagerListener
		virtual void on(MessageManagerListener::PrivateMessage, const ChatMessage& aMessage) noexcept;

		// QueueManagerListener
		void on(QueueManagerListener::Finished, const QueueItemPtr& qi, const std::string& dir, const HintedUser& aUser, int64_t aSpeed) noexcept;
		void on(QueueManagerListener::BundleRemoved, const BundlePtr& aBundle) noexcept;

		// DirectoryListingManagerListener
		void on(DirectoryListingManagerListener::OpenListing, DirectoryListing* aList, const std::string& aDir, const std::string& aXML) noexcept;
		void on(DirectoryListingManagerListener::PromptAction, completionF aF, const std::string& aMessage) noexcept;

		void on(UpdateManagerListener::UpdateAvailable, const std::string& title, const std::string& message, const std::string& aVersionString, const std::string& infoUrl, bool autoUpdate, int build, const std::string& autoUpdateUrl) noexcept;
		void on(UpdateManagerListener::BadVersion, const std::string& message, const std::string& url, const std::string& update, int buildID, bool canAutoUpdate) noexcept;
		void on(UpdateManagerListener::UpdateComplete, const std::string& updater) noexcept;
		void on(UpdateManagerListener::UpdateFailed, const std::string& line) noexcept;

		void on(ClientManagerListener::ClientCreated, Client*) noexcept;
		json previousStats;
	};
}

#endif