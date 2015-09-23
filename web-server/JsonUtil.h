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

#ifndef DCPLUSPLUS_DCPP_JSONUTIL_H
#define DCPLUSPLUS_DCPP_JSONUTIL_H

#include <web-server/stdinc.h>

namespace webserver {
	class JsonUtil {
	public:
		enum ErrorType {
			ERROR_MISSING,
			ERROR_INVALID,
			ERROR_EXISTS,
			ERROR_LAST
		};

		template <typename T, typename JsonT>
		static optional<T> getOptionalField(const string& aFieldName, const JsonT& aJson, bool aAllowEmpty = true, bool throwIfMissing = false) {
			if (throwIfMissing) {
				return getField<T>(aFieldName, aJson, aAllowEmpty);
			}

			auto p = aJson.find(aFieldName);
			if (p == aJson.end()) {
				return boost::none;
			}

			return parseValue<T>(aFieldName, *p, aAllowEmpty);
		}

		template <typename T, typename JsonT>
		static T getField(const string& aFieldName, const JsonT& aJson, bool aAllowEmpty = true) {
			auto p = aJson.find(aFieldName);
			if (p == aJson.end()) {
				throwError(aFieldName, ERROR_MISSING, "Field missing");
			}

			return parseValue<T>(aFieldName, *p, aAllowEmpty);
		}

		template <typename T, typename JsonT>
		static T parseValue(const string& aFieldName, const JsonT& aJson, bool aAllowEmpty = true) {
			if (!aJson.is_null()) {
				T ret;
				try {
					ret = aJson.get<T>();
				}
				catch (const exception& e) {
					throwError(aFieldName, ERROR_INVALID, e.what());
				}

				return ret;
			}

			if (!aAllowEmpty) {
				throwError(aFieldName, ERROR_INVALID, "Field can't be empty");
			}

			return convertNullValue<T>(aFieldName);
		}

		static void throwError(const string& aFieldName, ErrorType aType, const string& aMessage)  {
			throw ArgumentException(getError(aFieldName, aType, aMessage).c_str());
		}

		static string getError(const string& aFieldName, ErrorType aType, const string& aMessage) noexcept;
	private:
		// Convert null strings, add more conversions if needed
		template <class T>
		static typename std::enable_if<std::is_same<std::string, T>::value, T>::type convertNullValue(const string&) {
			return "";
		}

		template <class T>
		static typename std::enable_if<std::is_pointer<T>::value, T>::type convertNullValue(const string&) {
			return nullptr;
		}

		template <class T>
		static typename std::enable_if<!std::is_same<std::string, T>::value && !std::is_pointer<T>::value, T>::type convertNullValue(const string& aFieldName) {
			throw ArgumentException(getError(aFieldName, ERROR_INVALID, "Field can't be empty"));
		}
	};
}

#endif