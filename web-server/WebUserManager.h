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

#ifndef DCPLUSPLUS_DCPP_WEBUSERMANAGER_H
#define DCPLUSPLUS_DCPP_WEBUSERMANAGER_H

#include <web-server/stdinc.h>

#include <client/CriticalSection.h>
#include <client/SimpleXML.h>
#include <client/Singleton.h>
#include <client/TimerManager.h>

#include <web-server/WebUser.h>
#include <web-server/Session.h>

namespace webserver {

	class WebUserManager : public dcpp::Singleton<WebUserManager>, private TimerManagerListener {
	public:
		WebUserManager();

		SessionPtr authenticate(const string& aUserName, const string& aPassword, bool aIsSecure) noexcept;

		SessionPtr getSession(const string& aSession) noexcept;
		void logout(const SessionPtr& aSession);

		void load(SimpleXML& xml_) noexcept;
		void save(SimpleXML& xml_) const noexcept;
	private:
		mutable SharedMutex cs;

		std::map<std::string, WebUserPtr> users;
		std::map<std::string, SessionPtr> sessions;

		void on(TimerManagerListener::Minute, uint64_t aTick) noexcept;
	};
}

#endif