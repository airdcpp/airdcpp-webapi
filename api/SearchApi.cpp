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

	void SearchApi::onSocketRemoved() noexcept {
		searchView.onSocketRemoved();
	}

	SearchResultInfo::List SearchApi::getResultList() {
		SearchResultInfo::List ret;
		boost::range::copy(results | map_values, back_inserter(ret));
		return ret;
	}

	api_return SearchApi::handleGetTypes(ApiRequest& aRequest) {
		auto getName = [](const string& aId) {
			if (aId.size() == 1 && aId[0] >= '1' && aId[0] <= '6') {
				return string(SearchManager::getTypeStr(aId[0] - '0'));
			}

			return aId;
		};

		auto types = SearchManager::getInstance()->getSearchTypes();

		json retJ;
		for (const auto& s : types) {
			retJ.push_back({
				{ "id", s.first },
				{ "str", getName(s.first) }
			});
		}

		aRequest.setResponseBody(retJ);
		return websocketpp::http::status_code::ok;
	}

	api_return SearchApi::handlePostSearch(ApiRequest& aRequest) {
		decltype(auto) j = aRequest.getRequestBody();
		std::string str = j["pattern"];

		if (str.length() < MIN_SEARCH) {
			aRequest.setResponseErrorStr("Search string too short");
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
		currentSearchToken = Util::toString(Util::rand());

		auto queueTime = SearchManager::getInstance()->search(str, 0, type, SearchManager::SIZE_DONTCARE, currentSearchToken, Search::MANUAL);

		aRequest.setResponseBody({
			{ "queue_time", queueTime },
			{ "search_token", currentSearchToken }
		});
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

		auto result = make_shared<SearchResultInfo>(aResult, *curSearch.get());

		{
			WLock l(cs);
			auto i = results.find(aResult->getTTH());
			if (i != results.end()) {
				(*i).second->addItem(result);
			} else {
				results.emplace(aResult->getTTH(), result);
			}
		}

		searchView.onItemAdded(result);
	}
}