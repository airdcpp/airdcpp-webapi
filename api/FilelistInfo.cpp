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

#include <airdcpp/Download.h>
#include <airdcpp/DownloadManager.h>

namespace webserver {
	StringList FilelistInfo::subscriptionList = {
		"filelist_updated"
	};

	FilelistInfo::FilelistInfo(ParentType* aParentModule, const DirectoryListingPtr& aFilelist) : 
		SubApiModule(aParentModule, aFilelist->getUser()->getCID().toBase32(), subscriptionList), 
		dl(aFilelist), 
		itemHandler(properties, std::bind(&FilelistInfo::getCurrentViewItems, this),
		FilelistUtils::getStringInfo, FilelistUtils::getNumericInfo, FilelistUtils::compareItems, FilelistUtils::serializeItem),
		directoryView("filelist_view", this, itemHandler) 
	{
		dl->addListener(this);
		DownloadManager::getInstance()->addListener(this);
	}

	FilelistInfo::~FilelistInfo() {
		DownloadManager::getInstance()->removeListener(this);
		dl->removeListener(this);
	}

	FilelistItemInfo::List FilelistInfo::getCurrentViewItems() {
		//dl->addAsyncTask([&] {
		//	dl->get
		//});
		//boost::range::copy(directoryI | map_values, back_inserter(ret));
		//return ret;
	}

	json FilelistInfo::serializeState() noexcept {
		/*if (!aClient->getRedirectUrl().empty()) {
			return{
				{ "id", "redirect" },
				{ "hub_url", aClient->getRedirectUrl() }
			};
		}*/

		string id;
		switch (state) {
			case STATE_DOWNLOAD_PENDING: id = "download_pending"; break;
			case STATE_DOWNLOADING: id = "downloading"; break;
			case STATE_LOADING: id = "loading"; break;
			case STATE_LOADED: id = "loaded"; break;
		}

		return{
			{ "id", id }
		};
	}

	void FilelistInfo::on(DirectoryListingListener::LoadingFinished, int64_t aStart, const string& aDir, bool reloadList, bool changeDir, bool loadInGUIThread) noexcept {

	}

	void FilelistInfo::on(DirectoryListingListener::LoadingFailed, const string& aReason) noexcept {

	}

	void FilelistInfo::on(DirectoryListingListener::LoadingStarted, bool changeDir) noexcept {

	}

	void FilelistInfo::on(DirectoryListingListener::ChangeDirectory, const string& aDir, bool isSearchChange) noexcept {

	}

	void FilelistInfo::on(DirectoryListingListener::UpdateStatusMessage, const string& aMessage) noexcept {

	}

	void FilelistInfo::on(DownloadManagerListener::Failed, const Download* aDownload, const string& aReason) noexcept {
		if (aDownload->isFileList() && aDownload->getUser() == dl->getUser()) {
			state = STATE_DOWNLOAD_PENDING;
		}
	}

	void FilelistInfo::on(DownloadManagerListener::Starting, const Download* aDownload) noexcept {

	}

	void FilelistInfo::on(DirectoryListingListener::UserUpdated) noexcept {
		onSessionUpdated({
			{ "user", Serializer::serializeHintedUser(dl->getHintedUser()) }
		});
	}

	void FilelistInfo::onSessionUpdated(const json& aData) noexcept {
		if (!subscriptionActive("filelist_updated")) {
			return;
		}

		send("filelist_updated", aData);
	}
}