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
	ADMIN_PACKET_ADMIN_GAMESCRIPT,       ///< The admin sends a JSON string for the GameScript.
	ADMIN_PACKET_ADMIN_PING,             ///< The admin sends a ping to the server, expecting a ping-reply (PONG) packet.
	ADMIN_PACKET_ADMIN_EXTERNAL_CHAT,    ///< The admin sends a chat message from external source.

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
	ADMIN_PACKET_SERVER_CMD_LOGGING_OLD, ///< Used to be the type ID of \c ADMIN_PACKET_SERVER_CMD_LOGGING in \c NETWORK_GAME_ADMIN_VERSION 1.
	ADMIN_PACKET_SERVER_GAMESCRIPT,      ///< The server gives the admin information from the GameScript in JSON.
	ADMIN_PACKET_SERVER_RCON_END,        ///< The server indicates that the remote console command has completed.
	ADMIN_PACKET_SERVER_PONG,            ///< The server replies to a ping request from the admin.
	ADMIN_PACKET_SERVER_CMD_LOGGING,     ///< The server gives the admin copies of incoming command packets.

	INVALID_ADMIN_PACKET = 0xFF,         ///< An invalid marker for admin packets.
};

/** Status of an admin. */
enum AdminStatus {
	ADMIN_STATUS_INACTIVE,      ///< The admin is not connected nor active.
	ADMIN_STATUS_ACTIVE,        ///< The admin is active.
	ADMIN_STATUS_END,           ///< Must ALWAYS be on the end of this list!! (period)
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
	ADMIN_UPDATE_GAMESCRIPT,      ///< The admin would like to have gamescript messages.
	ADMIN_UPDATE_END,             ///< Must ALWAYS be on the end of this list!! (period)
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
	ADMIN_CRR_BANKRUPT,  ///< The company went belly-up.

	ADMIN_CRR_END,       ///< Sentinel for end.
};

/** Main socket handler for admin related connections. */
class NetworkAdminSocketHandler : public NetworkTCPSocketHandler {
protected:
	std::string admin_name;    ///< Name of the admin.
	std::string admin_version; ///< Version string of the admin.
	AdminStatus status;        ///< Status of this admin.

	NetworkRecvStatus ReceiveInvalidPacket(PacketAdminType type);

