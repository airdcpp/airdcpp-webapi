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

#ifndef DCPLUSPLUS_WEBSERVER_EXTENSIONMANAGER_H
#define DCPLUSPLUS_WEBSERVER_EXTENSIONMANAGER_H

#include "forward.h"

#include <airdcpp/core/thread/CriticalSection.h>
#include <airdcpp/message/Message.h>
#include <airdcpp/core/Singleton.h>
#include <airdcpp/core/Speaker.h>
#include <airdcpp/core/update/UpdateManagerListener.h>

#include <web-server/ExtensionListener.h>
#include <web-server/ExtensionManagerListener.h>
#include <web-server/SocketManagerListener.h>
#include <web-server/WebServerManagerListener.h>

namespace dcpp {
	struct HttpDownload;
}

namespace webserver {
	class NpmRepository;

	struct ExtensionEngine {
		string name;
		string command;
		StringList arguments;

		using List = vector<ExtensionEngine>;
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ExtensionEngine, name, command, arguments);

	class ExtensionManager: public Speaker<ExtensionManagerListener>, private WebServerManagerListener, private UpdateManagerListener, private ExtensionListener, private SocketManagerListener {
	public:
		explicit ExtensionManager(WebServerManager* aWsm);
		~ExtensionManager() override;

		// Load and start all managed extensions from disk
		void load() noexcept;

		void checkExtensionUpdates() const noexcept;

		// Wait for the extensions to be ready
		// (allow them to connect the socket, add listeners etc.)
		bool waitLoaded() const noexcept;

		// Download extension from the given URL and install it
		// SHA1 checksum is optional
		// Returns false if the extension is being downloaded already
		bool downloadExtension(const string& aInstallId, const string& aUrl, const string& aSha1) noexcept;

		// Install extensions from the given tarball
		void installLocalExtension(const string& aInstallId, const string& aPath) noexcept;

		// Register non-local extension
		ExtensionPtr registerRemoteExtensionThrow(const SessionPtr& aSession, const json& aPackageJson);
		void unregisterRemoteExtension(const ExtensionPtr& aExtension) noexcept;

		// Remove extension from disk
		// Throws FileException on disk errors and Exception on other errors
		void uninstallLocalExtensionThrow(const ExtensionPtr& aExtension, bool aForced = false);

		ExtensionPtr getExtension(const string& aName) const noexcept;
		ExtensionList getExtensions() const noexcept;

		// Get the engine start command for extension
		// Throws on errors
		struct ExtensionLaunchInfo {
			string command;
			StringList arguments;
		};

		ExtensionLaunchInfo getStartCommandThrow(const StringList& aSupportedExtEngines, const ExtensionEngine::List& aInstalledEngines) const;

		ExtensionEngine::List getEngines() const noexcept;

		// Parses the engine command param (command1;command2;...) and tests each token for an existing application
		static string selectEngineCommand(const string& aEngineCommands) noexcept;

		NpmRepository& getNpmRepository() noexcept {
			return *npmRepository.get();
		}

		ExtensionManager(ExtensionManager&) = delete;
		ExtensionManager& operator=(ExtensionManager&) = delete;
	private:
		bool removeExtension(const ExtensionPtr& aExtension) noexcept;
		void onExtensionStateUpdated(const Extension* aExtension) noexcept;
		void onExtensionFailed(const Extension* aExtension, uint32_t aExitCode) noexcept;
		bool startExtensionImpl(const ExtensionPtr& aExtension, const ExtensionEngine::List& aInstalledEngines) noexcept;

		using BlockedExtensionMap = map<string, string>;
		BlockedExtensionMap blockedExtensions;

		void uninstallBlockedExtensions() noexcept;

		static bool validateSha1(const string& aData, const string& aSha1) noexcept;
		void onExtensionDownloadCompleted(const string& aInstallId, const string& aUrl, const string& aSha1) noexcept;
		void failInstallation(const string& aInstallId, const string& aMessage, const string& aException) noexcept;

		using HttpDownloadMap = map<string, shared_ptr<HttpDownload>>;
		HttpDownloadMap httpDownloads;

		unique_ptr<NpmRepository> npmRepository;

		mutable SharedMutex cs;

		// Load extension from the supplied path and store in the extension list
		// Returns the extension pointer in case it was parsed succesfully
		ExtensionPtr loadLocalExtension(const string& aPath) noexcept;

		ExtensionList extensions;

		WebServerManager* wsm;

		void on(WebServerManagerListener::Started) noexcept override;
		void on(WebServerManagerListener::Stopping) noexcept override;
		void on(WebServerManagerListener::Stopped) noexcept override;

		void on(SocketManagerListener::SocketDisconnected, const WebSocketPtr& aSocket) noexcept override;

		void on(ExtensionListener::ExtensionStarted, const Extension*) noexcept override;
		void on(ExtensionListener::ExtensionStopped, const Extension*, bool /*aFailed*/) noexcept override;

		void on(ExtensionListener::SettingValuesUpdated, const Extension*, const SettingValueMap&) noexcept override;
		void on(ExtensionListener::SettingDefinitionsUpdated, const Extension*) noexcept override;
		void on(ExtensionListener::PackageUpdated, const Extension*) noexcept override;

		void on(UpdateManagerListener::VersionFileDownloaded, SimpleXML& aXml) noexcept override;

		void log(const string& aMsg, LogMessage::Severity aSeverity) const noexcept;

		TimerPtr updateCheckTask = nullptr;
	};
}

#endif