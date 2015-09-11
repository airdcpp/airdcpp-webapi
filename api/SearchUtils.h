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

#ifndef DCPLUSPLUS_DCPP_SEARCHUTILS_H
#define DCPLUSPLUS_DCPP_SEARCHUTILS_H

#include <api/SearchApi.h>

#include <web-server/stdinc.h>

#include <client/typedefs.h>
#include <client/SearchResult.h>

namespace webserver {
	class SearchUtils {
	public:
		static json serializeResult(const SearchApi::SearchInfoPtr& aResult, int aPropertyName) noexcept;

		static int compareResults(const SearchApi::SearchInfoPtr& a, const SearchApi::SearchInfoPtr& b, int aPropertyName) noexcept;
		static std::string  getStringInfo(const SearchApi::SearchInfoPtr& a, int aPropertyName) noexcept;
		static double getNumericInfo(const SearchApi::SearchInfoPtr& a, int aPropertyName) noexcept;
	};
}

#endif