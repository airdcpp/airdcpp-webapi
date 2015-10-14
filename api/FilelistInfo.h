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

#ifndef DCPLUSPLUS_DCPP_FILELIST_H
#define DCPLUSPLUS_DCPP_FILELIST_H

#include <web-server/stdinc.h>

#include <airdcpp/typedefs.h>
#include <airdcpp/GetSet.h>

#include <airdcpp/DirectoryListing.h>
#include <airdcpp/QueueItemBase.h>
#include <airdcpp/TargetUtil.h>

#include <api/HierarchicalApiModule.h>
#include <api/common/ListViewController.h>

namespace webserver {
	//typedef uint32_t ResultToken;
	class FilelistItemInfo : public FastAlloc<FilelistItemInfo> {
	public:
		typedef shared_ptr<FilelistItemInfo> Ptr;
		typedef vector<Ptr> List;

		enum ItemType {
			FILE,
			DIRECTORY
		};

		const DirectoryListing::File* file;
		const DirectoryListing::Directory::Ptr dir;

		FilelistItemInfo(DirectoryListing::File* f) : type(FILE), file(f) { }
		FilelistItemInfo(DirectoryListing::Directory::Ptr& d) : type(DIRECTORY), dir(d) {}
		~FilelistItemInfo() { }

		DupeType getDupe() const noexcept { return type == DIRECTORY ? dir->getDupe() : file->getDupe(); }
		const string& getName() const noexcept { return type == DIRECTORY ? dir->getName() : file->getName(); }
		string getPath() const noexcept { return type == DIRECTORY ? dir->getPath() : file->getPath(); }
		bool isAdl() const noexcept { return type == DIRECTORY ? dir->getAdls() : file->getAdls(); }

		time_t getDate() const noexcept { return type == DIRECTORY ? dir->getRemoteDate() : file->getRemoteDate(); }
		time_t getSize() const noexcept { return type == DIRECTORY ? dir->getTotalSize(false) : file->getSize(); }

		DirectoryListingToken getToken() const noexcept { return type == DIRECTORY ? dir->getToken() : file->getToken(); }

		ItemType getType() const noexcept {
			return type;
		}
	private:
		const ItemType type;
	};

	typedef FilelistItemInfo::Ptr FilelistItemInfoPtr;


	class FilelistInfo : public SubApiModule<CID, FilelistInfo, std::string> {
	public:
		typedef ParentApiModule<CID, FilelistInfo> ParentType;
		typedef shared_ptr<FilelistInfo> Ptr;

		static StringList subscriptionList;

		//typedef vector<Ptr> List;
		//typedef unordered_map<TTHValue, Ptr> Map;

		const PropertyList properties = {
			{ PROP_NAME, "name", TYPE_TEXT, SERIALIZE_TEXT, SORT_CUSTOM },
			{ PROP_TYPE, "type", TYPE_TEXT, SERIALIZE_CUSTOM, SORT_CUSTOM },
			{ PROP_SIZE, "size", TYPE_SIZE, SERIALIZE_NUMERIC, SORT_NUMERIC },
			{ PROP_DATE, "time", TYPE_TIME, SERIALIZE_NUMERIC, SORT_NUMERIC },
			{ PROP_PATH, "path", TYPE_TEXT, SERIALIZE_TEXT, SORT_TEXT },
			{ PROP_TTH, "tth", TYPE_TEXT, SERIALIZE_TEXT, SORT_TEXT },
			{ PROP_DUPE, "dupe", TYPE_NUMERIC_OTHER, SERIALIZE_NUMERIC, SORT_NUMERIC },
		};

		enum Properties {
			PROP_TOKEN = -1,
			PROP_NAME,
			PROP_TYPE,
			PROP_SIZE,
			PROP_DATE,
			PROP_PATH,
			PROP_TTH,
			PROP_DUPE,
			PROP_LAST
		};

		FilelistInfo(ParentType* aParentModule, const DirectoryListingPtr& aFilelist);
		~FilelistInfo() {	}

		//const UserPtr& getUser() const { return sr->getUser().user; }
		//const string& getHubUrl() const { return sr->getUser().hint; }

		//api_return download(const string& aTarget, TargetUtil::TargetType aTargetType, QueueItemBase::Priority p);

		/*uint32_t getToken() const noexcept {
			return token;
		}*/
	private:
		FilelistItemInfo::List getCurrentViewItems();
		PropertyItemHandler<FilelistItemInfoPtr> itemHandler;

		typedef ListViewController<FilelistItemInfoPtr, PROP_LAST> DirectoryView;
		DirectoryView directoryView;

		DirectoryListingPtr dl;
	};

	typedef FilelistInfo::Ptr FilelistInfoPtr;
}

#endif