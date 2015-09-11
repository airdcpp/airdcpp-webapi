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

#ifndef DCPLUSPLUS_DCPP_LISTVIEW_H
#define DCPLUSPLUS_DCPP_LISTVIEW_H

#include <web-server/stdinc.h>
#include <web-server/WebServerManager.h>

#include <client/typedefs.h>
#include <client/TaskQueue.h>

#include <api/ApiModule.h>
#include <api/common/PropertyFilter.h>
#include <api/common/Serializer.h>

namespace webserver {

	template<class T, int PropertyCount>
	class ListViewController {
	public:
		typedef typename PropertyItemHandler<T>::ItemList ItemList;

		ListViewController(const string& aViewName, ApiModule* aModule, const PropertyItemHandler<T>& aItemHandler) :
			module(aModule), viewName(aViewName), itemHandler(aItemHandler), filter(aItemHandler.properties),
			timer(WebServerManager::getInstance()->addTimer([this] { runTasks(); }, 200))
		{

		}

		~ListViewController() {
			timer->stop();
		}

		int getSortProperty() const noexcept {
			return sortPropery;
		}

		void getApiHandlers(ApiModule::RequestHandlerMap& requestHandlers, ApiModule::SubscriptionMap& subscriptions) {
			METHOD_HANDLER(viewName, ApiRequest::METHOD_PUT, (EXACT_PARAM("filter")), true, ListViewController::handlePutFilter);
			METHOD_HANDLER(viewName, ApiRequest::METHOD_DELETE, (EXACT_PARAM("filter")), false, ListViewController::handleDeleteFilter);

			METHOD_HANDLER(viewName, ApiRequest::METHOD_POST, (), true, ListViewController::handlePostSettings);
			METHOD_HANDLER(viewName, ApiRequest::METHOD_DELETE, (), false, ListViewController::handleReset);

			METHOD_HANDLER(viewName, ApiRequest::METHOD_GET, (EXACT_PARAM("items"), NUM_PARAM, NUM_PARAM), false, ListViewController::handleGetItems);
		}

		void onSocketRemoved() {
			reset();
		}

		api_return handlePutFilter(ApiRequest& aRequest) throw(exception) {
			decltype(auto) j = aRequest.getRequestBody();

			std::string pattern = j["pattern"];
			if (pattern.empty()) {
				resetFilter();
			} else {
				setFilter(pattern, j["method"], findPropertyByName(j["property"], itemHandler.properties));
			}
			return websocketpp::http::status_code::ok;
		}

		api_return handlePostSettings(ApiRequest& aRequest) throw(exception) {
			parseProperties(aRequest.getRequestBody());

			if (!active) {
				active = true;

				{
					WLock l(cs);
					allItems = itemHandler.itemListF();
				}

				timer->start();
			}
			return websocketpp::http::status_code::ok;
		}

		api_return handleReset(ApiRequest& aRequest) throw(exception) {
			reset();
			return websocketpp::http::status_code::ok;
		}

		void parseProperties(const json& j) {
			if (j.find("range_start") != j.end() && j.find("range_end") != j.end()) {
				auto start = j["range_start"];
				auto end = j["range_end"];

				if (start > end) {
					throw std::exception("Range start is after range end");
				}

				setRange(start, end);
			}

			if (j.find("sort_property") != j.end()) {
				auto prop = findPropertyByName(j["sort_property"], itemHandler.properties);
				if (prop == -1) {
					throw std::exception("Invalid sort property");
				}

				sortProperty = prop;
				sortFilterChanged = true;
			}

			if (j.find("sort_ascending") != j.end()) {
				sortAscending = j["sort_ascending"];
				sortFilterChanged = true;
			}

			if (j.find("paused") != j.end()) {
				bool paused = j["paused"];
				if (paused && timer->isRunning()) {
					timer->stop();
				} else if (!paused && !timer->isRunning()) {
					timer->start();
				}
			}
		}

		api_return handleDeleteFilter(ApiRequest& aRequest) throw(exception) {
			resetFilter();
			return websocketpp::http::status_code::ok;
		}

