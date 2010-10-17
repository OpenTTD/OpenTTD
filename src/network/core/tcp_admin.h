/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_admin.h Basic functions to receive and send TCP packets to and from the admin network.
 */

#ifndef NETWORK_CORE_TCP_ADMIN_H
#define NETWORK_CORE_TCP_ADMIN_H

#include "os_abstraction.h"
#include "tcp.h"
#include "../network_type.h"
#include "../../core/pool_type.hpp"

#ifdef ENABLE_NETWORK

/**
 * Enum with types of TCP packets specific to the admin network.
 * This protocol may only be extended to ensure stability.
 */
enum PacketAdminType {
	ADMIN_PACKET_ADMIN_JOIN,             ///< The admin announces and authenticates itself to the server.
	ADMIN_PACKET_ADMIN_QUIT,             ///< The admin tells the server that it is quitting.
	ADMIN_PACKET_ADMIN_UPDATE_FREQUENCY, ///< The admin tells the server the update frequency of a particular piece of information.
	ADMIN_PACKET_ADMIN_POLL,             ///< The admin explicitly polls for a piece of information.

	ADMIN_PACKET_SERVER_FULL = 100,      ///< The server tells the admin it cannot accept the admin.
	ADMIN_PACKET_SERVER_BANNED,          ///< The server tells the admin it is banned.
	ADMIN_PACKET_SERVER_ERROR,           ///< The server tells the admin an error has occurred.
	ADMIN_PACKET_SERVER_PROTOCOL,        ///< The server tells the admin its protocol version.
	ADMIN_PACKET_SERVER_WELCOME,         ///< The server welcomes the admin to a game.
	ADMIN_PACKET_SERVER_NEWGAME,         ///< The server tells the admin its going to start a new game.
	ADMIN_PACKET_SERVER_SHUTDOWN,        ///< The server tells the admin its shutting down.

	ADMIN_PACKET_SERVER_DATE,            ///< The server tells the admin what the current game date is.

	INVALID_ADMIN_PACKET = 0xFF,         ///< An invalid marker for admin packets.
};

/** Status of an admin. */
enum AdminStatus {
	ADMIN_STATUS_INACTIVE,      ///< The admin is not connected nor active.
	ADMIN_STATUS_ACTIVE,        ///< The admin is active.
	ADMIN_STATUS_END            ///< Must ALWAYS be on the end of this list!! (period)
};

/** Update types an admin can register a frequency for */
enum AdminUpdateType {
	ADMIN_UPDATE_DATE,            ///< Updates about the date of the game.
	ADMIN_UPDATE_END              ///< Must ALWAYS be on the end of this list!! (period)
};

/** Update frequencies an admin can register. */
enum AdminUpdateFrequency {
	ADMIN_FREQUENCY_POLL      = 0x01, ///< The admin can poll this.
	ADMIN_FREQUENCY_DAILY     = 0x02, ///< The admin gets information about this on a daily basis.
	ADMIN_FREQUENCY_WEEKLY    = 0x04, ///< The admin gets information about this on a weekly basis.
	ADMIN_FREQUENCY_MONTHLY   = 0x08, ///< The admin gets information about this on a monthly basis.
	ADMIN_FREQUENCY_QUARTERLY = 0x10, ///< The admin gets information about this on a quarterly basis.
	ADMIN_FREQUENCY_ANUALLY   = 0x20, ///< The admin gets information about this on a yearly basis.
	ADMIN_FREQUENCY_AUTOMATIC = 0x40, ///< The admin gets information about this when it changes.
};
DECLARE_ENUM_AS_BIT_SET(AdminUpdateFrequency);

#define DECLARE_ADMIN_RECEIVE_COMMAND(type) virtual NetworkRecvStatus NetworkPacketReceive_## type ##_command(Packet *p)
#define DEF_ADMIN_RECEIVE_COMMAND(cls, type) NetworkRecvStatus cls ##NetworkAdminSocketHandler::NetworkPacketReceive_ ## type ## _command(Packet *p)

/** Main socket handler for admin related connections. */
class NetworkAdminSocketHandler : public NetworkTCPSocketHandler {
protected:
	char admin_name[NETWORK_CLIENT_NAME_LENGTH];           ///< Name of the admin.
	char admin_version[NETWORK_REVISION_LENGTH];           ///< Version string of the admin.
	AdminStatus status;                                    ///< Status of this admin.

	/**
	 * Join the admin network:
	 * string  Password the server is expecting for this network.
	 * string  Name of the application being used to connect.
	 * string  Version string of the application being used to connect.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_ADMIN_JOIN);

	/**
	 * Notification to the server that this admin is quitting.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_ADMIN_QUIT);

	/**
	 * Register updates to be sent at certain frequencies (as announced in the PROTOCOL packet):
	 * uint16  Update type (see #AdminUpdateType).
	 * uint16  Update frequency (see #AdminUpdateFrequency), setting #ADMIN_FREQUENCY_POLL is always ignored.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_ADMIN_UPDATE_FREQUENCY);

	/**
	 * Poll the server for certain updates, an invalid poll (e.g. not existent id) gets silently dropped:
	 * uint8   #AdminUpdateType the server should answer for, only if #AdminUpdateFrequency #ADMIN_FREQUENCY_POLL is advertised in the PROTOCOL packet.
	 * uint32  ID relevant to the packet type.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_ADMIN_POLL);

	/**
	 * The server is full (connection gets closed).
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_FULL);

	/**
	 * The source IP address is banned (connection gets closed).
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_BANNED);

	/**
	 * An error was caused by this admin connection (connection gets closed).
	 * uint8  NetworkErrorCode the error caused.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_ERROR);

	/**
	 * Inform a just joined admin about the protocol specifics:
	 * uint8   Protocol version.
	 * bool    Further protocol data follows (repeats through all update packet types).
	 * uint16  Update packet type.
	 * uint16  Frequencies allowed for this update packet (bitwise).
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_PROTOCOL);

	/**
	 * Welcome a connected admin to the game:
	 * string  Name of the Server (e.g. as advertised to master server).
	 * string  OpenTTD version string.
	 * bool    Server is dedicated.
	 * string  Name of the Map.
	 * uint32  Random seed of the Map.
	 * uint8   Landscape of the Map.
	 * uint32  Start date of the Map.
	 * uint16  Map width.
	 * uint16  Map height.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_WELCOME);

	/**
	 * Notification about a newgame.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_NEWGAME);

	/**
	 * Notification about the server shutting down.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_SHUTDOWN);

	/**
	 * Send the current date of the game:
	 * uint32  Current game date.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_DATE);

	NetworkRecvStatus HandlePacket(Packet *p);
public:
	NetworkRecvStatus CloseConnection(bool error = true);

	NetworkAdminSocketHandler(SOCKET s);
	~NetworkAdminSocketHandler();

	NetworkRecvStatus Recv_Packets();

	const char *Recv_Command(Packet *p, CommandPacket *cp);
	void Send_Command(Packet *p, const CommandPacket *cp);
};

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_TCP_ADMIN_H */
