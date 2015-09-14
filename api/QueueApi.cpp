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

#include <web-server/stdinc.h>

#include <api/QueueApi.h>
#include <api/QueueUtils.h>

#include <api/common/Serializer.h>
#include <api/common/Deserializer.h>

#include <client/QueueManager.h>
#include <client/DownloadManager.h>

#include <boost/range/algorithm/copy.hpp>

namespace webserver {
	QueueApi::QueueApi() : 
			bundlePropertyHandler(bundleProperties, QueueUtils::getBundleList, 
				QueueUtils::getStringInfo, QueueUtils::getNumericInfo, QueueUtils::compareBundles, QueueUtils::serializeBundleProperty),
			bundleView("bundle_view", this, bundlePropertyHandler) {

		QueueManager::getInstance()->addListener(this);
		DownloadManager::getInstance()->addListener(this);

		subscriptions["bundle_added"];
		subscriptions["bundle_removed"];
		subscriptions["bundle_updated"];
		subscriptions["bundle_tick"];

		subscriptions["file_updated"];
		subscriptions["file_removed"];
		subscriptions["file_updated"];

		bundleView.getApiHandlers(requestHandlers, subscriptions);

		METHOD_HANDLER("bundles", ApiRequest::METHOD_GET, (NUM_PARAM, NUM_PARAM), false, QueueApi::handleGetBundles);

		METHOD_HANDLER("bundle", ApiRequest::METHOD_POST, (EXACT_PARAM("file")), true, QueueApi::handleAddFileBundle);
		METHOD_HANDLER("bundle", ApiRequest::METHOD_POST, (EXACT_PARAM("directory")), true, QueueApi::handleAddDirectoryBundle);
		METHOD_HANDLER("bundle", ApiRequest::METHOD_GET, (TOKEN_PARAM), false, QueueApi::handleGetBundle);
		METHOD_HANDLER("bundle", ApiRequest::METHOD_DELETE, (TOKEN_PARAM), false, QueueApi::handleRemoveBundle);
		METHOD_HANDLER("bundle", ApiRequest::METHOD_PUT, (TOKEN_PARAM), true, QueueApi::handleUpdateBundle);

		METHOD_HANDLER("bundle", ApiRequest::METHOD_POST, (TOKEN_PARAM, EXACT_PARAM("search")), false, QueueApi::handleSearchBundle);

		METHOD_HANDLER("temp_item", ApiRequest::METHOD_POST, (), true, QueueApi::handleAddTempItem);
		METHOD_HANDLER("temp_item", ApiRequest::METHOD_GET, (TOKEN_PARAM), false, QueueApi::handleGetFile);
		METHOD_HANDLER("temp_item", ApiRequest::METHOD_DELETE, (TOKEN_PARAM), false, QueueApi::handleRemoveFile);

		METHOD_HANDLER("filelist", ApiRequest::METHOD_POST, (CID_PARAM), true, QueueApi::handleAddFilelist);
		METHOD_HANDLER("filelist", ApiRequest::METHOD_GET, (TOKEN_PARAM), false, QueueApi::handleGetFile);
		METHOD_HANDLER("filelist", ApiRequest::METHOD_DELETE, (TOKEN_PARAM), false, QueueApi::handleRemoveFile);
	}

	QueueApi::~QueueApi() {
		QueueManager::getInstance()->removeListener(this);
		DownloadManager::getInstance()->removeListener(this);
	}

	void QueueApi::onSocketRemoved() noexcept {
		bundleView.onSocketRemoved();
	}

	// BUNDLES
	api_return QueueApi::handleGetBundles(ApiRequest& aRequest)  throw(exception) {
		int start = aRequest.getRangeParam(0);
		int count = aRequest.getRangeParam(1);

		auto j = Serializer::serializeItemList(start, count, bundlePropertyHandler);

		aRequest.setResponseBody(j);
		return websocketpp::http::status_code::ok;
	}

	api_return QueueApi::handleGetBundle(ApiRequest& aRequest) throw(exception) {
		auto b = QueueManager::getInstance()->findBundle(aRequest.getRangeParam(0));
		if (b) {
			auto j = Serializer::serializeItem(b, bundlePropertyHandler);
			aRequest.setResponseBody(j);
			return websocketpp::http::status_code::ok;
		}

		return websocketpp::http::status_code::not_found;
	}

	api_return QueueApi::handleSearchBundle(ApiRequest& aRequest) throw(exception) {
		auto b = QueueManager::getInstance()->findBundle(aRequest.getTokenParam(0));
		if (b) {
			QueueManager::getInstance()->searchBundleAlternates(b, true);
			return  websocketpp::http::status_code::ok;
		}

		return websocketpp::http::status_code::not_found;
	}