		void reset() noexcept {
			active = false;
			timer->stop();

			WLock l(cs);
			currentViewItems.clear();
			allItems.clear();
			filter.clear();
			tasks.clear();

			prevRangeEnd = -1;
			prevRangeStart = -1;
			prevTotalListItemCount = -1;
		}

		void onItemAdded(const T& aItem) {
			if (!active) return;

			WLock l(cs);
			auto prep = filter.prepare();
			if (!matchesFilter(aItem, prep)) {
				return;
			}

			addListItem(aItem);
		}

		void sendJson(const json& j) {
			if (j.is_null()) {
				return;
			}

			module->send(viewName + "_updated", j);
		}

		void onItemRemoved(const T& aItem) {
			if (!active) return;

			WLock l(cs);
			auto pos = findItem(aItem, allItems);
			if (pos != allItems.end()) {
				removeListItem(*pos);
			}
		}

		void onItemUpdated(const T& aItem, const PropertyIdSet& aUpdatedProperties) {
			if (!active) return;

			bool inList;
			{
				RLock l(cs);
				inList = isInList(aItem, allItems);
			}

			auto prep = filter.prepare();
			if (!matchesFilter(aItem, prep)) {
				if (inList) {
					removeListItem(aItem);
				}

				return;
			} else if (!inList) {
				addListItem(aItem);
				return;
			}

			updateListItem(aItem, aUpdatedProperties);
		}

		void onItemsUpdated(const ItemList& aItems, const PropertyIdSet& aUpdatedProperties) {
			if (!active) return;

			for (const auto& item : aItems) {
				onItemUpdated(item, aUpdatedProperties);
			}
		}

		void resetFilter() {
			{
				WLock l(cs);
				filter.clear();
			}

			onFilterUpdated();
		}

		void setFilter(const string& aPattern, int aMethod, int aProperty) {
			{
				WLock l(cs);
				filter.setFilterMethod(static_cast<StringMatch::Method>(aMethod));
				filter.setFilterProperty(aProperty);
				filter.setText(aPattern);
			}

			onFilterUpdated();
		}

		void onFilterUpdated() {
			sortFilterChanged = true;

			ItemList itemsNew;
			auto prep = filter.prepare();
			{
				for (const auto& i : itemHandler.itemListF()) {
					if (matchesFilter(i, prep)) {
						itemsNew.push_back(i);
					}
				}
			}

			{
				WLock l(cs);
				allItems.swap(itemsNew);
			}

			resetRange();
		}

		bool itemSort(const T& t1, const T& t2) {
			int res = 0;
			switch (itemHandler.properties[sortProperty].sortMethod) {
				case SORT_NUMERIC: {
					res = compare(itemHandler.numberF(t1, sortProperty), itemHandler.numberF(t2, sortProperty));
					break;
				}
				case SORT_TEXT: {
					res = Util::stricmp(itemHandler.stringF(t1, sortProperty).c_str(), itemHandler.stringF(t2, sortProperty).c_str());
					break;
				}
				case SORT_CUSTOM: {
					res = itemHandler.customSorterF(t1, t2, sortProperty);
					break;
				}
			}

			return sortAscending ? res < 0 : res > 0;
		}
	private:
		// RANGE START
		bool inRange(int pos) const noexcept {
			return pos >= rangeStart && pos <= rangeEnd;
		}

		void resetRange() {
			setRange(0, rangeEnd-rangeStart);
		}

		void setRange(int aStart, int aEnd) {
			rangeStart = aStart;
			rangeEnd = aEnd;
		}

		int getVisibleCount() {
			return min(static_cast<int>(allItems.size()), rangeEnd - rangeStart);
		}
		// RANGE END

		api_return handleGetItems(ApiRequest& aRequest) {
			auto start = aRequest.getIntParam(1);
			auto end = aRequest.getIntParam(2);
			decltype(allItems) allItemsCopy;

			{
				RLock l(cs);
				allItemsCopy = allItems;
			}

			auto j = Serializer::serializeFromPosition(start, end - start, allItemsCopy, [&](const T& i) {
				return Serializer::serializeItem(i, itemHandler);
			});

			aRequest.setResponseBody(j);
			return websocketpp::http::status_code::ok;
		}

