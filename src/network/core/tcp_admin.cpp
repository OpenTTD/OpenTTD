/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_admin.cpp Basic functions to receive and send TCP packets to and from the admin network.
 */

#include "../../stdafx.h"

#include "../network_internal.h"
#include "tcp_admin.h"
#include "../../debug.h"

#include "../../safeguards.h"

/* Make sure that these enums match. */
assert_compile((int)CRR_MANUAL    == (int)ADMIN_CRR_MANUAL);
assert_compile((int)CRR_AUTOCLEAN == (int)ADMIN_CRR_AUTOCLEAN);
assert_compile((int)CRR_BANKRUPT  == (int)ADMIN_CRR_BANKRUPT);
assert_compile((int)CRR_END       == (int)ADMIN_CRR_END);

/**
 * Create the admin handler for the given socket.
 * @param s The socket to communicate over.
 */
NetworkAdminSocketHandler::NetworkAdminSocketHandler(SOCKET s) : status(ADMIN_STATUS_INACTIVE)
{
	this->sock = s;
	this->admin_name[0] = '\0';
	this->admin_version[0] = '\0';
}

NetworkAdminSocketHandler::~NetworkAdminSocketHandler()
{
}

NetworkRecvStatus NetworkAdminSocketHandler::CloseConnection(bool error)
{
	delete this;
	return NETWORK_RECV_STATUS_CONN_LOST;
}

/**
 * Handle the given packet, i.e. pass it to the right parser receive command.
 * @param p the packet to handle.
 * @return #NetworkRecvStatus of handling.
 */
