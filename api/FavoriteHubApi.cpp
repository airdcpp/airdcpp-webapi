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

#include <api/FavoriteHubApi.h>
#include <api/FavoriteHubUtils.h>

#include <client/AirUtil.h>
#include <client/ClientManager.h>
#include <client/FavoriteManager.h>
#include <client/ShareManager.h>

namespace webserver {
	FavoriteHubApi::FavoriteHubApi() : itemHandler(properties, FavoriteHubUtils::getEntryList,
		FavoriteHubUtils::getStringInfo, FavoriteHubUtils::getNumericInfo, FavoriteHubUtils::compareEntries, FavoriteHubUtils::serializeHub),
		view("favorite_hub_view", this, itemHandler) {

		FavoriteManager::getInstance()->addListener(this);

		view.getApiHandlers(requestHandlers, subscriptions);

		METHOD_HANDLER("hub", ApiRequest::METHOD_POST, (), true, FavoriteHubApi::handleAddHub);
		METHOD_HANDLER("hub", ApiRequest::METHOD_DELETE, (TOKEN_PARAM), false, FavoriteHubApi::handleRemoveHub);
		METHOD_HANDLER("hub", ApiRequest::METHOD_PUT, (TOKEN_PARAM), true, FavoriteHubApi::handleUpdateHub);

		METHOD_HANDLER("hub", ApiRequest::METHOD_POST, (TOKEN_PARAM, EXACT_PARAM("connect")), false, FavoriteHubApi::handleConnect);
		METHOD_HANDLER("hub", ApiRequest::METHOD_POST, (TOKEN_PARAM, EXACT_PARAM("disconnect")), false, FavoriteHubApi::handleDisconnect);
	}

	FavoriteHubApi::~FavoriteHubApi() {
		FavoriteManager::getInstance()->removeListener(this);
	}

	string FavoriteHubApi::updateValidatedProperties(FavoriteHubEntryPtr& aEntry, json& j, bool aNewHub) noexcept {
		string name, server;
		ShareProfilePtr shareProfile = nullptr;

		auto i = j.find("name");
		if (i != j.end()) {
			name = (*i).get<string>();
			if (name.empty()) {
				return STRING(NAME_REQUIRED);
			}
		} else if (aNewHub) {
			return STRING(NAME_REQUIRED);
		}

		i = j.find("hub_url");
		if (i != j.end()) {
			server = (*i).get<string>();
			if (server.empty()) {
				return STRING(INCOMPLETE_FAV_HUB);
			}

			if (!FavoriteManager::getInstance()->isUnique(server, aEntry->getToken())) {
				return STRING(FAVORITE_HUB_ALREADY_EXISTS);
			}
		} else if (aNewHub) {
			return STRING(INCOMPLETE_FAV_HUB);
		}

		i = j.find("share_profile");
		if (i != j.end()) {
			if (AirUtil::isAdcHub(server.empty() ? aEntry->getServerStr() : server)) {
				return "Share profiles can't be changed for NMDC hubs";
			}

			shareProfile = ShareManager::getInstance()->getShareProfile(*i, false);
			if (!shareProfile) {
				return "Invalid share profile";
			}
		}

		// We have valid values
		if (!name.empty()) {
			aEntry->setName(name);
		}

		if (!server.empty()) {
			aEntry->setServerStr(server);
		}

		if (shareProfile) {
			aEntry->setShareProfile(shareProfile);
		}

		return Util::emptyString;
	}

	void FavoriteHubApi::updateSimpleProperties(FavoriteHubEntryPtr& aEntry, json& j) noexcept {
		for (auto i : json::iterator_wrapper(j)) {
			auto key = i.key();
			if (key == "auto_connect") {
				aEntry->setAutoConnect(i.value());
			} else if (key == "hub_description") {
				const string val = i.value();
				aEntry->setDescription(val);
			} else if (key == "password") {
				const string val = i.value();
				aEntry->setPassword(val);
			} else if (key == "nick") {
				const string val = i.value();
				aEntry->get(HubSettings::Nick) = val;
			} else if (key == "user_description") {
				const string val = i.value();
				aEntry->get(HubSettings::Description) = val;
			}
		}
	}