		typename ItemList::iterator findItem(const T& aItem, ItemList& aItems) noexcept {
			return find_if(aItems.begin(), aItems.end(), [&](const T& i) { return aItem->getToken() == i->getToken(); });
		}

		typename ItemList::const_iterator findItem(const T& aItem, const ItemList& aItems) const noexcept {
			return find_if(aItems.begin(), aItems.end(), [&](const T& i) { return aItem->getToken() == i->getToken(); });
		}

		typename bool isInList(const T& aItem, const ItemList& aItems) const noexcept {
			return findItem(aItem, aItems) != aItems.end();
		}

		typename int64_t getPosition(const T& aItem, const ItemList& aItems) const noexcept {
			auto i = findItem(aItem, aItems);
			if (i == aItems.end()) {
				return -1;
			}

			return distance(aItems.begin(), i);
		}

		struct ItemTask : public Task {
			ItemTask(const T& aItem) : item(aItem) { }
			~ItemTask() { }

			const T item;
		};

		struct ItemUpdateTask : public ItemTask {
			ItemUpdateTask(const T& aItem, const PropertyIdSet& aUpdatedProperties) : ItemTask(aItem), updatedProperties(aUpdatedProperties) { }

			const PropertyIdSet updatedProperties;
		};

		void addListItem(const T& aItem) {
			tasks.add(ADD_ITEM, unique_ptr<Task>(new ItemTask(aItem)));
		}

		void updateListItem(const T& aItem, const PropertyIdSet& aUpdatedProperties) {
			tasks.add(UPDATE_ITEM, unique_ptr<Task>(new ItemUpdateTask(aItem, aUpdatedProperties)));
		}

		void removeListItem(const T& aItem) {
			tasks.add(REMOVE_ITEM, unique_ptr<Task>(new ItemTask(aItem)));
		}

		void runTasks() {
			TaskQueue::List tl;
			tasks.get(tl);

			int totalItemCount = 0;
			{
				WLock l(cs);
				totalItemCount = allItems.size();
				if (tl.empty() &&
					prevRangeEnd == rangeEnd &&
					prevRangeStart == rangeStart &&
					prevTotalListItemCount == totalItemCount &&
					!sortFilterChanged
					) {
						// Nothing to update
						return;
				}

				std::sort(allItems.begin(), allItems.end(), std::bind(&ListViewController::itemSort, this, std::placeholders::_1, std::placeholders::_2));
			}

			sortFilterChanged = false;

			// Go through the tasks
			std::map<T, const PropertyIdSet&> updatedItems;
			for (auto& t : tl) {
				switch (t.first) {
					case ADD_ITEM: {
						decltype(auto) task = static_cast<ItemTask&>(*t.second);
						handleAddItem(task.item);
						break;
					}
					case REMOVE_ITEM: {
						decltype(auto) task = static_cast<ItemTask&>(*t.second);
						handleRemoveItem(task.item);
						break;
					}
					case UPDATE_ITEM: {
						decltype(auto) task = static_cast<ItemUpdateTask&>(*t.second);
						updatedItems.emplace(task.item, task.updatedProperties);
						break;
					}
				}
			}

			// Get the new visible items
			decltype(currentViewItems) viewItemsNew, oldViewItems;
			{
				RLock l(cs);
				auto startIter = allItems.begin();
				advance(startIter, rangeStart);

				auto endIter = startIter;
				advance(endIter, min(static_cast<int>(allItems.size()), rangeEnd) - rangeStart);

				std::copy(startIter, endIter, back_inserter(viewItemsNew));
				oldViewItems = currentViewItems;
			}

			json j;

			// List items
			int pos = 0;
			for (const auto& item : viewItemsNew) {
				if (!isInList(item, oldViewItems)) {
					appendItem(item, j, pos);
				} else {
					// append position
					auto props = updatedItems.find(item);
					if (props != updatedItems.end()) {
						appendItem(item, j, pos, props->second);
						/*} else if (pos != getPosition(item, oldViewItems)) {
							appendItemPosition(item, j, pos);
						} else if (!j.is_null()) {
							// Mark that we have items after the appended one...
							j[pos] = nullptr;
						}*/
					} else {
						appendItemPosition(item, j, pos);
					}
				}

				pos++;
			}

			{
				WLock l(cs);
				currentViewItems.swap(viewItemsNew);
			}

			// Update total items
			if (prevTotalListItemCount != totalItemCount) {
				appendRowCount(j, totalItemCount);
				prevTotalListItemCount = totalItemCount;
			}

			// Update row range
			if (prevRangeStart != rangeStart || prevRangeEnd != rangeEnd) {
				appendRange(j);
				prevRangeStart = rangeStart;
				prevRangeEnd = rangeEnd;
			}

			sendJson(j);
		}

