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

#include <api/SearchApi.h>
#include <api/SearchUtils.h>

//#include <client/AirUtil.h>
#include <client/ScopedFunctor.h>

const unsigned int MIN_SEARCH = 2;

namespace webserver {
	SearchApi::SearchApi() : itemHandler(properties, std::bind(&SearchApi::getResultList, this), 
		SearchUtils::getStringInfo, SearchUtils::getNumericInfo, SearchUtils::compareResults, SearchUtils::serializeResult), 
		searchView("search_view", this, itemHandler) {

		SearchManager::getInstance()->addListener(this);

		//subscriptions["search_result"];
		searchView.getApiHandlers(requestHandlers, subscriptions);

		METHOD_HANDLER("query", ApiRequest::METHOD_POST, (), true, SearchApi::handlePostSearch);
		METHOD_HANDLER("types", ApiRequest::METHOD_GET, (), false, SearchApi::handleGetTypes);
	}

	SearchApi::~SearchApi() {
		SearchManager::getInstance()->removeListener(this);
	}

	const SearchApi::SearchInfo::List& SearchApi::getResultList() {
		return results;
	}

	SearchApi::SearchInfo::SearchInfo(const SearchResultPtr& aSR, const SearchQuery& aSearch) : token(Util::rand()), sr(aSR) {
		//check the dupe
		if (SETTING(DUPE_SEARCH)) {
			if (sr->getType() == SearchResult::TYPE_DIRECTORY)
				dupe = AirUtil::checkDirDupe(sr->getPath(), sr->getSize());
			else
				dupe = AirUtil::checkFileDupe(sr->getTTH());
		}

		// don't count the levels because they can't be compared with each others...
		matchRelevancy = SearchQuery::getRelevancyScores(aSearch, 0, aSR->getType() == SearchResult::TYPE_DIRECTORY, aSR->getFileName());
		if (aSearch.recursion && aSearch.recursion->isComplete()) {
			// there are subdirectories/files that have more matches than the main directory
			// don't give too much weight for those
			sourceScoreFactor = 0.001;

			// we don't get the level scores so balance those here
			matchRelevancy = max(0.0, matchRelevancy - (0.05*aSearch.recursion->recursionLevel));
		}

		//get the ip info
		/*std::string ip = sr->getIP();
		if (!ip.empty()) {
			// Only attempt to grab a country mapping if we actually have an IP address
			std::string tmpCountry = GeoManager::getInstance()->getCountry(sr->getIP());
			if (!tmpCountry.empty()) {
				ip = tmpCountry + " (" + ip + ")";
				flagIndex = Localization::getFlagIndexByCode(tmpCountry.c_str());
			}
		}*/
	}

	double SearchApi::SearchInfo::getTotalRelevancy() const {
		return (hits * sourceScoreFactor) + matchRelevancy;
	}

	api_return SearchApi::handleGetTypes(ApiRequest& aRequest) {
		auto types = SearchManager::getInstance()->getSearchTypes();

		return websocketpp::http::status_code::ok;
	}

	api_return SearchApi::handlePostSearch(ApiRequest& aRequest) {
		/*
		if(m_lastSearch && m_lastSearch + 3*1000 < TimerManager::getInstance()->getTick()) {
		core::Log::get()->log("Wait a moment before a new search");
		return;
		}
		*/

		decltype(auto) j = aRequest.getRequestBody();
		std::string str = j["pattern"];

		if (str.length() < MIN_SEARCH) {
			aRequest.setResponseError("Search std::string too short");
			return websocketpp::http::status_code::bad_request;
		}

		searchView.clearItems();

		auto type = str.size() == 39 && Encoder::isBase32(str.c_str()) ? SearchManager::TYPE_TTH : SearchManager::TYPE_ANY;

		// new search
		auto newSearch = SearchQuery::getSearch(str, Util::emptyString, 0, type, SearchManager::SIZE_DONTCARE, StringList(), SearchQuery::MATCH_FULL_PATH, false);

		{
			WLock l(cs);
			results.clear();
		}

		curSearch = shared_ptr<SearchQuery>(newSearch);

		SettingsManager::getInstance()->addToHistory(str, SettingsManager::HISTORY_SEARCH);
		//lastSearch = GET_TICK();
		currentSearchToken = Util::toString(Util::rand());

		SearchManager::getInstance()->search(str, 0, type, SearchManager::SIZE_DONTCARE, currentSearchToken, Search::MANUAL);
		return websocketpp::http::status_code::ok;
	}


	void SearchApi::on(SearchManagerListener::SR, const SearchResultPtr& aResult) noexcept {
		auto search = curSearch;
		if (!search)
			return;

		if (!aResult->getToken().empty()) {
			// ADC
			if (currentSearchToken != aResult->getToken()) {
				return;
			}
		} else {
			// NMDC

			// exludes
			RLock l(cs);
			if (curSearch->isExcluded(aResult->getPath())) {
				return;
			}

			if (search->root && *search->root != aResult->getTTH()) {
				return;
			}
		}

		if (search->itemType == SearchQuery::TYPE_FILE && aResult->getType() != SearchResult::TYPE_FILE) {
			return;
		}

		// path
		SearchQuery::Recursion recursion;
		ScopedFunctor([&] { search->recursion = nullptr; });
		if (!search->root && !search->matchesNmdcPath(aResult->getPath(), recursion)) {
			return;
		}

		auto i = make_shared<SearchInfo>(aResult, *curSearch.get());

		{
			WLock l(cs);
			results.push_back(i);
		}

		searchView.onItemAdded(i);
	}
}