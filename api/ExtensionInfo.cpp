/*
* Copyright (C) 2011-2017 AirDC++ Project
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

#include <api/ExtensionInfo.h>
#include <api/common/Serializer.h>
#include <api/common/SettingUtils.h>

#include <web-server/JsonUtil.h>
#include <web-server/Extension.h>
#include <web-server/ExtensionManager.h>
#include <web-server/Session.h>
#include <web-server/WebServerManager.h>

#include <airdcpp/File.h>


namespace webserver {
	const StringList ExtensionInfo::subscriptionList = {
		"extension_started",
		"extension_stopped",
		"extension_updated",
		"extension_settings_updated",
		"extension_package_updated",
	};

	ExtensionInfo::ExtensionInfo(ParentType* aParentModule, const ExtensionPtr& aExtension) : 
		SubApiModule(aParentModule, aExtension->getName(), subscriptionList),
		extension(aExtension) 
	{
		METHOD_HANDLER(Access::ADMIN, METHOD_POST, (EXACT_PARAM("start")), ExtensionInfo::handleStartExtension);
		METHOD_HANDLER(Access::ADMIN, METHOD_POST, (EXACT_PARAM("stop")), ExtensionInfo::handleStopExtension);

		METHOD_HANDLER(Access::SETTINGS_VIEW, METHOD_GET, (EXACT_PARAM("settings"), EXACT_PARAM("definitions")), ExtensionInfo::handleGetSettingDefinitions);
		METHOD_HANDLER(Access::SETTINGS_EDIT, METHOD_POST, (EXACT_PARAM("settings"), EXACT_PARAM("definitions")), ExtensionInfo::handlePostSettingDefinitions);

		METHOD_HANDLER(Access::SETTINGS_VIEW, METHOD_GET, (EXACT_PARAM("settings")), ExtensionInfo::handleGetSettings);
		METHOD_HANDLER(Access::SETTINGS_EDIT, METHOD_PATCH, (EXACT_PARAM("settings")), ExtensionInfo::handlePostSettings);
	}

	void ExtensionInfo::init() noexcept {
		extension->addListener(this);
	}

	string ExtensionInfo::getId() const noexcept {
		return extension->getName();
	}

	ExtensionInfo::~ExtensionInfo() {
		extension->removeListener(this);
	}

	api_return ExtensionInfo::handleStartExtension(ApiRequest& aRequest) {
		try {
			auto server = aRequest.getSession()->getServer();
			extension->start(server->getExtensionManager().getStartCommand(extension), server);
		} catch (const Exception& e) {
			aRequest.setResponseErrorStr(e.what());
			return websocketpp::http::status_code::internal_server_error;
		}

		return websocketpp::http::status_code::no_content;
	}

	api_return ExtensionInfo::handleStopExtension(ApiRequest& aRequest) {
		extension->stop();
		return websocketpp::http::status_code::no_content;
	}

	api_return ExtensionInfo::handleGetSettings(ApiRequest& aRequest) {
		aRequest.setResponseBody(extension->getSettingValues());
		return websocketpp::http::status_code::ok;
	}

	api_return ExtensionInfo::handleGetSettingDefinitions(ApiRequest& aRequest) {
		auto ret = json::array();
		for (const auto& setting: extension->getSettings()) {
			ret.push_back(setting.serializeDefinitions());
		}

		aRequest.setResponseBody(ret);
		return websocketpp::http::status_code::ok;
	}

	api_return ExtensionInfo::handlePostSettingDefinitions(ApiRequest& aRequest) {
		if (extension->hasSettings()) {
			aRequest.setResponseErrorStr("Setting definitions exist for this extensions already");
			return websocketpp::http::status_code::conflict;
		}

		auto defs = SettingUtils::deserializeDefinitions(aRequest.getRequestBody());
		extension->swapSettingDefinitions(defs);
		return websocketpp::http::status_code::no_content;
	}

	api_return ExtensionInfo::handlePostSettings(ApiRequest& aRequest) {
		SettingValueMap settings;

		// Validate values
		for (const auto& elem : json::iterator_wrapper(aRequest.getRequestBody())) {
			auto setting = extension->getSetting(elem.key());
			if (!setting) {
				JsonUtil::throwError(elem.key(), JsonUtil::ERROR_INVALID, "Setting not found");
			}

			settings[elem.key()] = SettingUtils::validateValue(*setting, elem.value());
		}

		extension->setSettingValues(aRequest.getRequestBody());
		return websocketpp::http::status_code::no_content;
	}

	json ExtensionInfo::serializeExtension(const ExtensionPtr& aExtension) noexcept {
		return {
			{ "id", aExtension->getName() },
			{ "name", aExtension->getName() },
			{ "description", aExtension->getDescription() },
			{ "version", aExtension->getVersion() },
			{ "homepage", aExtension->getHomepage() },
			{ "author", aExtension->getAuthor() },
			{ "running", aExtension->isRunning() },
			{ "private", aExtension->isPrivate() },
			{ "logs", ExtensionInfo::serializeLogs(aExtension) },
			{ "engines", aExtension->getEngines() },
			{ "managed", aExtension->isManaged() },
			{ "has_settings", aExtension->hasSettings() },
		};
	}

	void ExtensionInfo::on(ExtensionListener::SettingValuesUpdated, const SettingValueMap& aUpdatedSettings) noexcept {
		maybeSend("extension_settings_updated", [&aUpdatedSettings] {
			return aUpdatedSettings;
		});
	}

	void ExtensionInfo::on(ExtensionListener::SettingDefinitionsUpdated) noexcept {
		onUpdated([&] {
			return json({
				{ "has_settings", extension->hasSettings() }
			});
		});
	}

	json ExtensionInfo::serializeLogs(const ExtensionPtr& aExtension) noexcept {
		return Serializer::serializeList(aExtension->getLogs(), Serializer::serializeFilesystemItem);
	}

	void ExtensionInfo::on(ExtensionListener::ExtensionStarted) noexcept {
		onUpdated([&] {
			return json({
				{ "running", extension->isRunning() }
			});
		});

		maybeSend("extension_started", [&] {
			return serializeExtension(extension);
		});
	}

	void ExtensionInfo::on(ExtensionListener::ExtensionStopped) noexcept {
		onUpdated([&] {
			return json({
				{ "running", extension->isRunning() }
			});
		});

		maybeSend("extension_stopped", [&] {
			return serializeExtension(extension);
		});
	}

	void ExtensionInfo::on(ExtensionListener::PackageUpdated) noexcept {
		onUpdated([&] {
			return serializeExtension(extension);
		});

		maybeSend("extension_package_updated", [&] {
			return serializeExtension(extension);
		});
	}

	void ExtensionInfo::onUpdated(const JsonCallback& aDataCallback) noexcept {
		maybeSend("extension_updated", aDataCallback);
	}
}