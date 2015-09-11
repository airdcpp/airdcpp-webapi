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

#include <api/Deserializer.h>

#include <client/ClientManager.h>

namespace webserver {
	UserPtr Deserializer::deserializeUser(const json& aJson) throw(exception) {
		std::string cidStr = aJson["cid"];
		if (!Encoder::isBase32(cidStr.c_str())) {
			throw exception("Invalid CID");
		}

		auto cid = CID(cidStr);
		return ClientManager::getInstance()->findUser(cid);
	}

	HintedUser Deserializer::deserializeHintedUser(const json& aJson) throw(exception) {
		return HintedUser(deserializeUser(aJson), aJson["hub_url"]);
	}

	TTHValue Deserializer::deserializeTTH(const json& aJson) throw(exception) {
		std::string tthStr = aJson["tth"];
		if (!Encoder::isBase32(tthStr.c_str())) {
			throw exception("Invalid TTH");
		}

		return TTHValue(tthStr);
	}

	QueueItemBase::Priority Deserializer::deserializePriority(const json& aJson, bool allowDefault) throw(exception) {
		int priority = aJson["priority"];

		int minAllowed = allowDefault ? QueueItemBase::DEFAULT : QueueItemBase::PAUSED_FORCE;
		if (priority < minAllowed || priority > QueueItemBase::HIGHEST) {
			throw exception("Invalid priority");
		}

		return static_cast<QueueItemBase::Priority>(priority);
	}
}