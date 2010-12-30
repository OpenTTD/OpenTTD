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
	ADMIN_PACKET_ADMIN_CHAT,             ///< The admin sends a chat message to be distributed.
	ADMIN_PACKET_ADMIN_RCON,             ///< The admin sends a remote console command.

	ADMIN_PACKET_SERVER_FULL = 100,      ///< The server tells the admin it cannot accept the admin.
	ADMIN_PACKET_SERVER_BANNED,          ///< The server tells the admin it is banned.
	ADMIN_PACKET_SERVER_ERROR,           ///< The server tells the admin an error has occurred.
	ADMIN_PACKET_SERVER_PROTOCOL,        ///< The server tells the admin its protocol version.
	ADMIN_PACKET_SERVER_WELCOME,         ///< The server welcomes the admin to a game.
	ADMIN_PACKET_SERVER_NEWGAME,         ///< The server tells the admin its going to start a new game.
	ADMIN_PACKET_SERVER_SHUTDOWN,        ///< The server tells the admin its shutting down.

	ADMIN_PACKET_SERVER_DATE,            ///< The server tells the admin what the current game date is.
	ADMIN_PACKET_SERVER_CLIENT_JOIN,     ///< The server tells the admin that a client has joined.
	ADMIN_PACKET_SERVER_CLIENT_INFO,     ///< The server gives the admin information about a client.
	ADMIN_PACKET_SERVER_CLIENT_UPDATE,   ///< The server gives the admin an information update on a client.
	ADMIN_PACKET_SERVER_CLIENT_QUIT,     ///< The server tells the admin that a client quit.
	ADMIN_PACKET_SERVER_CLIENT_ERROR,    ///< The server tells the admin that a client caused an error.
	ADMIN_PACKET_SERVER_COMPANY_NEW,     ///< The server tells the admin that a new company has started.
	ADMIN_PACKET_SERVER_COMPANY_INFO,    ///< The server gives the admin information about a company.
	ADMIN_PACKET_SERVER_COMPANY_UPDATE,  ///< The server gives the admin an information update on a company.
	ADMIN_PACKET_SERVER_COMPANY_REMOVE,  ///< The server tells the admin that a company was removed.
	ADMIN_PACKET_SERVER_COMPANY_ECONOMY, ///< The server gives the admin some economy related company information.
	ADMIN_PACKET_SERVER_COMPANY_STATS,   ///< The server gives the admin some statistics about a company.
	ADMIN_PACKET_SERVER_CHAT,            ///< The server received a chat message and relays it.
	ADMIN_PACKET_SERVER_RCON,            ///< The server's reply to a remove console command.
	ADMIN_PACKET_SERVER_CONSOLE,         ///< The server gives the admin the data that got printed to its console.
	ADMIN_PACKET_SERVER_CMD_NAMES,       ///< The server sends out the names of the DoCommands to the admins.
	ADMIN_PACKET_SERVER_CMD_LOGGING,     ///< The server gives the admin copies of incoming command packets.

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
	ADMIN_UPDATE_CLIENT_INFO,     ///< Updates about the information of clients.
	ADMIN_UPDATE_COMPANY_INFO,    ///< Updates about the generic information of companies.
	ADMIN_UPDATE_COMPANY_ECONOMY, ///< Updates about the economy of companies.
	ADMIN_UPDATE_COMPANY_STATS,   ///< Updates about the statistics of companies.
	ADMIN_UPDATE_CHAT,            ///< The admin would like to have chat messages.
	ADMIN_UPDATE_CONSOLE,         ///< The admin would like to have console messages.
	ADMIN_UPDATE_CMD_NAMES,       ///< The admin would like a list of all DoCommand names.
	ADMIN_UPDATE_CMD_LOGGING,     ///< The admin would like to have DoCommand information.
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
DECLARE_ENUM_AS_BIT_SET(AdminUpdateFrequency)

