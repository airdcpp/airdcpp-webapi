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

#include <api/HubApi.h>

#include <airdcpp/ClientManager.h>
#include <airdcpp/HubEntry.h>

namespace webserver {
	HubApi::HubApi() {
		ClientManager::getInstance()->addListener(this);

		METHOD_HANDLER("hub", ApiRequest::METHOD_POST, (), true, HubApi::handleConnect);
	}

	HubApi::~HubApi() {
		ClientManager::getInstance()->removeListener(this);
	}

	api_return HubApi::handleConnect(ApiRequest& aRequest) throw(exception) {
		decltype(auto) req = aRequest.getRequestBody();

		const string address = req["hub_url"];

		RecentHubEntryPtr r = new RecentHubEntry(address);
		//r->setName(entry->getName());
		//r->setDescription(entry->getDescription());
		ClientManager::getInstance()->createClient(r, SETTING(DEFAULT_SP));

		return websocketpp::http::status_code::ok;
	}
}