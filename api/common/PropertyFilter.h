/*
* Copyright (C) 2011-2024 AirDC++ Project
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
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

#ifndef DCPP_PROPERTYFILTER_H
#define DCPP_PROPERTYFILTER_H

#include <airdcpp/core/thread/CriticalSection.h>

#include <api/common/Property.h>

namespace webserver {
	class PropertyFilter;
	using FilterToken = uint32_t;


	class PropertyFilter : public boost::noncopyable {
	public:
		using InfoFunction = std::function<std::string (int)>;
		using NumericFunction = std::function<double (int)>;
		using CustomFilterFunction = std::function<bool (int, const StringMatch &, double)>;

		using Ptr = shared_ptr<PropertyFilter>;
		using List = vector<Ptr>;


		// Helper class that will keep a reference to the filter
		// and prevent making changes to it during during matching (= lifetime of this object)
		template<typename FilterT>
		class Matcher {
		public:
			Matcher(const FilterT& aFilter) : filter(aFilter) {
				filter->cs.lock_shared();
			}

			~Matcher() {
				if (filter) {
					filter->cs.unlock_shared();
				}
			}

			Matcher(Matcher&& rhs) noexcept : filter(rhs.filter) {
				rhs.filter = nullptr;
			}

			Matcher(Matcher&) = delete;
			Matcher& operator=(Matcher&) = delete;
			Matcher& operator=(Matcher&& rhs) = delete;

			using MatcherT = Matcher<FilterT>;
			using List = vector<MatcherT>;
			static inline bool match(const List& prep, const NumericFunction& aNumericF, const InfoFunction& aStringF, const CustomFilterFunction& aCustomF) {
				return ranges::all_of(prep, [&](const Matcher& aMatcher) { 
					return aMatcher.filter->match(aNumericF, aStringF, aCustomF); 
				});
			}

			static inline bool match(const MatcherT& prep, const NumericFunction& aNumericF, const InfoFunction& aStringF, const CustomFilterFunction& aCustomF) {
				return prep.filter->match(aNumericF, aStringF, aCustomF);
			}
		private:
			FilterT filter;
		};

		using MatcherList = vector<Matcher<PropertyFilter::Ptr>>;

		explicit PropertyFilter(const PropertyList& aPropertyTypes);

		void prepare(const string& aPattern, int aMethod, int aProperty);

		bool empty() const noexcept;
		void clear() noexcept;

		void setInverse(bool aInverse) noexcept;
		bool getInverse() const noexcept { return inverse; }

		FilterToken getId() const noexcept {
			return id;
		}

	private:
		friend class Preparation;

		mutable SharedMutex cs;
		bool match(const NumericFunction& numericF, const InfoFunction& infoF, const CustomFilterFunction& aCustomF) const;
		bool matchText(int aProperty, const InfoFunction& infoF) const;
		bool matchNumeric(int aProperty, const NumericFunction& infoF) const;
		bool matchAnyColumn(const NumericFunction& numericF, const InfoFunction& infoF) const;

		void setPattern(const std::string& aText) noexcept;
		void setFilterProperty(int aFilterProperty) noexcept;
		void setFilterMethod(StringMatch::Method aFilterMethod) noexcept;

		const FilterToken id;
		PropertyList propertyTypes;

		pair<double, bool> prepareSize() const noexcept;
		pair<double, bool> prepareTime() const noexcept;
		pair<double, bool> prepareSpeed() const noexcept;

		StringMatch::Method defMethod = StringMatch::PARTIAL;
		int currentFilterProperty;
		FilterPropertyType type = FilterPropertyType::TYPE_TEXT;

		const int propertyCount;

		StringMatch matcher;
		double numericMatcher = 0;

		// Hide matching items
		bool inverse = false;

		// Filtering mode was typed into filtering expression
		bool usingTypedMethod = false;

		enum FilterMode {
			EQUAL,
			GREATER_EQUAL,
			LESS_EQUAL,
			GREATER,
			LESS,
			NOT_EQUAL,
			LAST
		};

		FilterMode numComparisonMode = FilterMode::LAST;
	};
}

#endif