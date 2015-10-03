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

#ifndef DCPLUSPLUS_DCPP_PRIVATEMESSAGEAPI_H
#define DCPLUSPLUS_DCPP_PRIVATEMESSAGEAPI_H

#include <web-server/stdinc.h>

#include <api/ApiModule.h>
#include <api/PrivateChatInfo.h>

#include <airdcpp/typedefs.h>
#include <airdcpp/MessageManager.h>

namespace webserver {
	class PrivateChatApi : public ApiModule, private MessageManagerListener {
	public:
		PrivateChatApi(Session* aSession);
		~PrivateChatApi();

		void onSocketRemoved() noexcept;

		int getVersion() const noexcept {
			return 0;
		}
	private:
		api_return handleChat(ApiRequest& aRequest) throw(exception);
		api_return handlePostChat(ApiRequest& aRequest) throw(exception);
		api_return handleDeleteChat(ApiRequest& aRequest) throw(exception);

		api_return handleGetThreads(ApiRequest& aRequest) throw(exception);

		PrivateChatInfoPtr findChat(const string& aCidStr) throw(exception);

		PrivateChatInfo::List getUsers();

		PrivateChatInfo::Map chats;

		void on(MessageManagerListener::ChatCreated, const PrivateChatPtr& aChat, bool aReceivedMessage) noexcept;
		void on(MessageManagerListener::ChatRemoved, const PrivateChatPtr& aChat) noexcept;

		mutable SharedMutex cs;
	};
}

#endif