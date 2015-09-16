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
#include <web-server/WebServerManager.h>

#include <api/TransferApi.h>

#include <api/common/Serializer.h>

#include <client/DownloadManager.h>
#include <client/UploadManager.h>

namespace webserver {
	TransferApi::TransferApi() : timer(WebServerManager::getInstance()->addTimer([this] { onTimer(); }, 1000)) {
		DownloadManager::getInstance()->addListener(this);
		UploadManager::getInstance()->addListener(this);

		subscriptions["statistics"];
		timer->start();
	}

	TransferApi::~TransferApi() {
		timer->stop(true);

		DownloadManager::getInstance()->removeListener(this);
		UploadManager::getInstance()->removeListener(this);
	}

	void TransferApi::onTimer() {
		if (!subscriptions["statistics"])
			return;

		json j = {
			{ "session_down", Socket::getTotalDown() },
			{ "session_up", Socket::getTotalUp() },
			{ "speed_down", DownloadManager::getInstance()->getLastDownSpeed() },
			{ "speed_up", DownloadManager::getInstance()->getLastUpSpeed() },
			{ "upload_bundles", lastUploadBundles },
			{ "download_bundles", lastDownloadBundles },
			{ "uploads", lastUploads },
			{ "downloads", lastDownloads },
		};

		lastUploadBundles = 0;
		lastDownloadBundles = 0;

		lastUploads = 0;
		lastDownloads = 0;

		if (previousStats == j)
			return;

		previousStats = j;
		send("statistics", j);
	}

	void TransferApi::on(UploadManagerListener::Tick, const UploadList& aUploads) noexcept {
		lastUploads = aUploads.size();
	}

	void TransferApi::on(DownloadManagerListener::Tick, const DownloadList& aDownloads) noexcept {
		lastDownloads = aDownloads.size();
	}

	void TransferApi::on(DownloadManagerListener::BundleTick, const BundleList& bundles, uint64_t aTick) noexcept {
		lastDownloadBundles = bundles.size();
	}

	void TransferApi::on(UploadManagerListener::BundleTick, const UploadBundleList& bundles) noexcept {
		lastUploadBundles = bundles.size();
	}
}