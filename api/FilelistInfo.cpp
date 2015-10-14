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

#include <api/FilelistInfo.h>
#include <api/FilelistUtils.h>

namespace webserver {
	StringList FilelistInfo::subscriptionList = {
		"list_updated"
	};

	FilelistInfo::FilelistInfo(ParentType* aParentModule, const DirectoryListingPtr& aFilelist) : 
		SubApiModule(aParentModule, aFilelist->getUser()->getCID().toBase32(), subscriptionList), 
		dl(aFilelist), 
		itemHandler(properties, std::bind(&FilelistInfo::getCurrentViewItems, this),
		FilelistUtils::getStringInfo, FilelistUtils::getNumericInfo, FilelistUtils::compareItems, FilelistUtils::serializeItem),
		directoryView("filelist_view", this, itemHandler) 
	{

	}

	FilelistItemInfo::List FilelistInfo::getCurrentViewItems() {
		FilelistItemInfo::List ret;
		//boost::range::copy(directoryI | map_values, back_inserter(ret));
		return ret;
	}
}