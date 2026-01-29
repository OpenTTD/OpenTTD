/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tcp_admin.cpp Basic functions to receive and send TCP packets to and from the admin network. */

#include "../../stdafx.h"

#include "../network_internal.h"
#include "tcp_admin.h"
#include "../../debug.h"

#include "../../safeguards.h"

/* Make sure that these enums match. */
static_assert((int)CRR_MANUAL    == (int)ADMIN_CRR_MANUAL);
static_assert((int)CRR_AUTOCLEAN == (int)ADMIN_CRR_AUTOCLEAN);
static_assert((int)CRR_BANKRUPT  == (int)ADMIN_CRR_BANKRUPT);
static_assert((int)CRR_END       == (int)ADMIN_CRR_END);

NetworkRecvStatus NetworkAdminSocketHandler::CloseConnection(bool)
{
	delete this;
	return NETWORK_RECV_STATUS_CLIENT_QUIT;
}

/**
 * Handle the given packet, i.e. pass it to the right parser receive command.
 * @param p the packet to handle.
 * @return #NetworkRecvStatus of handling.
 */
NetworkRecvStatus NetworkAdminSocketHandler::HandlePacket(Packet &p)
{
	PacketAdminType type = static_cast<PacketAdminType>(p.Recv_uint8());

	switch (type) {
		case PacketAdminType::AdminJoin: return this->ReceiveAdminJoin(p);
		case PacketAdminType::AdminQuit: return this->ReceiveAdminQuit(p);
		case PacketAdminType::AdminUpdateFrequency: return this->ReceiveAdminUpdateFrequency(p);
		case PacketAdminType::AdminPoll: return this->ReceiveAdminPoll(p);
		case PacketAdminType::AdminChat: return this->ReceiveAdminChat(p);
		case PacketAdminType::AdminExternalChat: return this->ReceiveAdminExternalChat(p);
		case PacketAdminType::AdminRemoteConsoleCommand: return this->ReceiveAdminRemoteConsoleCommand(p);
		case PacketAdminType::AdminGameScript: return this->ReceiveAdminGameScript(p);
		case PacketAdminType::AdminPing: return this->ReceiveAdminPing(p);
		case PacketAdminType::AdminJoinSecure: return this->ReceiveAdminJoinSecure(p);
		case PacketAdminType::AdminAuthenticationResponse: return this->ReceiveAdminAuthenticationResponse(p);

		case PacketAdminType::ServerFull: return this->ReceiveServerFull(p);
		case PacketAdminType::ServerBanned: return this->ReceiveServerBanned(p);
		case PacketAdminType::ServerError: return this->ReceiveServerError(p);
		case PacketAdminType::ServerProtocol: return this->ReceiveServerProtocol(p);
		case PacketAdminType::ServerWelcome: return this->ReceiveServerWelcome(p);
		case PacketAdminType::ServerNewGame: return this->ReceiveServerNewGame(p);
		case PacketAdminType::ServerShutdown: return this->ReceiveServerShutdown(p);

		case PacketAdminType::ServerDate: return this->ReceiveServerDate(p);
		case PacketAdminType::ServerClientJoin: return this->ReceiveServerClientJoin(p);
		case PacketAdminType::ServerClientInfo: return this->ReceiveServerClientInfo(p);
		case PacketAdminType::ServerClientUpdate: return this->ReceiveServerClientUpdate(p);
		case PacketAdminType::ServerClientQuit: return this->ReceiveServerClientQuit(p);
		case PacketAdminType::ServerClientError: return this->ReceiveServerClientError(p);
		case PacketAdminType::ServerCompanyNew: return this->ReceiveServerCompanyNew(p);
		case PacketAdminType::ServerCompanyInfo: return this->ReceiveServerCompanyInfo(p);
		case PacketAdminType::ServerCompanyUpdate: return this->ReceiveServerCompanyUpdate(p);
		case PacketAdminType::ServerCompanyRemove: return this->ReceiveServerCompanyRemove(p);
		case PacketAdminType::ServerCompanyEconomy: return this->ReceiveServerCompanyEconomy(p);
		case PacketAdminType::ServerCompanyStatistics: return this->ReceiveServerCompanyStatistics(p);
		case PacketAdminType::ServerChat: return this->ReceiveServerChat(p);
		case PacketAdminType::ServerRemoteConsoleCommand: return this->ReceiveServerRemoteConsoleCommand(p);
		case PacketAdminType::ServerConsole: return this->ReceiveServerConsole(p);
		case PacketAdminType::ServerCommandNames: return this->ReceiveServerCommandNames(p);
		case PacketAdminType::ServerCommandLogging: return this->ReceiveServerCommandLogging(p);
		case PacketAdminType::ServerRemoteConsoleCommandEnd: return this->ReceiveServerRemoteConsoleCommandEnd(p);
		case PacketAdminType::ServerPong: return this->ReceiveServerPong(p);
		case PacketAdminType::ServerAuthenticationRequest: return this->ReceiveServerAuthenticationRequest(p);
		case PacketAdminType::ServerEnableEncryption: return this->ReceiveServerEnableEncryption(p);

		default:
			Debug(net, 0, "[tcp/admin] Received invalid packet type {} from '{}' ({})", type, this->admin_name, this->admin_version);
			this->CloseConnection();
			return NETWORK_RECV_STATUS_MALFORMED_PACKET;
	}
}

