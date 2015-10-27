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

#include <web-server/WebUserManager.h>
#include <web-server/WebServerManager.h>

#include <airdcpp/typedefs.h>
#include <airdcpp/Util.h>

namespace webserver {
	WebUserManager::WebUserManager() {
		//auto user = make_shared<WebUser>("test", "test");
		//users.emplace("test", make_shared<WebUser>("test", "test"));

		//auto session = make_shared<Session>(user, /*Util::toString(Util::rand())*/ "581774371", false);
		//sessions.emplace(session->getToken(), session);
	}

	SessionPtr WebUserManager::authenticate(const string& aUserName, const string& aPassword, bool aIsSecure) noexcept {
		WLock l(cs);

		auto u = users.find(aUserName);
		if (u == users.end()) {
			return nullptr;
		}

		if (u->second->getPassword() != aPassword) {
			return nullptr;
		}

		auto session = make_shared<Session>(u->second, Util::toString(Util::rand()), aIsSecure);
		sessions.emplace(session->getToken(), session);
		return session;
	}

	SessionPtr WebUserManager::getSession(const string& aSession) const noexcept {
		RLock l(cs);
		auto s = sessions.find(aSession);

		if (s == sessions.end()) {
			return nullptr;
		}

		return s->second;
	}

	void WebUserManager::logout(const SessionPtr& aSession) {
		WLock l(cs);
		sessions.erase(aSession->getToken());
	}

	void WebUserManager::on(TimerManagerListener::Minute, uint64_t aTick) noexcept{
		StringList removedTokens;

		{
			RLock l(cs);
			for (const auto& s: sessions | map_values) {
				if (s->getLastActivity() + 1000 * 60 * 120 < aTick) {
					removedTokens.push_back(s->getToken());
				}
			}
		}

		removedTokens.erase(remove_if(removedTokens.begin(), removedTokens.end(), [](const string& aToken) {
			return !WebServerManager::getInstance()->getSocket(aToken);
		}), removedTokens.end());

		if (!removedTokens.empty()) {
			WLock l(cs);
			for (const auto& token : removedTokens) {
				sessions.erase(token);
			}
		}
	}

	void WebUserManager::save(SimpleXML& xml_) const noexcept {
		xml_.addTag("WebUsers");
		xml_.stepIn();
		{
			RLock l(cs);
			for (auto& u : users | map_values) {
				xml_.addTag("WebUser");
				xml_.addChildAttrib("Username", u->getUserName());
				xml_.addChildAttrib("Password", u->getPassword());
			}
		}
		xml_.stepOut();
	}

	void WebUserManager::load(SimpleXML& xml_) noexcept {
		if (xml_.findChild("WebUsers")) {
			xml_.stepIn();
			while (xml_.findChild("WebUser")) {
				const string& username = xml_.getChildAttrib("Username");
				const string& password = xml_.getChildAttrib("Password");

				if (username.empty() || password.empty()) {
					continue;
				}

				users.emplace(username, make_shared<WebUser>(username, password));

			}
			xml_.stepOut();
		}

		xml_.resetCurrentChild();
	}

	bool WebUserManager::hasUsers() const noexcept {
		RLock l(cs);
		return !users.empty();
	}
}