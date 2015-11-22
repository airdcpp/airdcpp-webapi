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

#ifndef DCPLUSPLUS_DCPP_SHAREAPI_H
#define DCPLUSPLUS_DCPP_SHAREAPI_H

#include <web-server/stdinc.h>

#include <api/ApiModule.h>

#include <airdcpp/typedefs.h>
#include <airdcpp/ShareManagerListener.h>

namespace webserver {
	class ShareApi : public ApiModule, private ShareManagerListener {
	public:
		ShareApi(Session* aSession);
		~ShareApi();

		int getVersion() const noexcept {
			return 0;
		}
	private:
		static json serializeShareProfile(const ShareProfilePtr& aProfile) noexcept;
		//static json serializeShareRoot(const ShareDirInfo& aRoot) noexcept;

		api_return handleGetProfiles(ApiRequest& aRequest);
		api_return handleAddProfile(ApiRequest& aRequest);
		api_return handleUpdateProfile(ApiRequest& aRequest);
		api_return handleRemoveProfile(ApiRequest& aRequest);

		api_return handleGetRoots(ApiRequest& aRequest);
		api_return handleAddRoot(ApiRequest& aRequest);
		api_return handleUpdateRoot(ApiRequest& aRequest);
		api_return handleRemoveRoot(ApiRequest& aRequest);

		void parseProfile(ShareProfilePtr& aProfile, const json& j);

		api_return handleGetStats(ApiRequest& aRequest);

		api_return handleGetGroupedRootPaths(ApiRequest& aRequest);
		api_return handleFindDupePaths(ApiRequest& aRequest);

		void on(ShareManagerListener::ProfileAdded, ProfileToken aProfile) noexcept;
		void on(ShareManagerListener::ProfileUpdated, ProfileToken aProfile) noexcept;
		void on(ShareManagerListener::ProfileRemoved, ProfileToken aProfile) noexcept;
	};
}

#endif