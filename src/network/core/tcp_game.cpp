/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_game.cpp Basic functions to receive and send TCP packets for game purposes.
 */

#ifdef ENABLE_NETWORK

#include "../../stdafx.h"

#include "../network.h"
#include "../network_internal.h"
#include "../../debug.h"

#include "table/strings.h"

/**
 * Create a new socket for the game connection.
 * @param s The socket to connect with.
 */
NetworkGameSocketHandler::NetworkGameSocketHandler(SOCKET s)
{
	this->sock              = s;
	this->last_frame        = _frame_counter;
	this->last_frame_server = _frame_counter;
	this->last_packet       = _realtime_tick;
}

/**
 * Functions to help ReceivePacket/SendPacket a bit
 *  A socket can make errors. When that happens this handles what to do.
 * For clients: close connection and drop back to main-menu
 * For servers: close connection and that is it
 * @return the new status
 */
NetworkRecvStatus NetworkGameSocketHandler::CloseConnection(bool error)
{
	/* Clients drop back to the main menu */
	if (!_network_server && _networking) {
		_switch_mode = SM_MENU;
		_networking = false;
		extern StringID _switch_mode_errorstr;
		_switch_mode_errorstr = STR_NETWORK_ERROR_LOSTCONNECTION;

		return NETWORK_RECV_STATUS_CONN_LOST;
	}

	return this->CloseConnection(error ? NETWORK_RECV_STATUS_SERVER_ERROR : NETWORK_RECV_STATUS_CONN_LOST);
}


/**
 * Defines a simple (switch) case for each network packet
 * @param type the packet type to create the case for
 */
#define GAME_COMMAND(type) case type: return this->NetworkPacketReceive_ ## type ## _command(p); break;

/**
 * Handle the given packet, i.e. pass it to the right parser receive command.
 * @param p the packet to handle
 * @return #NetworkRecvStatus of handling.
 */
NetworkRecvStatus NetworkGameSocketHandler::HandlePacket(Packet *p)
{
	PacketGameType type = (PacketGameType)p->Recv_uint8();

	this->last_packet = _realtime_tick;

	switch (this->HasClientQuit() ? PACKET_END : type) {
		GAME_COMMAND(PACKET_SERVER_FULL)
		GAME_COMMAND(PACKET_SERVER_BANNED)
		GAME_COMMAND(PACKET_CLIENT_JOIN)
		GAME_COMMAND(PACKET_SERVER_ERROR)
		GAME_COMMAND(PACKET_CLIENT_COMPANY_INFO)
		GAME_COMMAND(PACKET_SERVER_COMPANY_INFO)
		GAME_COMMAND(PACKET_SERVER_CLIENT_INFO)
		GAME_COMMAND(PACKET_SERVER_NEED_GAME_PASSWORD)
		GAME_COMMAND(PACKET_SERVER_NEED_COMPANY_PASSWORD)
		GAME_COMMAND(PACKET_CLIENT_GAME_PASSWORD)
		GAME_COMMAND(PACKET_CLIENT_COMPANY_PASSWORD)
		GAME_COMMAND(PACKET_SERVER_WELCOME)
		GAME_COMMAND(PACKET_CLIENT_GETMAP)
		GAME_COMMAND(PACKET_SERVER_WAIT)
		GAME_COMMAND(PACKET_SERVER_MAP_BEGIN)
		GAME_COMMAND(PACKET_SERVER_MAP_DATA)
		GAME_COMMAND(PACKET_SERVER_MAP_DONE)
		GAME_COMMAND(PACKET_CLIENT_MAP_OK)
		GAME_COMMAND(PACKET_SERVER_JOIN)
		GAME_COMMAND(PACKET_SERVER_FRAME)
		GAME_COMMAND(PACKET_SERVER_SYNC)
		GAME_COMMAND(PACKET_CLIENT_ACK)
		GAME_COMMAND(PACKET_CLIENT_COMMAND)
		GAME_COMMAND(PACKET_SERVER_COMMAND)
		GAME_COMMAND(PACKET_CLIENT_CHAT)
		GAME_COMMAND(PACKET_SERVER_CHAT)
		GAME_COMMAND(PACKET_CLIENT_SET_PASSWORD)
		GAME_COMMAND(PACKET_CLIENT_SET_NAME)
		GAME_COMMAND(PACKET_CLIENT_QUIT)
		GAME_COMMAND(PACKET_CLIENT_ERROR)
		GAME_COMMAND(PACKET_SERVER_QUIT)
		GAME_COMMAND(PACKET_SERVER_ERROR_QUIT)
		GAME_COMMAND(PACKET_SERVER_SHUTDOWN)
		GAME_COMMAND(PACKET_SERVER_NEWGAME)
		GAME_COMMAND(PACKET_SERVER_RCON)
		GAME_COMMAND(PACKET_CLIENT_RCON)
		GAME_COMMAND(PACKET_SERVER_CHECK_NEWGRFS)
		GAME_COMMAND(PACKET_CLIENT_NEWGRFS_CHECKED)
		GAME_COMMAND(PACKET_SERVER_MOVE)
		GAME_COMMAND(PACKET_CLIENT_MOVE)
		GAME_COMMAND(PACKET_SERVER_COMPANY_UPDATE)
		GAME_COMMAND(PACKET_SERVER_CONFIG_UPDATE)

		default:
			this->CloseConnection();

			if (this->HasClientQuit()) {
				DEBUG(net, 0, "[tcp/game] received invalid packet type %d from client %d", type, this->client_id);
			} else {
				DEBUG(net, 0, "[tcp/game] received illegal packet from client %d", this->client_id);
			}
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
	Packet *p;
	while ((p = this->ReceivePacket()) != NULL) {
		NetworkRecvStatus res = HandlePacket(p);
		delete p;
		if (res != NETWORK_RECV_STATUS_OKAY) return res;
	}

	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Create stub implementations for all receive commands that only
 * show a warning that the given command is not available for the
 * socket where the packet came from.
 * @param type the packet type to create the stub for
 */
#define DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(type) \
NetworkRecvStatus NetworkGameSocketHandler::NetworkPacketReceive_## type ##_command(Packet *p) \
{ \
	DEBUG(net, 0, "[tcp/game] received illegal packet type %d from client %d", \
			type, this->client_id); \
	return NETWORK_RECV_STATUS_MALFORMED_PACKET; \
}

DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_FULL)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_BANNED)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_JOIN)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_ERROR)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_COMPANY_INFO)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_COMPANY_INFO)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_CLIENT_INFO)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_NEED_GAME_PASSWORD)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_NEED_COMPANY_PASSWORD)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_GAME_PASSWORD)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_COMPANY_PASSWORD)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_WELCOME)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_GETMAP)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_WAIT)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_MAP_BEGIN)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_MAP_DATA)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_MAP_DONE)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_MAP_OK)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_JOIN)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_FRAME)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_SYNC)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_ACK)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_COMMAND)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_COMMAND)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_CHAT)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_CHAT)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_SET_PASSWORD)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_SET_NAME)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_QUIT)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_ERROR)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_QUIT)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_ERROR_QUIT)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_SHUTDOWN)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_NEWGAME)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_RCON)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_RCON)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_CHECK_NEWGRFS)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_NEWGRFS_CHECKED)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_MOVE)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_MOVE)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_COMPANY_UPDATE)
DEFINE_UNAVAILABLE_GAME_RECEIVE_COMMAND(PACKET_SERVER_CONFIG_UPDATE)

#endif /* ENABLE_NETWORK */
