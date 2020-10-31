/*
* Copyright (C) 2011-2019 AirDC++ Project
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

#include "stdinc.h"

#include <web-server/WebUserManager.h>
#include <web-server/WebServerManager.h>
#include <web-server/WebServerSettings.h>

#include <airdcpp/typedefs.h>

#include <airdcpp/ActivityManager.h>
#include <airdcpp/SimpleXML.h>
#include <airdcpp/TimerManager.h>
#include <airdcpp/Util.h>

#ifdef _WIN32
#include <Wincrypt.h>
#endif

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/range/algorithm/count_if.hpp>


#define FLOOD_COUNT 5
#define FLOOD_PERIOD 45

#define REFRESH_TOKEN_VALIDITY_DAYS 30ULL

namespace webserver {
	WebUserManager::WebUserManager(WebServerManager* aServer) : server(aServer), authFloodCounter(FLOOD_COUNT, FLOOD_PERIOD) {
		aServer->addListener(this);
	}

	WebUserManager::~WebUserManager() {
		server->removeListener(this);
	}

	SessionPtr WebUserManager::parseHttpSession(const string& aAuthToken, const string& aIP) {
		auto token = aAuthToken;

		auto authType = AUTH_UNKNOWN;
		if (token.length() > 6 && token.substr(0, 6) == "Basic ") {
			token = websocketpp::base64_decode(token.substr(6));
			authType = AUTH_BASIC;
		} else if (token.length() > 7 && token.substr(0, 7) == "Bearer ") {
			token = token.substr(7);
			authType = AUTH_BEARER;
		} else {
			authType = AUTH_BEARER;
		}

		auto session = getSession(token);
		if (!session) {
			if (authType == AUTH_BASIC) {
				string username, password;

				auto i = token.rfind(':');
				if (i == string::npos) {
					throw std::domain_error("Invalid authorization token format");
				}

				username = token.substr(0, i);
				password = token.substr(i + 1);

				return authenticateSession(username, password, Session::TYPE_BASIC_AUTH, 60, aIP, token);
			} else {
				throw std::domain_error(STRING(WEB_SESSIONS_INVALID_TOKEN));
			}
		}

		return session;
	}

	SessionPtr WebUserManager::authenticateSession(const string& aUserName, const string& aPassword, Session::SessionType aType, uint64_t aMaxInactivityMinutes, const string& aIP) {
		auto uuid = boost::uuids::to_string(boost::uuids::random_generator()());
		return authenticateSession(aUserName, aPassword, aType, aMaxInactivityMinutes, aIP, uuid);
	}

	SessionPtr WebUserManager::authenticateSession(const string& aRefreshToken, Session::SessionType aType, uint64_t aMaxInactivityMinutes, const string& aIP) {
		WebUserPtr user = nullptr;

		{
			RLock l(cs);
			auto i = refreshTokens.find(aRefreshToken);
			if (i == refreshTokens.end()) {
				throw std::domain_error("Invalid refresh token");
			}

			user = i->second.user;
		}

		{
			WLock l(cs);
			refreshTokens.erase(aRefreshToken);
		}

		setDirty();

		auto uuid = boost::uuids::to_string(boost::uuids::random_generator()());
		return createSession(user, uuid, aType, aMaxInactivityMinutes, aIP);
	}

	SessionPtr WebUserManager::authenticateSession(const string& aUserName, const string& aPassword, Session::SessionType aType, uint64_t aMaxInactivityMinutes, const string& aIP, const string& aSessionToken) {
		if (!authFloodCounter.checkFlood(aIP)) {
			server->log(STRING_F(WEB_SERVER_MULTIPLE_FAILED_ATTEMPTS, aIP), LogMessage::SEV_WARNING);
			throw std::domain_error(STRING(WEB_SESSIONS_TOO_MANY_ATTEMPTS));
		}

		auto user = getUser(aUserName);
		if (!user || user->getPassword() != aPassword) {
			authFloodCounter.addAttempt(aIP);
			throw std::domain_error(STRING(WEB_SESSIONS_INVALID_USER_PW));
		}

		return createSession(user, aSessionToken, aType, aMaxInactivityMinutes, aIP);
	}

	SessionPtr WebUserManager::createSession(const WebUserPtr& aUser, const string& aSessionToken, Session::SessionType aType, uint64_t aMaxInactivityMinutes, const string& aIP) noexcept {
		dcassert(aType != Session::TYPE_BASIC_AUTH || aSessionToken.find(':') != string::npos);

		auto session = std::make_shared<Session>(aUser, aSessionToken, aType, server, aMaxInactivityMinutes, aIP);

		aUser->setLastLogin(GET_TIME());
		aUser->addSession();

		if (aType != Session::TYPE_EXTENSION) {
			fire(WebUserManagerListener::UserUpdated(), aUser);
			setDirty();
		}

		{
			WLock l(cs);

			// Single session per user when using basic auth
			dcassert(aType != Session::TYPE_BASIC_AUTH || boost::find_if(sessionsRemoteId | map_values, [&](const SessionPtr& aSession) {
				return aSession->getSessionType() == Session::TYPE_BASIC_AUTH && aSession->getUser() == aUser;
			}).base() == sessionsRemoteId.end());

			sessionsRemoteId.emplace(session->getAuthToken(), session);
			sessionsLocalId.emplace(session->getId(), session);
		}

		fire(WebUserManagerListener::SessionCreated(), session);
		return session;
	}

	SessionPtr WebUserManager::createExtensionSession(const string& aExtensionName) noexcept {
		auto uuid = boost::uuids::to_string(boost::uuids::random_generator()());

		// For internal use only (can't be used for logging in)
		auto user = std::make_shared<WebUser>(aExtensionName, Util::emptyString, true);

		return createSession(user, uuid, Session::TYPE_EXTENSION, WEBCFG(DEFAULT_SESSION_IDLE_TIMEOUT).uint64(), "localhost");
	}

	SessionList WebUserManager::getSessions() const noexcept {
		SessionList ret;

		{
			RLock l(cs);
			boost::range::copy(sessionsLocalId | map_values, back_inserter(ret));
		}

		return ret;
	}

	SessionPtr WebUserManager::getSession(const string& aSession) const noexcept {
		RLock l(cs);
		auto s = sessionsRemoteId.find(aSession);
		if (s == sessionsRemoteId.end()) {
			return nullptr;
		}

		return s->second;
	}

	SessionPtr WebUserManager::getSession(LocalSessionId aId) const noexcept {
		RLock l(cs);
		auto s = sessionsLocalId.find(aId);
		if (s == sessionsLocalId.end()) {
			return nullptr;
		}

		return s->second;
	}

	size_t WebUserManager::getUserSessionCount() const noexcept {
		RLock l(cs);
		return boost::count_if(sessionsLocalId | map_values, [=](const SessionPtr& s) {
			return s->getSessionType() != Session::TYPE_EXTENSION;
		});
	}

	void WebUserManager::logout(const SessionPtr& aSession) {
		removeSession(aSession, false);

		auto socket = server->getSocket(aSession->getId());
		if (socket) {
			resetSocketSession(socket);
		} else {
			dcdebug("No socket for session %s\n", aSession->getAuthToken().c_str());
		}

		dcdebug("Session %s logging out, use count: %ld\n", aSession->getAuthToken().c_str(), aSession.use_count());
	}

	void WebUserManager::resetSocketSession(const WebSocketPtr& aSocket) noexcept {
		if (aSocket->getSession()) {
			dcdebug("Resetting socket for session %s\n", aSocket->getSession()->getAuthToken().c_str());
			aSocket->getSession()->onSocketDisconnected();
			aSocket->setSession(nullptr);
		}
	}

	void WebUserManager::checkExpiredSessions() noexcept {
		SessionList removedSession;
		auto tick = GET_TICK();

		{
			RLock l(cs);
			boost::algorithm::copy_if(sessionsLocalId | map_values, back_inserter(removedSession), [=](const SessionPtr& s) {
				return s->getMaxInactivity() > 0 && s->getLastActivity() + s->getMaxInactivity() < tick;
			});
		}

		for (const auto& s : removedSession) {
			// Don't remove sessions with active socket
			if (!server->getSocket(s->getId())) {
				removeSession(s, true);
			}
		}
	}

	void WebUserManager::checkExpiredTokens() noexcept {
		TokenInfo::List removedTokens;
		auto time = GET_TIME();

		{
			RLock l(cs);
			boost::algorithm::copy_if(refreshTokens | map_values, back_inserter(removedTokens), [=](const TokenInfo& ti) {
				return time > ti.expiresOn;
			});
		}

		{
			WLock l(cs);
			for (const auto& t : removedTokens) {
				refreshTokens.erase(t.token);
			}
		}
	}

	void WebUserManager::removeSession(const SessionPtr& aSession, bool aTimedOut) noexcept {
		aSession->getUser()->removeSession();
		fire(WebUserManagerListener::UserUpdated(), aSession->getUser());

		{
			WLock l(cs);
			sessionsRemoteId.erase(aSession->getAuthToken());
			sessionsLocalId.erase(aSession->getId());
		}

		fire(WebUserManagerListener::SessionRemoved(), aSession, aTimedOut);
	}

	void WebUserManager::on(WebServerManagerListener::Started) noexcept {
		expirationTimer = server->addTimer([this] { 
			checkExpiredSessions();
			checkExpiredTokens();
			authFloodCounter.prune();
		}, FLOOD_PERIOD * 1000);

		expirationTimer->start(false);
	}

	void WebUserManager::on(WebServerManagerListener::Stopping) noexcept {

	}

	void WebUserManager::on(WebServerManagerListener::Stopped) noexcept {
		expirationTimer = nullptr;

		// Let the modules handle deletion in a clean way before we are shutting down...
		SessionList sessions;

		{
			WLock l(cs);
			boost::copy(sessionsLocalId | map_values, back_inserter(sessions));

			sessionsLocalId.clear();
			sessionsRemoteId.clear();
		}

		while (true) {
			if (all_of(sessions.begin(), sessions.end(), [](const SessionPtr& aSession) {
				return aSession.use_count() == 1;
			})) {
				break;
			}

			Thread::sleep(50);
		}
	}

	void WebUserManager::on(WebServerManagerListener::SocketDisconnected, const WebSocketPtr& aSocket) noexcept {
		resetSocketSession(aSocket);
	}

	void WebUserManager::on(WebServerManagerListener::LoadSettings, SimpleXML& xml_) noexcept {
		if (xml_.findChild("WebUsers")) {
			xml_.stepIn();
			while (xml_.findChild("WebUser")) {
				const auto& username = xml_.getChildAttrib("Username");
				const auto& password = xml_.getChildAttrib("Password");

				if (username.empty() || password.empty()) {
					continue;
				}

				const auto& permissions = xml_.getChildAttrib("Permissions");

				// Set as admin mainly for compatibility with old accounts if no permissions were found
				auto user = std::make_shared<WebUser>(username, password, permissions.empty());

				user->setLastLogin(xml_.getIntChildAttrib("LastLogin"));
				if (!permissions.empty()) {
					user->setPermissions(permissions);
				}

				users.emplace(username, user);

			}
			xml_.stepOut();
		}

		if (xml_.findChild("RefreshTokens")) {
			xml_.stepIn();
			while (xml_.findChild("TokenInfo")) {
				const auto& token = xml_.getChildAttrib("Token");
				const auto& username = xml_.getChildAttrib("Username");
				const auto& expiresOn = static_cast<time_t>(xml_.getLongLongChildAttrib("ExpiresOn"));

				if (username.empty() || token.empty() || GET_TIME() > expiresOn) {
					continue;
				}

				auto user = getUser(username);
				if (!user) {
					continue;
				}

				refreshTokens.emplace(token, TokenInfo({ token, user, expiresOn }));
			}
			xml_.stepOut();
		}

		xml_.resetCurrentChild();
	}

	void WebUserManager::on(WebServerManagerListener::SaveSettings, SimpleXML& xml_) noexcept {
		{
			xml_.addTag("WebUsers");
			xml_.stepIn();
			{
				RLock l(cs);
				for (const auto& u: users | map_values) {
					xml_.addTag("WebUser");
					xml_.addChildAttrib("Username", u->getUserName());
					xml_.addChildAttrib("Password", u->getPassword());
					xml_.addChildAttrib("LastLogin", u->getLastLogin());
					xml_.addChildAttrib("Permissions", u->getPermissionsStr());
				}
			}
			xml_.stepOut();
		}

		{
			xml_.addTag("RefreshTokens");
			xml_.stepIn();
			{
				RLock l(cs);
				for (const auto& t: refreshTokens | map_values) {
					xml_.addTag("TokenInfo");
					xml_.addChildAttrib("Token", t.token);
					xml_.addChildAttrib("Username", t.user->getUserName());
					xml_.addChildAttrib("ExpiresOn", t.expiresOn);
				}
			}
			xml_.stepOut();
		}
	}

	void WebUserManager::setDirty() noexcept {
		server->setDirty();
	}

	bool WebUserManager::hasUsers() const noexcept {
		RLock l(cs);
		return !users.empty();
	}

	bool WebUserManager::hasUser(const string& aUserName) const noexcept {
		RLock l(cs);
		return users.find(aUserName) != users.end();
	}

	bool WebUserManager::addUser(const WebUserPtr& aUser) noexcept {
		auto user = getUser(aUser->getUserName());
		if (user) {
			return false;
		}

		{
			WLock l(cs);
			users.emplace(aUser->getUserName(), aUser);
		}

		fire(WebUserManagerListener::UserAdded(), aUser);
		setDirty();
		return true;
	}

	WebUserPtr WebUserManager::getUser(const string& aUserName) const noexcept {
		RLock l(cs);
		auto user = users.find(aUserName);
		if (user == users.end()) {
			return nullptr;
		}

		return user->second;
	}

	string WebUserManager::createRefreshToken(const WebUserPtr& aUser) noexcept {
		const auto uuid = boost::uuids::to_string(boost::uuids::random_generator()());
		const time_t expiration = GET_TIME() + static_cast<time_t>(REFRESH_TOKEN_VALIDITY_DAYS * 24ULL * 60ULL * 60ULL * 1000ULL);

		{
			WLock l(cs);
			refreshTokens.emplace(uuid, TokenInfo({ uuid, aUser, expiration }));
		}

		setDirty();
		return uuid;
	}

	void WebUserManager::removeRefreshTokens(const WebUserPtr& aUser) noexcept {
		TokenInfo::List removedTokens;

		{
			RLock l(cs);
			boost::algorithm::copy_if(refreshTokens | map_values, back_inserter(removedTokens), [=](const TokenInfo& ti) {
				return ti.user == aUser;
			});
		}

		{
			WLock l(cs);
			for (const auto& t: removedTokens) {
				refreshTokens.erase(t.token);
			}
		}

		setDirty();
	}


	void WebUserManager::removeSessions(const WebUserPtr& aUser) noexcept {
		SessionList removedSession;

		{
			RLock l(cs);
			boost::algorithm::copy_if(sessionsLocalId | map_values, back_inserter(removedSession), [=](const SessionPtr& s) {
				return s->getUser() == aUser;
			});
		}

		for (const auto& s: removedSession) {
			auto socket = server->getSocket(s->getId());
			if (socket) {
				socket->close(websocketpp::close::status::normal, "Re-authentication required");
			}

			removeSession(s, true);
		}
	}

	bool WebUserManager::updateUser(const WebUserPtr& aUser, bool aRemoveSessions) noexcept {
		if (aRemoveSessions) {
			removeRefreshTokens(aUser);
			removeSessions(aUser);
		}

		fire(WebUserManagerListener::UserUpdated(), aUser);
		setDirty();
		return true;
	}

	bool WebUserManager::removeUser(const string& aUserName) noexcept {
		auto user = getUser(aUserName);
		if (!user) {
			return false;
		}

		removeRefreshTokens(user);
		removeSessions(user);

		{
			WLock l(cs);
			users.erase(aUserName);
		}

		fire(WebUserManagerListener::UserRemoved(), user);
		setDirty();
		return true;
	}

	StringList WebUserManager::getUserNames() const noexcept {
		StringList ret;

		RLock l(cs);
		boost::copy(users | map_keys, back_inserter(ret));
		return ret;
	}

	WebUserList WebUserManager::getUsers() const noexcept {
		WebUserList ret;

		RLock l(cs);
		boost::copy(users | map_values, back_inserter(ret));
		return ret;
	}

	void WebUserManager::replaceWebUsers(const WebUserList& newUsers) noexcept {
		{
			WLock l(cs);
			refreshTokens.clear();
			users.clear();
			for (auto u : newUsers) {
				users.emplace(u->getUserName(), u);
			}
		}

		setDirty();
	}
}