/** Reasons for removing a company - communicated to admins. */
enum AdminCompanyRemoveReason {
	ADMIN_CRR_MANUAL,    ///< The company is manually removed.
	ADMIN_CRR_AUTOCLEAN, ///< The company is removed due to autoclean.
	ADMIN_CRR_BANKRUPT   ///< The company went belly-up.
};

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
	 * uint32  ID relevant to the packet type, e.g.
	 *          - the client ID for #ADMIN_UPDATE_CLIENT_INFO. Use UINT32_MAX to show all clients.
	 *          - the company ID for #ADMIN_UPDATE_COMPANY_INFO. Use UINT32_MAX to show all companies.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_ADMIN_POLL);

	/**
	 * Send chat as the server:
	 * uint8   Action such as NETWORK_ACTION_CHAT_CLIENT (see #NetworkAction).
	 * uint8   Destination type such as DESTTYPE_BROADCAST (see #DestType).
	 * uint32  ID of the destination such as company or client id.
	 * string  Message.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_ADMIN_CHAT);

	/**
	 * Execute a command on the servers console:
	 * string  Command to be executed.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_ADMIN_RCON);

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

	/**
	 * Notification of a new client:
	 * uint32  ID of the new client.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_CLIENT_JOIN);

	/**
	 * Client information of a specific client:
	 * uint32  ID of the client.
	 * string  Network address of the client.
	 * string  Name of the client.
	 * uint8   Language of the client.
	 * uint32  Date the client joined the game.
	 * uint8   ID of the company the client is playing as (255 for spectators).
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_CLIENT_INFO);

	/**
	 * Client update details on a specific client (e.g. after rename or move):
	 * uint32  ID of the client.
	 * string  Name of the client.
	 * uint8   ID of the company the client is playing as (255 for spectators).
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_CLIENT_UPDATE);

	/**
	 * Notification about a client leaving the game.
	 * uint32  ID of the client that just left.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_CLIENT_QUIT);

	/**
	 * Notification about a client error (and thus the clients disconnection).
	 * uint32  ID of the client that made the error.
	 * uint8   Error the client made (see NetworkErrorCode).
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_CLIENT_ERROR);

	/**
	 * Notification of a new company:
	 * uint8   ID of the new company.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_COMPANY_NEW);

	/**
	 * Company information on a specific company:
	 * uint8   ID of the company.
	 * string  Name of the company.
	 * string  Name of the companies manager.
	 * uint8   Main company colour.
	 * bool    Company is password protected.
	 * uint32  Year the company was inaugurated.
	 * bool    Company is an AI.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_COMPANY_INFO);

	/**
	 * Company information of a specific company:
	 * uint8   ID of the company.
	 * string  Name of the company.
	 * string  Name of the companies manager.
	 * uint8   Main company colour.
	 * bool    Company is password protected.
	 * uint8   Quarters of bankruptcy.
	 * uint8   Owner of share 1.
	 * uint8   Owner of share 2.
	 * uint8   Owner of share 3.
	 * uint8   Owner of share 4.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_COMPANY_UPDATE);

	/**
	 * Notification about a removed company (e.g. due to banrkuptcy).
	 * uint8   ID of the company.
	 * uint8   Reason for being removed (see #AdminCompanyRemoveReason).
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_COMPANY_REMOVE);

	/**
	 * Economy update of a specific company:
	 * uint8   ID of the company.
	 * uint64  Money.
	 * uint64  Loan.
	 * uint64  Income.
	 * uint64  Company value (last quarter).
	 * uint16  Performance (last quarter).
	 * uint16  Delivered cargo (last quarter).
	 * uint64  Company value (previous quarter).
	 * uint16  Performance (previous quarter).
	 * uint16  Delivered cargo (previous quarter).
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_COMPANY_ECONOMY);

	/**
	 * Company statistics on stations and vehicles:
	 * uint8   ID of the company.
	 * uint16  Number of trains.
	 * uint16  Number of lorries.
	 * uint16  Number of busses.
	 * uint16  Number of planes.
	 * uint16  Number of ships.
	 * uint16  Number of train stations.
	 * uint16  Number of lorry stations.
	 * uint16  Number of bus stops.
	 * uint16  Number of airports and heliports.
	 * uint16  Number of harbours.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_COMPANY_STATS);

	/**
	 * Send chat from the game into the admin network:
	 * uint8   Action such as NETWORK_ACTION_CHAT_CLIENT (see #NetworkAction).
	 * uint8   Destination type such as DESTTYPE_BROADCAST (see #DestType).
	 * uint32  ID of the client who sent this message.
	 * string  Message.
	 * uint64  Money (only when it is a 'give money' action).
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_CHAT);

	/**
	 * Result of an rcon command:
	 * uint16  Colour as it would be used on the server or a client.
	 * string  Output of the executed command.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_RCON);

	/**
	 * Send what would be printed on the server's console also into the admin network.
	 * string  The origin of the text, e.g. "console" for console, or "net" for network related (debug) messages.
	 * string  Text as found on the console of the server.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_CONSOLE);

	/**
	 * Send DoCommand names to the bot upon request only.
	 * Multiple of these packets can follow each other in order to provide
	 * all known DoCommand names.
	 *
	 * NOTICE: Data provided with this packet is not stable and will not be
	 *         treated as such. Do not rely on IDs or names to be constant
	 *         across different versions / revisions of OpenTTD.
	 *         Data provided in this packet is for logging purposes only.
	 *
	 * These three fields are repeated until the packet is full:
	 * bool    Data to follow.
	 * uint16  ID of the DoCommand.
	 * string  Name of DoCommand.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_CMD_NAMES);

	/**
	 * Send incoming command packets to the admin network.
	 * This is for logging purposes only.
	 *
	 * NOTICE: Data provided with this packet is not stable and will not be
	 *         treated as such. Do not rely on IDs or names to be constant
	 *         across different versions / revisions of OpenTTD.
	 *         Data provided in this packet is for logging purposes only.
	 *
	 * uint32  ID of the client sending the command.
	 * uint8   ID of the company (0..MAX_COMPANIES-1).
	 * uint16  ID of the command.
	 * uint32  P1 (variable data passed to the command).
	 * uint32  P2 (variable data passed to the command).
	 * uint32  Tile where this is taking place.
	 * string  Text passed to the command.
	 * uint32  Frame of execution.
	 */
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_SERVER_CMD_LOGGING);

	NetworkRecvStatus HandlePacket(Packet *p);
public:
	NetworkRecvStatus CloseConnection(bool error = true);

	NetworkAdminSocketHandler(SOCKET s);
	~NetworkAdminSocketHandler();

	NetworkRecvStatus ReceivePackets();

	const char *ReceiveCommand(Packet *p, CommandPacket *cp);
	void SendCommand(Packet *p, const CommandPacket *cp);
};

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_TCP_ADMIN_H */