/**
 * Do the actual receiving of packets.
 * As long as HandlePacket returns OKAY packets are handled. Upon
 * failure, or no more packets to process the last result of
 * HandlePacket is returned.
 * @return #NetworkRecvStatus of the last handled packet.
 */
NetworkRecvStatus NetworkAdminSocketHandler::ReceivePackets()
{
	std::unique_ptr<Packet> p;
	while ((p = this->ReceivePacket()) != nullptr) {
		NetworkRecvStatus res = this->HandlePacket(*p);
		if (res != NETWORK_RECV_STATUS_OKAY) return res;
	}

	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Helper for logging receiving invalid packets.
 * @param type The received packet type.
 * @return The status the network should have, in this case: "malformed packet error".
 */
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveInvalidPacket(PacketAdminType type)
{
	Debug(net, 0, "[tcp/admin] Received illegal packet type {} from admin {} ({})", type, this->admin_name, this->admin_version);
	return NETWORK_RECV_STATUS_MALFORMED_PACKET;
}

NetworkRecvStatus NetworkAdminSocketHandler::ReceiveAdminJoin(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::AdminJoin); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveAdminQuit(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::AdminQuit); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveAdminUpdateFrequency(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::AdminUpdateFrequency); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveAdminPoll(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::AdminPoll); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveAdminChat(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::AdminChat); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveAdminExternalChat(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::AdminExternalChat); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveAdminRemoteConsoleCommand(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::AdminRemoteConsoleCommand); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveAdminGameScript(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::AdminGameScript); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveAdminPing(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::AdminPing); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveAdminJoinSecure(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::AdminJoinSecure); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveAdminAuthenticationResponse(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::AdminAuthenticationResponse); }

NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerFull(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerFull); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerBanned(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerBanned); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerError(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerError); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerProtocol(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerProtocol); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerWelcome(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerWelcome); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerNewGame(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerNewGame); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerShutdown(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerShutdown); }

NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerDate(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerDate); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerClientJoin(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerClientJoin); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerClientInfo(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerClientInfo); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerClientUpdate(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerClientUpdate); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerClientQuit(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerClientQuit); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerClientError(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerClientError); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerCompanyNew(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerCompanyNew); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerCompanyInfo(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerCompanyInfo); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerCompanyUpdate(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerCompanyUpdate); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerCompanyRemove(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerCompanyRemove); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerCompanyEconomy(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerCompanyEconomy); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerCompanyStatistics(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerCompanyStatistics); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerChat(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerChat); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerRemoteConsoleCommand(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerRemoteConsoleCommand); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerConsole(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerConsole); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerCommandNames(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerCommandNames); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerCommandLogging(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerCommandLogging); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerRemoteConsoleCommandEnd(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerRemoteConsoleCommandEnd); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerPong(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerPong); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerAuthenticationRequest(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerAuthenticationRequest); }
NetworkRecvStatus NetworkAdminSocketHandler::ReceiveServerEnableEncryption(Packet &) { return this->ReceiveInvalidPacket(PacketAdminType::ServerEnableEncryption); }
