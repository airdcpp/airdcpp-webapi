/*
* Copyright (C) 2011-2019 AirDC++ Project
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

#include "stdinc.h"

#include <api/SettingApi.h>

#include <api/CoreSettings.h>
#include <api/common/Serializer.h>
#include <api/common/SettingUtils.h>

#include <web-server/ApiSettingItem.h>
#include <web-server/JsonUtil.h>
#include <web-server/WebServerManager.h>
#include <web-server/WebServerSettings.h>

#include <airdcpp/SettingHolder.h>

namespace webserver {
	SettingApi::SettingApi(Session* aSession) : ApiModule(aSession) {
		METHOD_HANDLER(Access::SETTINGS_VIEW,	METHOD_POST, (EXACT_PARAM("definitions")),	SettingApi::handleGetDefinitions);
		METHOD_HANDLER(Access::ANY,				METHOD_POST, (EXACT_PARAM("get")),			SettingApi::handleGetValues);
		METHOD_HANDLER(Access::SETTINGS_EDIT,	METHOD_POST, (EXACT_PARAM("set")),			SettingApi::handleSetValues);
		METHOD_HANDLER(Access::SETTINGS_EDIT,	METHOD_POST, (EXACT_PARAM("reset")),		SettingApi::handleResetValues);
	}

	SettingApi::~SettingApi() {
	}

	api_return SettingApi::handleGetDefinitions(ApiRequest& aRequest) {
		const auto& requestJson = aRequest.getRequestBody();

		json retJson = json::array();
		parseSettingKeys(requestJson, [&](ApiSettingItem& aItem) {
			retJson.push_back(SettingUtils::serializeDefinition(aItem));
		}, aRequest.getSession()->getServer());

		aRequest.setResponseBody(retJson);
		return websocketpp::http::status_code::ok;
	}

	api_return SettingApi::handleGetValues(ApiRequest& aRequest) {
		const auto& requestJson = aRequest.getRequestBody();

		auto valueMode = JsonUtil::getEnumFieldDefault<string>("value_mode", requestJson, "current", { "current", "force_auto", "force_manual" });
		if (JsonUtil::getOptionalFieldDefault<bool>("force_auto_values", requestJson, false)) { // Deprecated
			valueMode = "force_auto";
		}

		auto retJson = json::object();
		parseSettingKeys(requestJson, [&](ApiSettingItem& aItem) {
			if (valueMode != "force_manual" && aItem.usingAutoValue(valueMode == "force_auto")) {
				retJson[aItem.name] = aItem.getAutoValue();
			} else {
				retJson[aItem.name] = aItem.getValue();
			}
		}, aRequest.getSession()->getServer());

		aRequest.setResponseBody(retJson);
		return websocketpp::http::status_code::ok;
	}

	void SettingApi::parseSettingKeys(const json& aJson, ParserF aHandler, WebServerManager* aWsm) {
		auto keys = JsonUtil::getField<StringList>("keys", aJson, true);
		for (const auto& key : keys) {
			auto setting = getSettingItem(key, aWsm);
			if (!setting) {
				JsonUtil::throwError(key, JsonUtil::ERROR_INVALID, "Setting not found");
			}

			aHandler(*setting);
		}
	}

	api_return SettingApi::handleResetValues(ApiRequest& aRequest) {
		const auto& requestJson = aRequest.getRequestBody();

		parseSettingKeys(requestJson, [&](ApiSettingItem& aItem) {
			aItem.unset();
		}, aRequest.getSession()->getServer());

		return websocketpp::http::status_code::no_content;
	}

	api_return SettingApi::handleSetValues(ApiRequest& aRequest) {
		auto server = aRequest.getSession()->getServer();
		auto holder = make_shared<SettingHolder>(
			[=](const string& aError) {
				server->log(aError, LogMessage::SEV_ERROR);
			}
		);

		bool hasSet = false;
		for (const auto& elem : aRequest.getRequestBody().items()) {
			auto setting = getSettingItem(elem.key(), aRequest.getSession()->getServer());
			if (!setting) {
				JsonUtil::throwError(elem.key(), JsonUtil::ERROR_INVALID, "Setting not found");
			}

			setting->setValue(SettingUtils::validateValue(elem.value(), *setting, nullptr));
			hasSet = true;
		}

		dcassert(hasSet);

		SettingsManager::getInstance()->save();
		server->setDirty();

		server->addAsyncTask([=] {
			holder->apply();
		});

		return websocketpp::http::status_code::no_content;
	}

	ApiSettingItem* SettingApi::getSettingItem(const string& aKey, WebServerManager* aWsm) noexcept {
		auto p = ApiSettingItem::findSettingItem<CoreSettingItem>(coreSettings, aKey);
		if (p) {
			return p;
		}

		return aWsm->getSettings().getSettingItem(aKey);
	}
}
