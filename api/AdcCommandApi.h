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

#ifndef DCPLUSPLUS_DCPP_ADC_COMMAND_API_H
#define DCPLUSPLUS_DCPP_ADC_COMMAND_API_H

#include <api/base/FilterableHookApiModule.h>
#include <api/HubInfo.h>

#include <airdcpp/core/header/typedefs.h>

#include <airdcpp/protocol/AdcCommand.h>
#include <airdcpp/protocol/AdcSupports.h>
#include <airdcpp/hub/Client.h>
#include <airdcpp/hub/ClientManagerListener.h>
#include <airdcpp/protocol/ProtocolCommandManager.h>

namespace webserver {
	class AdcCommandApi : public FilterableHookApiModule<AdcCommand::CommandType>, private ClientManagerListener, private ProtocolCommandManagerListener {
	public:
		static StringList subscriptionList;

		explicit AdcCommandApi(Session* aSession);
		~AdcCommandApi() override;

		struct NamedAdcParam {
			string name;
			string value;
		};

		static json serializeAdcCommand(const AdcCommand& aCmd) noexcept;
		static json serializeTo(const AdcCommand& aCmd, const Client&) noexcept;
		static json serializeFrom(const AdcCommand& aCmd, const Client&) noexcept;
		static json serializeUser(dcpp::SID aSID, const Client&) noexcept;
		static json serializeUserConnection(const UserConnection& aUserConnection) noexcept;

		static string deserializeSupportString(const json& aCmd, const string& aFieldName);
		static AdcCommand::CommandType deserializeCommandField(const json& aCmd, const string& aFieldName);

		static AdcCommand::CommandType parseCommand(const string& aCommandStr);
		static string serializeCommand(const AdcCommand::CommandType& aType);

		static AdcCommand::ParamMap deserializeHookParams(const json& aJson, const ActionHookResultGetter<AdcCommand::ParamMap>& aResultGetter);
		static AdcCommand::ParamMap deserializeNamedParams(const json& aJson, bool aAllowEmpty);
		static AdcCommand::ParamList deserializeIndexedParams(const json& aJson, bool aAllowEmpty);
		static NamedAdcParam deserializeNamedParam(const json& aJson, const string& aFieldName);

		static AdcCommand deserializeCommand(const json& aJson);

		static void deserializeCommandFeatures(const json& aJson, AdcCommand& aCmd);
		static void deserializeCommandRecipient(const json& aJson, AdcCommand& aCmd, const ClientPtr& aClient);
	private:
		class SupportHandler {
		public:
			explicit SupportHandler(AdcSupports& aSupportStore) : supportStore(aSupportStore) {}

			api_return handleAddSupport(ApiRequest& aRequest);
			api_return handleRemoveSupport(ApiRequest& aRequest);
		private: 
			AdcSupports& supportStore;
		};

		SupportHandler hubSupports;
		SupportHandler hubUserSupports;
		SupportHandler userConnectionSupports;

		api_return handlePostHubCommand(ApiRequest& aRequest);
		api_return handlePostUDPCommand(ApiRequest& aRequest);
		api_return handlePostTCPCommand(ApiRequest& aRequest);

		static json serializeOutgoingHubCommand(const AdcCommand& aCmd, const Client& aClient) noexcept;
		static json serializeOutgoingUDPCommand(const AdcCommand& aCmd, const OnlineUserPtr& aUser) noexcept;
		static json serializeOutgoingTCPCommand(const AdcCommand& aCmd, const UserConnection& aUserConnection) noexcept;

		ActionHookResult<AdcCommand::ParamMap> outgoingHubMessageHook(const AdcCommand& aCmd, const Client& aClient, const ActionHookResultGetter<AdcCommand::ParamMap>& aResultGetter);
		ActionHookResult<AdcCommand::ParamMap> outgoingUdpMessageHook(const AdcCommand& aCmd, const OnlineUserPtr& aUser, const string& aIpPort, const ActionHookResultGetter<AdcCommand::ParamMap>& aResultGetter);
		ActionHookResult<AdcCommand::ParamMap> outgoingTcpMessageHook(const AdcCommand& aCmd, const UserConnection& aUserConnection, const ActionHookResultGetter<AdcCommand::ParamMap>& aResultGetter);

		void on(ProtocolCommandManagerListener::IncomingHubCommand, const AdcCommand&, const Client&) noexcept override;
		void on(ProtocolCommandManagerListener::IncomingUDPCommand, const AdcCommand&, const string&) noexcept override;
		void on(ProtocolCommandManagerListener::IncomingTCPCommand, const AdcCommand&, const string&, const HintedUser&) noexcept override;

		void on(ProtocolCommandManagerListener::OutgoingHubCommand, const AdcCommand&, const Client&) noexcept override;
		void on(ProtocolCommandManagerListener::OutgoingUDPCommand, const AdcCommand&, const string&, const OnlineUserPtr&) noexcept override;
		void on(ProtocolCommandManagerListener::OutgoingTCPCommand, const AdcCommand&, const UserConnection& aUserConnection) noexcept override;

		static boost::regex paramReg;
		static boost::regex commandReg;
		static boost::regex supportReg;
	};
}

#endif