	/**
	 * Join the admin network:
	 * string  Password the server is expecting for this network.
	 * string  Name of the application being used to connect.
	 * string  Version string of the application being used to connect.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_ADMIN_JOIN(Packet *p);

	/**
	 * Notification to the server that this admin is quitting.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_ADMIN_QUIT(Packet *p);

	/**
	 * Register updates to be sent at certain frequencies (as announced in the PROTOCOL packet):
	 * uint16_t  Update type (see #AdminUpdateType). Note integer type - see "Certain Packet Information" in docs/admin_network.md.
	 * uint16_t  Update frequency (see #AdminUpdateFrequency), setting #ADMIN_FREQUENCY_POLL is always ignored.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_ADMIN_UPDATE_FREQUENCY(Packet *p);

	/**
	 * Poll the server for certain updates, an invalid poll (e.g. not existent id) gets silently dropped:
	 * uint8_t   #AdminUpdateType the server should answer for, only if #AdminUpdateFrequency #ADMIN_FREQUENCY_POLL is advertised in the PROTOCOL packet. Note integer type - see "Certain Packet Information" in docs/admin_network.md.
	 * uint32_t  ID relevant to the packet type, e.g.
	 *          - the client ID for #ADMIN_UPDATE_CLIENT_INFO. Use UINT32_MAX to show all clients.
	 *          - the company ID for #ADMIN_UPDATE_COMPANY_INFO. Use UINT32_MAX to show all companies.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_ADMIN_POLL(Packet *p);

	/**
	 * Send chat as the server:
	 * uint8_t   Action such as NETWORK_ACTION_CHAT_CLIENT (see #NetworkAction).
	 * uint8_t   Destination type such as DESTTYPE_BROADCAST (see #DestType).
	 * uint32_t  ID of the destination such as company or client id.
	 * string  Message.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_ADMIN_CHAT(Packet *p);

	/**
	 * Send chat from the external source:
	 * string  Name of the source this message came from.
	 * uint16_t  TextColour to use for the message.
	 * string  Name of the user who sent the messsage.
	 * string  Message.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_ADMIN_EXTERNAL_CHAT(Packet *p);

	/**
	 * Execute a command on the servers console:
	 * string  Command to be executed.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_ADMIN_RCON(Packet *p);

	/**
	 * Send a JSON string to the current active GameScript.
	 * json  JSON string for the GameScript.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_ADMIN_GAMESCRIPT(Packet *p);

	/**
	 * Ping the server, requiring the server to reply with a pong packet.
	 * uint32_t Integer value to pass to the server, which is quoted in the reply.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_ADMIN_PING(Packet *p);

	/**
	 * The server is full (connection gets closed).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_FULL(Packet *p);

	/**
	 * The source IP address is banned (connection gets closed).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_BANNED(Packet *p);

	/**
	 * An error was caused by this admin connection (connection gets closed).
	 * uint8_t  NetworkErrorCode the error caused.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_ERROR(Packet *p);

	/**
	 * Inform a just joined admin about the protocol specifics:
	 * uint8_t   Protocol version.
	 * bool    Further protocol data follows (repeats through all update packet types).
	 * uint16_t  Update packet type.
	 * uint16_t  Frequencies allowed for this update packet (bitwise).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_PROTOCOL(Packet *p);

	/**
	 * Welcome a connected admin to the game:
	 * string  Name of the Server.
	 * string  OpenTTD version string.
	 * bool    Server is dedicated.
	 * string  Name of the Map.
	 * uint32_t  Random seed of the Map.
	 * uint8_t   Landscape of the Map.
	 * uint32_t  Start date of the Map.
	 * uint16_t  Map width.
	 * uint16_t  Map height.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_WELCOME(Packet *p);

	/**
	 * Notification about a newgame.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_NEWGAME(Packet *p);

	/**
	 * Notification about the server shutting down.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_SHUTDOWN(Packet *p);

	/**
	 * Send the current date of the game:
	 * uint32_t  Current game date.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_DATE(Packet *p);

	/**
	 * Notification of a new client:
	 * uint32_t  ID of the new client.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_CLIENT_JOIN(Packet *p);

	/**
	 * Client information of a specific client:
	 * uint32_t  ID of the client.
	 * string  Network address of the client.
	 * string  Name of the client.
	 * uint8_t   Language of the client.
	 * uint32_t  Date the client joined the game.
	 * uint8_t   ID of the company the client is playing as (255 for spectators).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_CLIENT_INFO(Packet *p);

	/**
	 * Client update details on a specific client (e.g. after rename or move):
	 * uint32_t  ID of the client.
	 * string  Name of the client.
	 * uint8_t   ID of the company the client is playing as (255 for spectators).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_CLIENT_UPDATE(Packet *p);

	/**
	 * Notification about a client leaving the game.
	 * uint32_t  ID of the client that just left.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_CLIENT_QUIT(Packet *p);

	/**
	 * Notification about a client error (and thus the clients disconnection).
	 * uint32_t  ID of the client that made the error.
	 * uint8_t   Error the client made (see NetworkErrorCode).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_CLIENT_ERROR(Packet *p);

	/**
	 * Notification of a new company:
	 * uint8_t   ID of the new company.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_COMPANY_NEW(Packet *p);

	/**
	 * Company information on a specific company:
	 * uint8_t   ID of the company.
	 * string  Name of the company.
	 * string  Name of the companies manager.
	 * uint8_t   Main company colour.
	 * bool    Company is password protected.
	 * uint32_t  Year the company was inaugurated.
	 * bool    Company is an AI.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_COMPANY_INFO(Packet *p);

	/**
	 * Company information of a specific company:
	 * uint8_t   ID of the company.
	 * string  Name of the company.
	 * string  Name of the companies manager.
	 * uint8_t   Main company colour.
	 * bool    Company is password protected.
	 * uint8_t   Quarters of bankruptcy.
	 * uint8_t   Owner of share 1.
	 * uint8_t   Owner of share 2.
	 * uint8_t   Owner of share 3.
	 * uint8_t   Owner of share 4.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_COMPANY_UPDATE(Packet *p);

	/**
	 * Notification about a removed company (e.g. due to bankruptcy).
	 * uint8_t   ID of the company.
	 * uint8_t   Reason for being removed (see #AdminCompanyRemoveReason).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_COMPANY_REMOVE(Packet *p);

	/**
	 * Economy update of a specific company:
	 * uint8_t   ID of the company.
	 * uint64_t  Money.
	 * uint64_t  Loan.
	 * int64_t   Income.
	 * uint16_t  Delivered cargo (this quarter).
	 * uint64_t  Company value (last quarter).
	 * uint16_t  Performance (last quarter).
	 * uint16_t  Delivered cargo (last quarter).
	 * uint64_t  Company value (previous quarter).
	 * uint16_t  Performance (previous quarter).
	 * uint16_t  Delivered cargo (previous quarter).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_COMPANY_ECONOMY(Packet *p);

	/**
	 * Company statistics on stations and vehicles:
	 * uint8_t   ID of the company.
	 * uint16_t  Number of trains.
	 * uint16_t  Number of lorries.
	 * uint16_t  Number of busses.
	 * uint16_t  Number of planes.
	 * uint16_t  Number of ships.
	 * uint16_t  Number of train stations.
	 * uint16_t  Number of lorry stations.
	 * uint16_t  Number of bus stops.
	 * uint16_t  Number of airports and heliports.
	 * uint16_t  Number of harbours.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_COMPANY_STATS(Packet *p);

	/**
	 * Send chat from the game into the admin network:
	 * uint8_t   Action such as NETWORK_ACTION_CHAT_CLIENT (see #NetworkAction).
	 * uint8_t   Destination type such as DESTTYPE_BROADCAST (see #DestType).
	 * uint32_t  ID of the client who sent this message.
	 * string  Message.
	 * uint64_t  Money (only when it is a 'give money' action).
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_CHAT(Packet *p);

	/**
	 * Result of an rcon command:
	 * uint16_t  Colour as it would be used on the server or a client.
	 * string  Output of the executed command.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_RCON(Packet *p);

	/**
	 * Send what would be printed on the server's console also into the admin network.
	 * string  The origin of the text, e.g. "console" for console, or "net" for network related (debug) messages.
	 * string  Text as found on the console of the server.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_CONSOLE(Packet *p);

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
	 * uint16_t  ID of the DoCommand.
	 * string  Name of DoCommand.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_CMD_NAMES(Packet *p);

	/**
	 * Send incoming command packets to the admin network.
	 * This is for logging purposes only.
	 *
	 * NOTICE: Data provided with this packet is not stable and will not be
	 *         treated as such. Do not rely on IDs or names to be constant
	 *         across different versions / revisions of OpenTTD.
	 *         Data provided in this packet is for logging purposes only.
	 *
	 * uint32_t  ID of the client sending the command.
	 * uint8_t   ID of the company (0..MAX_COMPANIES-1).
	 * uint16_t  ID of the command.
	 * <var>   Command specific buffer with encoded parameters of variable length.
	 *         The content differs per command and can change without notification.
	 * uint32_t  Frame of execution.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_CMD_LOGGING(Packet *p);

	/**
	 * Send a ping-reply (pong) to the admin that sent us the ping packet.
	 * uint32_t  Integer identifier - should be the same as read from the admins ping packet.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_PONG(Packet *p);

	/**
	 * Notify the admin connection that the rcon command has finished.
	 * string The command as requested by the admin connection.
	 * @param p The packet that was just received.
	 * @return The state the network should have.
	 */
	virtual NetworkRecvStatus Receive_SERVER_RCON_END(Packet *p);

	NetworkRecvStatus HandlePacket(Packet *p);
public:
	NetworkRecvStatus CloseConnection(bool error = true) override;

	NetworkAdminSocketHandler(SOCKET s);

	NetworkRecvStatus ReceivePackets();

	/**
	 * Get the status of the admin.
	 * @return The status of the admin.
	 */
	AdminStatus GetAdminStatus() const
	{
		return this->status;
	}
};

#endif /* NETWORK_CORE_TCP_ADMIN_H */
