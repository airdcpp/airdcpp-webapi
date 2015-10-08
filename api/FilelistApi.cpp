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

#include <api/FilelistApi.h>
//#include <api/SearchUtils.h>

#include <api/common/Deserializer.h>

namespace webserver {
	FilelistApi::FilelistApi(Session* aSession) : ApiModule(aSession) {

		DirectoryListingManager::getInstance()->addListener(this);

		createSubscription("list_created");
		createSubscription("list_removed");

		//MODULE_HANDLER("filelist", TOKEN_PARAM, FilelistApi::handleFilelist);
	}

	FilelistApi::~FilelistApi() {
		DirectoryListingManager::getInstance()->removeListener(this);
	}
}