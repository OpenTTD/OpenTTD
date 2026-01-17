/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tcp_game.cpp Basic functions to receive and send TCP packets for game purposes. */

#include "../../stdafx.h"

#include "../network.h"
#include "../network_internal.h"
#include "../../debug.h"
#include "../../error.h"
#include "../../strings_func.h"

#include "table/strings.h"

#include "../../safeguards.h"

static std::vector<std::unique_ptr<NetworkGameSocketHandler>> _deferred_deletions; ///< NetworkGameSocketHandler that still need to be deleted.

/**
 * Create a new socket for the game connection.
 * @param s The socket to connect with.
 */
NetworkGameSocketHandler::NetworkGameSocketHandler(SOCKET s) : NetworkTCPSocketHandler(s),
		last_frame(_frame_counter), last_frame_server(_frame_counter), last_packet(std::chrono::steady_clock::now()) {}

/**
 * Functions to help ReceivePacket/SendPacket a bit
 *  A socket can make errors. When that happens this handles what to do.
 * For clients: close connection and drop back to main-menu
 * For servers: close connection and that is it
 * @copydoc NetworkTCPSocketHandler::CloseConnection
 */
NetworkRecvStatus NetworkGameSocketHandler::CloseConnection([[maybe_unused]] bool error)
{
	/* Clients drop back to the main menu */
	if (!_network_server && _networking) {
		ClientNetworkEmergencySave();
		_switch_mode = SM_MENU;
		_networking = false;
		ShowErrorMessage(GetEncodedString(STR_NETWORK_ERROR_LOSTCONNECTION), {}, WL_CRITICAL);

		return this->CloseConnection(NETWORK_RECV_STATUS_CLIENT_QUIT);
	}

	return this->CloseConnection(NETWORK_RECV_STATUS_CONNECTION_LOST);
}


/**
 * Handle the given packet, i.e. pass it to the right parser receive command.
 * @param p the packet to handle
 * @return #NetworkRecvStatus of handling.
 */
