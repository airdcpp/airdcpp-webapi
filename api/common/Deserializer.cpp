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

#include <web-server/stdinc.h>
#include <web-server/JsonUtil.h>

#include <api/common/Deserializer.h>

#include <airdcpp/ClientManager.h>

namespace webserver {
	CID Deserializer::deserializeCID(const string& aCID) throw(exception) {
		if (!Encoder::isBase32(aCID.c_str())) {
			throw exception("Invalid CID");
		}

		return CID(aCID);
	}

	UserPtr Deserializer::deserializeUser(const json& aJson) throw(exception) {
		auto cidStr = JsonUtil::getField<string>("cid", aJson, false);
		return ClientManager::getInstance()->findUser(deserializeCID(cidStr));
	}

	HintedUser Deserializer::deserializeHintedUser(const json& aJson) throw(exception) {
		return HintedUser(deserializeUser(aJson), JsonUtil::getField<string>("hub_url", aJson, false));
	}

	TTHValue Deserializer::deserializeTTH(const json& aJson) throw(exception) {
		auto tthStr = JsonUtil::getField<string>("tth", aJson, false);
		if (!Encoder::isBase32(tthStr.c_str())) {
			throw exception("Invalid TTH");
		}

		return TTHValue(tthStr);
	}

	QueueItemBase::Priority Deserializer::deserializePriority(const json& aJson, bool allowDefault) throw(exception) {
		int minAllowed = allowDefault ? QueueItemBase::DEFAULT : QueueItemBase::PAUSED_FORCE;

		auto priority = JsonUtil::getEnumField<int>("priority", aJson, !allowDefault, minAllowed, QueueItemBase::HIGHEST);
		if (!priority) {
			return QueueItemBase::Priority::DEFAULT;
		}

		return static_cast<QueueItemBase::Priority>(*priority);
	}

	void Deserializer::deserializeDownloadParams(const json& aJson, string& target_, TargetUtil::TargetType& targetType_, QueueItemBase::Priority& priority_) {
		auto target = JsonUtil::getOptionalField<string>("target", aJson);
		if (!target) {
			target = SETTING(DOWNLOAD_DIRECTORY);
		}

		auto targetType = JsonUtil::getEnumField<int>("target_type", aJson, false, 0, TargetUtil::TARGET_LAST-1);
		if (!targetType) {
			targetType = TargetUtil::TARGET_PATH;
		}

		priority_ = deserializePriority(aJson, true);
		target_ = *target;
		targetType_ = static_cast<TargetUtil::TargetType>(*targetType);;
	}
}