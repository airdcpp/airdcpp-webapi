/*
* Copyright (C) 2011-2017 AirDC++ Project
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

#ifndef DCPLUSPLUS_DCPP_SETTING_UTILS_H
#define DCPLUSPLUS_DCPP_SETTING_UTILS_H

#include <web-server/stdinc.h>
#include <api/ApiSettingItem.h>

namespace webserver {
	class SettingUtils {
	public:
		static json validateValue(const ApiSettingItem& aItem, const json& aValue);
		static json validateObjectListValue(const ApiSettingItem::PtrList& aPropertyDefinitions, const json& aValue);

		static ServerSettingItem deserializeDefinition(const json& aJson);
		static ServerSettingItem::List deserializeDefinitions(const json& aJson);

		static json serializeDefinition(const ApiSettingItem& aItem) noexcept;
		static string typeToStr(ApiSettingItem::Type aType) noexcept;

		static ApiSettingItem::Type parseType(const string& aTypeStr) noexcept;
	private:
		static json parseEnumId(const json& aJson, ApiSettingItem::Type aType);
	};
}

#endif