NetworkRecvStatus NetworkGameSocketHandler::HandlePacket(Packet &p)
{
	PacketGameType type = static_cast<PacketGameType>(p.Recv_uint8());

	this->last_packet = std::chrono::steady_clock::now();

	switch (type) {
		case PacketGameType::ServerFull: return this->ReceiveServerFull(p);
		case PacketGameType::ServerBanned: return this->ReceiveServerBanned(p);
		case PacketGameType::ClientJoin: return this->ReceiveClientJoin(p);
		case PacketGameType::ServerError: return this->ReceiveServerError(p);
		case PacketGameType::ClientGameInfo: return this->ReceiveClientGameInfo(p);
		case PacketGameType::ServerGameInfo: return this->ReceiveServerGameInfo(p);
		case PacketGameType::ServerClientInfo: return this->ReceiveServerClientInfo(p);
		case PacketGameType::ClientIdentify: return this->ReceiveClientIdentify(p);
		case PacketGameType::ServerAuthenticationRequest: return this->ReceiveServerAuthenticationRequest(p);
		case PacketGameType::ClientAuthenticationResponse: return this->ReceiveClientAuthenticationResponse(p);
		case PacketGameType::ServerEnableEncryption: return this->ReceiveServerEnableEncryption(p);
		case PacketGameType::ServerWelcome: return this->ReceiveServerWelcome(p);
		case PacketGameType::ClientGetMap: return this->ReceiveClientGetMap(p);
		case PacketGameType::ServerWaitForMap: return this->ReceiveServerWaitForMap(p);
		case PacketGameType::ServerMapBegin: return this->ReceiveServerMapBegin(p);
		case PacketGameType::ServerMapSize: return this->ReceiveServerMapSize(p);
		case PacketGameType::ServerMapData: return this->ReceiveServerMapData(p);
		case PacketGameType::ServerMapDone: return this->ReceiveServerMapDone(p);
		case PacketGameType::ClientMapOk: return this->ReceiveClientMapOk(p);
		case PacketGameType::ServerClientJoined: return this->ReceiveServerClientJoined(p);
		case PacketGameType::ServerFrame: return this->ReceiveServerFrame(p);
		case PacketGameType::ServerSync: return this->ReceiveServerSync(p);
		case PacketGameType::ClientAck: return this->ReceiveClientAck(p);
		case PacketGameType::ClientCommand: return this->ReceiveClientCommand(p);
		case PacketGameType::ServerCommand: return this->ReceiveServerCommand(p);
		case PacketGameType::ClientChat: return this->ReceiveClientChat(p);
		case PacketGameType::ServerChat: return this->ReceiveServerChat(p);
		case PacketGameType::ServerExternalChat: return this->ReceiveServerExternalChat(p);
		case PacketGameType::ClientSetName: return this->ReceiveClientSetName(p);
		case PacketGameType::ClientQuit: return this->ReceiveClientQuit(p);
		case PacketGameType::ClientError: return this->ReceiveClientError(p);
		case PacketGameType::ServerQuit: return this->ReceiveServerQuit(p);
		case PacketGameType::ServerErrorQuit: return this->ReceiveServerErrorQuit(p);
		case PacketGameType::ServerShutdown: return this->ReceiveServerShutdown(p);
		case PacketGameType::ServerNewGame: return this->ReceiveServerNewGame(p);
		case PacketGameType::ServerRemoteConsoleCommand: return this->ReceiveServerRemoteConsoleCommand(p);
		case PacketGameType::ClientRemoteConsoleCommand: return this->ReceiveClientRemoteConsoleCommand(p);
		case PacketGameType::ServerCheckNewGRFs: return this->ReceiveServerCheckNewGRFs(p);
		case PacketGameType::ClientNewGRFsChecked: return this->ReceiveClientNewGRFsChecked(p);
		case PacketGameType::ServerMove: return this->ReceiveServerMove(p);
		case PacketGameType::ClientMove: return this->ReceiveClientMove(p);
		case PacketGameType::ServerConfigurationUpdate: return this->ReceiveServerConfigurationUpdate(p);

		default:
			Debug(net, 0, "[tcp/game] Received invalid packet type {} from client {}", type, this->client_id);
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
NetworkRecvStatus NetworkGameSocketHandler::ReceivePackets()
{
	std::unique_ptr<Packet> p;
	while ((p = this->ReceivePacket()) != nullptr) {
		NetworkRecvStatus res = HandlePacket(*p);
		if (res != NETWORK_RECV_STATUS_OKAY) return res;
	}

	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Helper for logging receiving invalid packets.
 * @param type The received packet type.
 * @return The status the network should have, in this case: "malformed packet error".
 */
NetworkRecvStatus NetworkGameSocketHandler::ReceiveInvalidPacket(PacketGameType type)
{
	Debug(net, 0, "[tcp/game] Received illegal packet type {} from client {}", type, this->client_id);
	return NETWORK_RECV_STATUS_MALFORMED_PACKET;
}

NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerFull(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerFull); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerBanned(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerBanned); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveClientJoin(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ClientJoin); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerError(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerError); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveClientGameInfo(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ClientGameInfo); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerGameInfo(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerGameInfo); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerClientInfo(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerClientInfo); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveClientIdentify(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ClientIdentify); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerAuthenticationRequest(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerAuthenticationRequest); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveClientAuthenticationResponse(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ClientAuthenticationResponse); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerEnableEncryption(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerEnableEncryption); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerWelcome(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerWelcome); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveClientGetMap(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ClientGetMap); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerWaitForMap(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerWaitForMap); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerMapBegin(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerMapBegin); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerMapSize(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerMapSize); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerMapData(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerMapData); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerMapDone(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerMapDone); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveClientMapOk(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ClientMapOk); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerClientJoined(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerClientJoined); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerFrame(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerFrame); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerSync(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerSync); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveClientAck(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ClientAck); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveClientCommand(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ClientCommand); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerCommand(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerCommand); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveClientChat(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ClientChat); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerChat(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerChat); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerExternalChat(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerExternalChat); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveClientSetName(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ClientSetName); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveClientQuit(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ClientQuit); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveClientError(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ClientError); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerQuit(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerQuit); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerErrorQuit(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerErrorQuit); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerShutdown(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerShutdown); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerNewGame(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerNewGame); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerRemoteConsoleCommand(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerRemoteConsoleCommand); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveClientRemoteConsoleCommand(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ClientRemoteConsoleCommand); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerCheckNewGRFs(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerCheckNewGRFs); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveClientNewGRFsChecked(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ClientNewGRFsChecked); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerMove(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerMove); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveClientMove(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ClientMove); }
NetworkRecvStatus NetworkGameSocketHandler::ReceiveServerConfigurationUpdate(Packet &) { return this->ReceiveInvalidPacket(PacketGameType::ServerConfigurationUpdate); }

/** Mark this socket handler for deletion, once iterating the socket handlers is done. */
void NetworkGameSocketHandler::DeferDeletion()
{
	_deferred_deletions.emplace_back(this);
	this->is_pending_deletion = true;
}

/** Actually delete the socket handlers that were marked for deletion. */
/* static */ void NetworkGameSocketHandler::ProcessDeferredDeletions()
{
	/* Calls deleter on all items */
	_deferred_deletions.clear();
}