	api_return FavoriteHubApi::handleAddHub(ApiRequest& aRequest) throw(exception) {
		auto j = aRequest.getRequestBody();

		FavoriteHubEntryPtr e = new FavoriteHubEntry();

		auto err = updateValidatedProperties(e, j, true);
		if (!err.empty()) {
			aRequest.setResponseError(err);
			return websocketpp::http::status_code::bad_request;
		}

		updateSimpleProperties(e, j);

		FavoriteManager::getInstance()->addFavoriteHub(e);

		json response;
		response["id"] = e->getToken();
		aRequest.setResponseBody(response);

		return websocketpp::http::status_code::ok;
	}

	api_return FavoriteHubApi::handleRemoveHub(ApiRequest& aRequest) throw(exception) {
		auto token = aRequest.getTokenParam(0);
		if (!FavoriteManager::getInstance()->removeFavoriteHub(token)) {
			aRequest.setResponseError("Hub not found");
			return websocketpp::http::status_code::not_found;
		}

		return websocketpp::http::status_code::ok;
	}

	api_return FavoriteHubApi::handleUpdateHub(ApiRequest& aRequest) throw(exception) {
		auto e = FavoriteManager::getInstance()->getFavoriteHubEntry(aRequest.getTokenParam(0));
		if (!e) {
			aRequest.setResponseError("Hub not found");
			return websocketpp::http::status_code::not_found;
		}

		// Check existing address
		auto j = aRequest.getRequestBody();

		auto err = updateValidatedProperties(e, j, false);
		if (!err.empty()) {
			aRequest.setResponseError(err);
			return websocketpp::http::status_code::bad_request;
		}

		updateSimpleProperties(e, j);

		FavoriteManager::getInstance()->onFavoriteHubUpdated(e);

		return websocketpp::http::status_code::ok;
	}

	api_return FavoriteHubApi::handleConnect(ApiRequest& aRequest) throw(exception) {
		auto entry = FavoriteManager::getInstance()->getFavoriteHubEntry(aRequest.getTokenParam(0));
		if (!entry) {
			aRequest.setResponseError("Hub not found");
			return websocketpp::http::status_code::not_found;
		}

		RecentHubEntryPtr r = new RecentHubEntry(entry->getServers()[0].first);
		r->setName(entry->getName());
		r->setDescription(entry->getDescription());
		ClientManager::getInstance()->createClient(r, entry->getShareProfile()->getToken());
		return websocketpp::http::status_code::ok;
	}

	api_return FavoriteHubApi::handleDisconnect(ApiRequest& aRequest) throw(exception) {
		auto entry = FavoriteManager::getInstance()->getFavoriteHubEntry(aRequest.getTokenParam(0));
		if (!entry) {
			aRequest.setResponseError("Hub not found");
			return websocketpp::http::status_code::not_found;
		}

		ClientManager::getInstance()->putClient(entry->getServers()[0].first);
		return websocketpp::http::status_code::ok;
	}

	void FavoriteHubApi::on(FavoriteManagerListener::FavoriteHubAdded, const FavoriteHubEntryPtr& e)  noexcept {
		view.onItemAdded(e);
	}
	void FavoriteHubApi::on(FavoriteManagerListener::FavoriteHubRemoved, const FavoriteHubEntryPtr& e) noexcept {
		view.onItemRemoved(e);
	}
	void FavoriteHubApi::on(FavoriteManagerListener::FavoriteHubUpdated, const FavoriteHubEntryPtr& e) noexcept {
		view.onItemUpdated(e, toPropertyIdSet(properties));
	}
}