	api_return QueueApi::handleAddFileBundle(ApiRequest& aRequest) throw(exception) {
		decltype(auto) j = aRequest.getRequestBody();

		BundlePtr b = nullptr;
		try {
			b = QueueManager::getInstance()->createFileBundle(
				j["target"],
				j["size"],
				Deserializer::deserializeTTH(j),
				Deserializer::deserializeHintedUser(j["user"]),
				j["date"],
				0,
				Deserializer::deserializePriority(j, true)
				);
		}
		catch (const Exception& e) {
			aRequest.setResponseError(e.getError());
			return websocketpp::http::status_code::internal_server_error;
		}

		if (b) {
			json retJson = {
				{ "id", b->getToken() }
			};

			aRequest.setResponseBody(retJson);
		}

		return websocketpp::http::status_code::ok;
	}

	api_return QueueApi::handleAddDirectoryBundle(ApiRequest& aRequest) throw(exception) {
		decltype(auto) j = aRequest.getRequestBody();

		BundleFileInfo::List files;
		for (const auto& fileJson : j["files"]) {
			files.push_back(BundleFileInfo(
				fileJson["name"],
				Deserializer::deserializeTTH(fileJson),
				fileJson["size"],
				fileJson["date"],
				Deserializer::deserializePriority(fileJson, true))
			);
		}

		BundlePtr b = nullptr;
		std::string errors;
		try {
			b = QueueManager::getInstance()->createDirectoryBundle(
				j["target"],
				Deserializer::deserializeHintedUser(j["user"]),
				files,
				Deserializer::deserializePriority(j, true),
				j["date"],
				errors
			);
		}
		catch (const QueueException& e) {
			aRequest.setResponseError(e.getError());
			return websocketpp::http::status_code::internal_server_error;
		}

		if (b) {
			json retJson = {
				{ "id", b->getToken() },
				{ "errors", errors }
			};

			aRequest.setResponseBody(retJson);
		}

		return websocketpp::http::status_code::ok;
	}

	api_return QueueApi::handleRemoveBundle(ApiRequest& aRequest) throw(exception) {
		auto success = QueueManager::getInstance()->removeBundle(aRequest.getTokenParam(0), false);
		return success ? websocketpp::http::status_code::ok : websocketpp::http::status_code::not_found;
	}

	api_return QueueApi::handleUpdateBundle(ApiRequest& aRequest) throw(exception) {
		auto b = QueueManager::getInstance()->findBundle(aRequest.getTokenParam(0));
		if (!b) {
			return websocketpp::http::status_code::not_found;
		}

		decltype(auto) j = aRequest.getRequestBody();

		// Priority
		if (j.find("priority") != j.end()) {
			QueueManager::getInstance()->setBundlePriority(b, Deserializer::deserializePriority(j, false));
		}

		// Target
		auto target = j.find("target");
		if (target != j.end()) {
			QueueManager::getInstance()->moveBundle(b, *target, true);
		}

		return websocketpp::http::status_code::ok;
	}



	// TEMP ITEMS
	api_return QueueApi::handleAddTempItem(ApiRequest& aRequest) throw(exception) {
		decltype(auto) j = aRequest.getRequestBody();

		try {
			QueueManager::getInstance()->addOpenedItem(j["filename"],
				j["size"],
				Deserializer::deserializeTTH(j),
				Deserializer::deserializeHintedUser(j["user"]),
				false
				);
		}
		catch (const Exception& e) {
			aRequest.setResponseError(e.getError());
			return websocketpp::http::status_code::internal_server_error;
		}

		return websocketpp::http::status_code::ok;
	}



	// FILELISTS
	api_return QueueApi::handleAddFilelist(ApiRequest& aRequest) throw(exception) {
		decltype(auto) j = aRequest.getRequestBody();

		auto i = j.find("directory");
		auto directory = i != j.end() ? (*i) : Util::emptyString;

		auto flags = QueueItem::FLAG_PARTIAL_LIST;
		//if (j["match"])
		//	flags = QueueItem::FLAG_MATCH_QUEUE | QueueItem::FLAG_RECURSIVE_LIST;

		try {
			QueueManager::getInstance()->addList(Deserializer::deserializeHintedUser(j), flags, directory);
		} catch (const Exception& e) {
			aRequest.setResponseError(e.getError());
			return websocketpp::http::status_code::internal_server_error;
		}

		return websocketpp::http::status_code::ok;
	}


	// FILES (COMMON)
	api_return QueueApi::handleGetFile(ApiRequest& aRequest) throw(exception) {
		auto success = QueueManager::getInstance()->findFile(aRequest.getTokenParam(0));
		return success ? websocketpp::http::status_code::ok : websocketpp::http::status_code::not_found;
	}