NetworkRecvStatus NetworkAdminSocketHandler::HandlePacket(Packet *p)
{
	PacketAdminType type = (PacketAdminType)p->Recv_uint8();

	switch (this->HasClientQuit() ? INVALID_ADMIN_PACKET : type) {
		case ADMIN_PACKET_ADMIN_JOIN:             return this->Receive_ADMIN_JOIN(p);
		case ADMIN_PACKET_ADMIN_QUIT:             return this->Receive_ADMIN_QUIT(p);
		case ADMIN_PACKET_ADMIN_UPDATE_FREQUENCY: return this->Receive_ADMIN_UPDATE_FREQUENCY(p);
		case ADMIN_PACKET_ADMIN_POLL:             return this->Receive_ADMIN_POLL(p);
		case ADMIN_PACKET_ADMIN_CHAT:             return this->Receive_ADMIN_CHAT(p);
		case ADMIN_PACKET_ADMIN_RCON:             return this->Receive_ADMIN_RCON(p);
		case ADMIN_PACKET_ADMIN_GAMESCRIPT:       return this->Receive_ADMIN_GAMESCRIPT(p);
		case ADMIN_PACKET_ADMIN_PING:             return this->Receive_ADMIN_PING(p);

		case ADMIN_PACKET_SERVER_FULL:            return this->Receive_SERVER_FULL(p);
		case ADMIN_PACKET_SERVER_BANNED:          return this->Receive_SERVER_BANNED(p);
		case ADMIN_PACKET_SERVER_ERROR:           return this->Receive_SERVER_ERROR(p);
		case ADMIN_PACKET_SERVER_PROTOCOL:        return this->Receive_SERVER_PROTOCOL(p);
		case ADMIN_PACKET_SERVER_WELCOME:         return this->Receive_SERVER_WELCOME(p);
		case ADMIN_PACKET_SERVER_NEWGAME:         return this->Receive_SERVER_NEWGAME(p);
		case ADMIN_PACKET_SERVER_SHUTDOWN:        return this->Receive_SERVER_SHUTDOWN(p);

		case ADMIN_PACKET_SERVER_DATE:            return this->Receive_SERVER_DATE(p);
		case ADMIN_PACKET_SERVER_CLIENT_JOIN:     return this->Receive_SERVER_CLIENT_JOIN(p);
		case ADMIN_PACKET_SERVER_CLIENT_INFO:     return this->Receive_SERVER_CLIENT_INFO(p);
		case ADMIN_PACKET_SERVER_CLIENT_UPDATE:   return this->Receive_SERVER_CLIENT_UPDATE(p);
		case ADMIN_PACKET_SERVER_CLIENT_QUIT:     return this->Receive_SERVER_CLIENT_QUIT(p);
		case ADMIN_PACKET_SERVER_CLIENT_ERROR:    return this->Receive_SERVER_CLIENT_ERROR(p);
		case ADMIN_PACKET_SERVER_COMPANY_NEW:     return this->Receive_SERVER_COMPANY_NEW(p);
		case ADMIN_PACKET_SERVER_COMPANY_INFO:    return this->Receive_SERVER_COMPANY_INFO(p);
		case ADMIN_PACKET_SERVER_COMPANY_UPDATE:  return this->Receive_SERVER_COMPANY_UPDATE(p);
		case ADMIN_PACKET_SERVER_COMPANY_REMOVE:  return this->Receive_SERVER_COMPANY_REMOVE(p);
		case ADMIN_PACKET_SERVER_COMPANY_ECONOMY: return this->Receive_SERVER_COMPANY_ECONOMY(p);
		case ADMIN_PACKET_SERVER_COMPANY_STATS:   return this->Receive_SERVER_COMPANY_STATS(p);
		case ADMIN_PACKET_SERVER_CHAT:            return this->Receive_SERVER_CHAT(p);
		case ADMIN_PACKET_SERVER_RCON:            return this->Receive_SERVER_RCON(p);
		case ADMIN_PACKET_SERVER_CONSOLE:         return this->Receive_SERVER_CONSOLE(p);
		case ADMIN_PACKET_SERVER_CMD_NAMES:       return this->Receive_SERVER_CMD_NAMES(p);
		case ADMIN_PACKET_SERVER_CMD_LOGGING:     return this->Receive_SERVER_CMD_LOGGING(p);
		case ADMIN_PACKET_SERVER_RCON_END:        return this->Receive_SERVER_RCON_END(p);
		case ADMIN_PACKET_SERVER_PONG:            return this->Receive_SERVER_PONG(p);

		default:
			if (this->HasClientQuit()) {
				DEBUG(net, 0, "[tcp/admin] received invalid packet type %d from '%s' (%s)", type, this->admin_name, this->admin_version);
			} else {
				DEBUG(net, 0, "[tcp/admin] received illegal packet from '%s' (%s)", this->admin_name, this->admin_version);
			}

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
	Packet *p;
	while ((p = this->ReceivePacket()) != NULL) {
		NetworkRecvStatus res = this->HandlePacket(p);
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
	DEBUG(net, 0, "[tcp/admin] received illegal packet type %d from admin %s (%s)", type, this->admin_name, this->admin_version);
	return NETWORK_RECV_STATUS_MALFORMED_PACKET;
}

NetworkRecvStatus NetworkAdminSocketHandler::Receive_ADMIN_JOIN(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_ADMIN_JOIN); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_ADMIN_QUIT(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_ADMIN_QUIT); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_ADMIN_UPDATE_FREQUENCY(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_ADMIN_UPDATE_FREQUENCY); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_ADMIN_POLL(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_ADMIN_POLL); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_ADMIN_CHAT(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_ADMIN_CHAT); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_ADMIN_RCON(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_ADMIN_RCON); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_ADMIN_GAMESCRIPT(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_ADMIN_GAMESCRIPT); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_ADMIN_PING(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_ADMIN_PING); }

NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_FULL(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_FULL); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_BANNED(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_BANNED); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_ERROR(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_ERROR); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_PROTOCOL(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_PROTOCOL); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_WELCOME(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_WELCOME); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_NEWGAME(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_NEWGAME); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_SHUTDOWN(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_SHUTDOWN); }

NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_DATE(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_DATE); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_CLIENT_JOIN(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_CLIENT_JOIN); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_CLIENT_INFO(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_CLIENT_INFO); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_CLIENT_UPDATE(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_CLIENT_UPDATE); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_CLIENT_QUIT(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_CLIENT_QUIT); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_CLIENT_ERROR(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_CLIENT_ERROR); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_COMPANY_NEW(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_COMPANY_NEW); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_COMPANY_INFO(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_COMPANY_INFO); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_COMPANY_UPDATE(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_COMPANY_UPDATE); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_COMPANY_REMOVE(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_COMPANY_REMOVE); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_COMPANY_ECONOMY(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_COMPANY_ECONOMY); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_COMPANY_STATS(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_COMPANY_STATS); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_CHAT(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_CHAT); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_RCON(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_RCON); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_CONSOLE(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_CONSOLE); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_CMD_NAMES(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_CMD_NAMES); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_CMD_LOGGING(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_CMD_LOGGING); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_RCON_END(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_RCON_END); }
NetworkRecvStatus NetworkAdminSocketHandler::Receive_SERVER_PONG(Packet *p) { return this->ReceiveInvalidPacket(ADMIN_PACKET_SERVER_PONG); }
