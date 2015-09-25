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

#include <api/ShareApi.h>
#include <api/common/Serializer.h>
#include <api/common/Deserializer.h>
#include <web-server/JsonUtil.h>

#include <client/ShareManager.h>
#include <client/HubEntry.h>

namespace webserver {
	ShareApi::ShareApi() {
		ShareManager::getInstance()->addListener(this);

		METHOD_HANDLER("profiles", ApiRequest::METHOD_GET, (), false, ShareApi::handleGetProfiles);
		METHOD_HANDLER("roots", ApiRequest::METHOD_GET, (), false, ShareApi::handleGetRoots);

		METHOD_HANDLER("find_dupe_paths", ApiRequest::METHOD_POST, (), true, ShareApi::handleFindDupePaths);
	}

	ShareApi::~ShareApi() {
		ShareManager::getInstance()->removeListener(this);
	}

	api_return ShareApi::handleGetProfiles(ApiRequest& aRequest) throw(exception) {
		json j;

		auto profiles = ShareManager::getInstance()->getProfiles();
		for (const auto& p : profiles) {
			j.push_back({
				{ "id", p->getToken() },
				{ "str", p->getDisplayName() },
				{ "default", p->isDefault() }
			});
		}

		aRequest.setResponseBody(j);
		return websocketpp::http::status_code::ok;
	}

	api_return ShareApi::handleGetRoots(ApiRequest& aRequest) throw(exception) {
		json ret;

		auto roots = ShareManager::getInstance()->getGroupedDirectories();
		for (const auto& vPath : roots) {
			json parentJson;
			parentJson["name"] = vPath.first;
			for (const auto& realPath : vPath.second) {
				parentJson["paths"].push_back(realPath);
			}

			ret.push_back(parentJson);
		}

		aRequest.setResponseBody(ret);
		return websocketpp::http::status_code::ok;
	}

	api_return ShareApi::handleFindDupePaths(ApiRequest& aRequest) throw(exception) {
		decltype(auto) requestJson = aRequest.getRequestBody();

		json ret;

		StringList paths;
		auto path = JsonUtil::getOptionalField<string>("path", requestJson, false, false);
		if (path) {
			paths = ShareManager::getInstance()->getDirPaths(*path);
		} else {
			auto tth = Deserializer::deserializeTTH(requestJson);
			paths = ShareManager::getInstance()->getRealPaths(tth);
		}

		for (const auto& p : paths) {
			ret.push_back(p);
		}

		aRequest.setResponseBody(ret);
		return websocketpp::http::status_code::ok;
	}
}