		void handleAddItem(const T& aItem) {
			WLock l(cs);
			//auto iter = std::lower_bound(allItems.begin(), allItems.end(), aItem, sortF);
			auto iter = findItem(aItem, allItems);
			if (iter != allItems.end() && *iter == aItem) {
				// Items that are updated right after adding may come here
				return;
			}

			auto pos = static_cast<int>(std::distance(allItems.begin(), iter));

			allItems.insert(iter, aItem);

			if (pos < rangeStart) {
				// Update the range range positions
				rangeStart++;
				rangeEnd++;
			}
		}

		void handleRemoveItem(const T& aItem) {
			WLock l(cs);
			auto iter = findItem(aItem, allItems);
			auto pos = static_cast<int>(std::distance(allItems.begin(), iter));

			allItems.erase(iter);

			if (pos < rangeStart) {
				if (getVisibleCount() <= (rangeEnd - rangeStart)) {
					// All items can fit the view
					resetRange();
				}

				// Update the range range positions
				rangeStart--;
				rangeEnd--;
			}
		}

		bool matchesFilter(const T& aItem, const PropertyFilter::Preparation& prep) {
			return filter.match(prep,
				[&](size_t aProperty) { return itemHandler.numberF(aItem, aProperty); },
				[&](size_t aProperty) { return itemHandler.stringF(aItem, aProperty); }
				);
		}

		// JSON APPEND START
		void appendItem(const T& aItem, json& json_, int pos) {
			appendItem(aItem, json_, pos, toPropertyIdSet(itemHandler.properties));
		}

		void appendItem(const T& aItem, json& json_, int pos, const PropertyIdSet& aPropertyIds) {
			appendItemPosition(aItem, json_, pos);
			json_["items"][pos]["properties"] = Serializer::serializeItemProperties(aItem, aPropertyIds, itemHandler);
		}

		void appendItemPosition(const T& aItem, json& json_, int pos) {
			json_["items"][pos]["id"] = aItem->getToken();
		}

		void appendRowCount(json& json_, int aCount) {
			json_["row_count"] = aCount;
		}

		void appendRange(json& json_) {
			json_["range_start"] = rangeStart;
			json_["range_end"] = rangeEnd;
		}

		PropertyFilter filter;
		const PropertyItemHandler<T>& itemHandler;

		ItemList currentViewItems;
		ItemList allItems;
		
		int sortProperty = 0;
		bool sortAscending = true;

		int rangeStart = 0;
		int rangeEnd = 0;

		int prevRangeStart = -1;
		int prevRangeEnd = -1;
		int prevTotalListItemCount = -1;

		bool sortFilterChanged = false;

		bool active = false;

		SharedMutex cs;

		ApiModule* module = nullptr;
		std::string viewName;

		enum Tasks {
			ADD_ITEM, UPDATE_ITEM, REMOVE_ITEM
		};

		// TODO: replace something better that would allow merging items
		// Also allow determining if the sort property has been updated for any item to avoid unnecessary sorts
		TaskQueue tasks;

		TimerPtr timer;
	};
}

#endif