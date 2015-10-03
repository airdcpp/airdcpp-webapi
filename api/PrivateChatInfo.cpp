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

#include <api/PrivateChatInfo.h>
#include <api/common/Serializer.h>

#include <airdcpp/PrivateChat.h>

namespace webserver {
	PrivateChatInfo::PrivateChatInfo(Session* aSession, const PrivateChatPtr& aChat) : ApiModule(aSession), chat(aChat) {

	}

	json PrivateChatInfo::serialize() const noexcept {
		return Serializer::serializeUser(chat->getUser());
	}

	//FilelistItemInfo::List FilelistInfo::getCurrentViewItems() {
	//	FilelistItemInfo::List ret;
		//boost::range::copy(directoryI | map_values, back_inserter(ret));
	//	return ret;
	//}
}