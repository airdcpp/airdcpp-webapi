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
#include <web-server/JsonUtil.h>

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
		METHOD_HANDLER("hub", ApiRequest::METHOD_PATCH, (TOKEN_PARAM), true, FavoriteHubApi::handleUpdateHub);
		METHOD_HANDLER("hub", ApiRequest::METHOD_GET, (TOKEN_PARAM), false, FavoriteHubApi::handleGetHub);

		METHOD_HANDLER("hub", ApiRequest::METHOD_POST, (TOKEN_PARAM, EXACT_PARAM("connect")), false, FavoriteHubApi::handleConnect);
		METHOD_HANDLER("hub", ApiRequest::METHOD_POST, (TOKEN_PARAM, EXACT_PARAM("disconnect")), false, FavoriteHubApi::handleDisconnect);
	}

	FavoriteHubApi::~FavoriteHubApi() {
		FavoriteManager::getInstance()->removeListener(this);
	}

	void FavoriteHubApi::onSocketRemoved() noexcept {
		view.onSocketRemoved();
	}

	string FavoriteHubApi::updateValidatedProperties(FavoriteHubEntryPtr& aEntry, json& j, bool aNewHub) {
		ShareProfilePtr shareProfilePtr = nullptr;

		auto name = JsonUtil::getOptionalField<string>("name", j, false, aNewHub);

		auto server = JsonUtil::getOptionalField<string>("hub_url", j, false, aNewHub);
		if (server) {
			if (!FavoriteManager::getInstance()->isUnique(*server, aEntry->getToken())) {
				JsonUtil::throwError("hub_url", JsonUtil::ERROR_EXISTS, STRING(FAVORITE_HUB_ALREADY_EXISTS));
			}
		}

		{
			auto shareProfileToken = JsonUtil::getOptionalField<ProfileToken>("share_profile", j, false, false);
			if (shareProfileToken) {
				if (!AirUtil::isAdcHub(!server ? aEntry->getServerStr() : *server) && *shareProfileToken != SETTING(DEFAULT_SP)) {
					JsonUtil::throwError("share_profile", JsonUtil::ERROR_INVALID, "Share profiles can't be changed for NMDC hubs");
				}

				shareProfilePtr = ShareManager::getInstance()->getShareProfile(*shareProfileToken, false);
				if (!shareProfilePtr) {
					JsonUtil::throwError("share_profile", JsonUtil::ERROR_INVALID, "Invalid share profile");
				}
			}
		}

		// We have valid values
		if (name) {
			aEntry->setName(*name);
		}

		if (server) {
			aEntry->setServerStr(*server);
		}

		if (shareProfilePtr) {
			aEntry->setShareProfile(shareProfilePtr);
		}

		return Util::emptyString;
	}

	void FavoriteHubApi::updateSimpleProperties(FavoriteHubEntryPtr& aEntry, json& j) {
		for (auto i : json::iterator_wrapper(j)) {
			auto key = i.key();
			if (key == "auto_connect") {
				aEntry->setAutoConnect(JsonUtil::parseValue<bool>("auto_connect", i.value()));
			} else if (key == "hub_description") {
				aEntry->setDescription(JsonUtil::parseValue<string>("hub_description", i.value()));
			} else if (key == "password") {
				aEntry->setPassword(JsonUtil::parseValue<string>("password", i.value()));
			} else if (key == "nick") {
				aEntry->get(HubSettings::Nick) = JsonUtil::parseValue<string>("nick", i.value());
			} else if (key == "user_description") {
				aEntry->get(HubSettings::Description) = JsonUtil::parseValue<string>("user_description", i.value());
			}
		}
	}

	api_return FavoriteHubApi::handleAddHub(ApiRequest& aRequest) throw(exception) {
		auto j = aRequest.getRequestBody();

		FavoriteHubEntryPtr e = new FavoriteHubEntry();

		auto err = updateValidatedProperties(e, j, true);
		if (!err.empty()) {
			aRequest.setResponseErrorStr(err);
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
			aRequest.setResponseErrorStr("Hub not found");
			return websocketpp::http::status_code::not_found;
		}

		return websocketpp::http::status_code::ok;
	}

	api_return FavoriteHubApi::handleGetHub(ApiRequest& aRequest) throw(exception) {
		auto entry = FavoriteManager::getInstance()->getFavoriteHubEntry(aRequest.getTokenParam(0));
		if (!entry) {
			aRequest.setResponseErrorStr("Hub not found");
			return websocketpp::http::status_code::not_found;
		}

		aRequest.setResponseBody(Serializer::serializeItem(entry, itemHandler));
		return websocketpp::http::status_code::ok;
	}

	api_return FavoriteHubApi::handleUpdateHub(ApiRequest& aRequest) throw(exception) {
		auto e = FavoriteManager::getInstance()->getFavoriteHubEntry(aRequest.getTokenParam(0));
		if (!e) {
			aRequest.setResponseErrorStr("Hub not found");
			return websocketpp::http::status_code::not_found;
		}

		// Check existing address
		auto j = aRequest.getRequestBody();

		auto err = updateValidatedProperties(e, j, false);
		if (!err.empty()) {
			aRequest.setResponseErrorStr(err);
			return websocketpp::http::status_code::bad_request;
		}

		updateSimpleProperties(e, j);

		FavoriteManager::getInstance()->onFavoriteHubUpdated(e);

		return websocketpp::http::status_code::ok;
	}

	api_return FavoriteHubApi::handleConnect(ApiRequest& aRequest) throw(exception) {
		auto entry = FavoriteManager::getInstance()->getFavoriteHubEntry(aRequest.getTokenParam(0));
		if (!entry) {
			aRequest.setResponseErrorStr("Hub not found");
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
			aRequest.setResponseErrorStr("Hub not found");
			return websocketpp::http::status_code::not_found;
		}

		ClientManager::getInstance()->putClient(entry->getServers()[0].first);
		return websocketpp::http::status_code::no_content;
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