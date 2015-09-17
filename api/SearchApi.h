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

#ifndef DCPLUSPLUS_DCPP_SEARCHAPI_H
#define DCPLUSPLUS_DCPP_SEARCHAPI_H

#include <web-server/stdinc.h>

#include <api/ApiModule.h>
#include <api/common/ListViewController.h>

#include <client/typedefs.h>
#include <client/AirUtil.h>
#include <client/SearchManager.h>
#include <client/SearchQuery.h>
#include <client/UserInfoBase.h>
#include <client/SearchResult.h>

namespace webserver {
	class SearchApi : public ApiModule, private SearchManagerListener {
	public:
		SearchApi();
		~SearchApi();

		int getVersion() const noexcept {
			return 0;
		}

		const PropertyList properties = {
			{ PROP_NAME, "name", TYPE_TEXT, SERIALIZE_TEXT, SORT_CUSTOM },
			{ PROP_RELEVANCY, "relevancy", TYPE_NUMERIC_OTHER, SERIALIZE_NUMERIC, SORT_NUMERIC },
			{ PROP_HITS, "hits", TYPE_NUMERIC_OTHER, SERIALIZE_NUMERIC, SORT_NUMERIC },
			{ PROP_USERS, "users", TYPE_TEXT, SERIALIZE_TEXT, SORT_TEXT },
			{ PROP_TYPE, "type", TYPE_TEXT, SERIALIZE_CUSTOM, SORT_CUSTOM },
			{ PROP_SIZE, "size", TYPE_SIZE, SERIALIZE_NUMERIC, SORT_NUMERIC },
			{ PROP_DATE, "time", TYPE_TIME, SERIALIZE_NUMERIC, SORT_NUMERIC },
			{ PROP_PATH, "path", TYPE_TEXT, SERIALIZE_TEXT, SORT_TEXT },
			{ PROP_CONNECTION, "connection", TYPE_SPEED, SERIALIZE_NUMERIC, SORT_NUMERIC },
			{ PROP_SLOTS, "slots", TYPE_TEXT, SERIALIZE_CUSTOM, SORT_CUSTOM }
		};

		enum Properties {
			PROP_TOKEN = -1,
			PROP_NAME,
			PROP_RELEVANCY,
			PROP_HITS,
			PROP_USERS,
			PROP_TYPE,
			PROP_SIZE,
			PROP_DATE,
			PROP_PATH,
			PROP_CONNECTION,
			PROP_SLOTS,
			PROP_LAST
		};

		//websocketpp::http::status_code::value handleRequest(ApiRequest& aRequest, const SessionPtr& aSession, const json& aJson, string& output_) throw(exception);

		class SearchInfo : public UserInfoBase {
		public:
			//typedef SearchInfo* Ptr;
			typedef shared_ptr<SearchInfo> Ptr;
			typedef vector<Ptr> List;

			SearchInfo::List subItems;

			SearchInfo(const SearchResultPtr& aSR, const SearchQuery& aSearch);
			~SearchInfo() {	}

			const UserPtr& getUser() const { return sr->getUser().user; }
			const string& getHubUrl() const { return sr->getUser().hint; }

			//bool collapsed = true;
			size_t hits = 0;
			//SearchInfo* parent = nullptr;

			/*struct CheckTTH {
				CheckTTH() : op(true), firstHubs(true), firstPath(true), firstTTH(true) { }

				void operator()(SearchInfo* si);
				bool firstHubs;
				StringList hubs;
				bool op;

				bool firstTTH;
				bool firstPath;
				optional<TTHValue> tth;
				optional<string> path;
			};*/

			//inline SearchInfo* createParent() { return new SearchInfo(*this); }
			//inline SearchInfo* createParent() { return this; }
			//inline const TTHValue& getGroupCond() const { return sr->getTTH(); }

			bool isDupe() const { return dupe != DUPE_NONE; }
			bool isShareDupe() const { return dupe == DUPE_SHARE || dupe == DUPE_SHARE_PARTIAL; }
			bool isQueueDupe() const { return dupe == DUPE_QUEUE || dupe == DUPE_FINISHED; }
			//StringList getDupePaths() const;

			SearchResultPtr sr;
			//IGETSET(uint8_t, flagIndex, FlagIndex, 0);
			IGETSET(DupeType, dupe, Dupe, DUPE_NONE);
			//GETSET(tstring, ipText, IpText);

			double getTotalRelevancy() const;
			double getMatchRelevancy() const { return matchRelevancy; }

			uint32_t getToken() const noexcept {
				return token;
			}
		private:
			double matchRelevancy = 0;
			double sourceScoreFactor = 0.01;
			uint32_t token;
		};

		typedef SearchInfo::Ptr SearchInfoPtr;
	private:
		const SearchInfo::List& getResultList();

		api_return SearchApi::handlePostSearch(ApiRequest& aRequest);
		api_return SearchApi::handleGetTypes(ApiRequest& aRequest);

		void on(SearchManagerListener::SR, const SearchResultPtr& aResult) noexcept;

		PropertyItemHandler<SearchInfoPtr> itemHandler;

		typedef ListViewController<SearchInfoPtr, PROP_LAST> SearchView;
		SearchView searchView;

		SearchInfo::List results;
		shared_ptr<SearchQuery> curSearch;

		std::string  currentSearchToken;
		SharedMutex cs;
	};
}

#endif