	api_return QueueApi::handleRemoveFile(ApiRequest& aRequest) throw(exception) {
		auto success = QueueManager::getInstance()->removeFile(aRequest.getTokenParam(0), false);
		return success ? websocketpp::http::status_code::ok : websocketpp::http::status_code::not_found;
	}



	// LISTENERS
	void QueueApi::on(QueueManagerListener::BundleAdded, const BundlePtr& aBundle) noexcept {
		bundleView.onItemAdded(aBundle);
		if (!subscriptions["bundle_added"])
			return;

		send("bundle_added", Serializer::serializeItem(aBundle, bundlePropertyHandler));
	}
	void QueueApi::on(QueueManagerListener::BundleRemoved, const BundlePtr& aBundle) noexcept {
		bundleView.onItemRemoved(aBundle);
		if (!subscriptions["bundle_removed"])
			return;

		send("bundle_removed", Serializer::serializeItem(aBundle, bundlePropertyHandler));
	}

	void QueueApi::on(QueueManagerListener::Removed, const QueueItemPtr& aQI, bool /*finished*/) noexcept {
		if (!subscriptions["file_removed"])
			return;

		//send("file_removed", QueueUtils::serializeQueueItem(aQI));
	}
	void QueueApi::on(QueueManagerListener::Added, QueueItemPtr& aQI) noexcept {
		if (!subscriptions["file_added"])
			return;

		//send("file_added", QueueUtils::serializeQueueItem(aQI));
	}

	void QueueApi::onFileUpdated(const QueueItemPtr& aQI) {
		if (!subscriptions["file_updated"])
			return;

		//send("file_updated", QueueUtils::serializeQueueItem(aQI));
	}
	void QueueApi::onBundleUpdated(const BundlePtr& aBundle, const PropertyIdSet& aUpdatedProperties) {
		bundleView.onItemUpdated(aBundle, aUpdatedProperties);

		if (!subscriptions["bundle_updated"])
			return;

		send("bundle_updated", Serializer::serializeItem(aBundle, bundlePropertyHandler));
	}

	void QueueApi::on(DownloadManagerListener::BundleTick, const BundleList& tickBundles, uint64_t /*aTick*/) noexcept {
		bundleView.onItemsUpdated(tickBundles, { PROP_SPEED, PROP_SECONDS_LEFT, PROP_BYTES_DOWNLOADED, PROP_STATUS });
		if (!subscriptions["bundle_tick"])
			return;

		json j;
		for (auto& b : tickBundles) {
			j.push_back(Serializer::serializeItem(b, bundlePropertyHandler));
		}

		send("bundle_tick", j);
	}

	void QueueApi::on(QueueManagerListener::BundleMoved, const BundlePtr& aBundle) noexcept {
		onBundleUpdated(aBundle, { PROP_TARGET, PROP_NAME, PROP_SIZE });
	}
	void QueueApi::on(QueueManagerListener::BundleMerged, const BundlePtr& aBundle, const string&) noexcept {
		onBundleUpdated(aBundle, { PROP_TARGET, PROP_NAME, PROP_SIZE });
	}
	void QueueApi::on(QueueManagerListener::BundleSize, const BundlePtr& aBundle) noexcept {
		onBundleUpdated(aBundle, { PROP_SIZE });
	}
	void QueueApi::on(QueueManagerListener::BundlePriority, const BundlePtr& aBundle) noexcept {
		onBundleUpdated(aBundle, { PROP_PRIORITY });
	}
	void QueueApi::on(QueueManagerListener::BundleStatusChanged, const BundlePtr& aBundle) noexcept {
		onBundleUpdated(aBundle, { PROP_STATUS });
	}
	void QueueApi::on(QueueManagerListener::BundleSources, const BundlePtr& aBundle) noexcept {
		onBundleUpdated(aBundle, { PROP_SOURCES });
	}

	void QueueApi::on(QueueManagerListener::SourcesUpdated, const QueueItemPtr& aQI) noexcept {
		onFileUpdated(aQI);
	}
	void QueueApi::on(QueueManagerListener::StatusUpdated, const QueueItemPtr& aQI) noexcept {
		onFileUpdated(aQI);
	}

	void QueueApi::on(FileRecheckFailed, const QueueItemPtr& aQI, const string& aError) noexcept {
		//onFileUpdated(qi);
	}

	void QueueApi::on(DownloadManagerListener::BundleWaiting, const BundlePtr aBundle) noexcept {
		onBundleUpdated(aBundle, { PROP_SECONDS_LEFT, PROP_SPEED, PROP_STATUS });
	}
}