/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_game.h Basic functions to receive and send TCP packets for game purposes.
 */

#ifndef NETWORK_CORE_TCP_GAME_H
#define NETWORK_CORE_TCP_GAME_H

#include "os_abstraction.h"
#include "tcp.h"
#include "../network_type.h"
#include "../../core/pool_type.hpp"

#ifdef ENABLE_NETWORK

/**
 * Enum with all types of UDP packets.
 * The order of the first 4 packets MUST not be changed, as
 * it protects old clients from joining newer servers
 * (because SERVER_ERROR is the respond to a wrong revision)
 */
enum PacketGameType {
	PACKET_SERVER_FULL,
	PACKET_SERVER_BANNED,
	PACKET_CLIENT_JOIN,
	PACKET_SERVER_ERROR,
	PACKET_CLIENT_COMPANY_INFO,
	PACKET_SERVER_COMPANY_INFO,
	PACKET_SERVER_CLIENT_INFO,
	PACKET_SERVER_NEED_GAME_PASSWORD,
	PACKET_SERVER_NEED_COMPANY_PASSWORD,
	PACKET_CLIENT_GAME_PASSWORD,
	PACKET_CLIENT_COMPANY_PASSWORD,
	PACKET_SERVER_WELCOME,
	PACKET_CLIENT_GETMAP,
	PACKET_SERVER_WAIT,
	PACKET_SERVER_MAP,
	PACKET_CLIENT_MAP_OK,
	PACKET_SERVER_JOIN,
	PACKET_SERVER_FRAME,
	PACKET_SERVER_SYNC,
	PACKET_CLIENT_ACK,
	PACKET_CLIENT_COMMAND,
	PACKET_SERVER_COMMAND,
	PACKET_CLIENT_CHAT,
	PACKET_SERVER_CHAT,
	PACKET_CLIENT_SET_PASSWORD,
	PACKET_CLIENT_SET_NAME,
	PACKET_CLIENT_QUIT,
	PACKET_CLIENT_ERROR,
	PACKET_SERVER_QUIT,
	PACKET_SERVER_ERROR_QUIT,
	PACKET_SERVER_SHUTDOWN,
	PACKET_SERVER_NEWGAME,
	PACKET_SERVER_RCON,
	PACKET_CLIENT_RCON,
	PACKET_SERVER_CHECK_NEWGRFS,
	PACKET_CLIENT_NEWGRFS_CHECKED,
	PACKET_SERVER_MOVE,
	PACKET_CLIENT_MOVE,
	PACKET_SERVER_COMPANY_UPDATE,
	PACKET_SERVER_CONFIG_UPDATE,
	PACKET_END                   ///< Must ALWAYS be on the end of this list!! (period)
};

/** Packet that wraps a command */
struct CommandPacket;

/** A queue of CommandPackets. */
class CommandQueue {
	CommandPacket *first; ///< The first packet in the queue.
	CommandPacket *last;  ///< The last packet in the queue; only valid when first != NULL.
	uint count;           ///< The number of items in the queue.

public:
	/** Initialise the command queue. */
	CommandQueue() : first(NULL), last(NULL) {}
	/** Clear the command queue. */
	~CommandQueue() { this->Free(); }
	void Append(CommandPacket *p);
	CommandPacket *Pop();
	CommandPacket *Peek();
	void Free();
	/** Get the number of items in the queue. */
	uint Count() const { return this->count; }
};

/** Status of a client */
enum ClientStatus {
	STATUS_INACTIVE,     ///< The client is not connected nor active
	STATUS_NEWGRFS_CHECK, ///< The client is checking NewGRFs
	STATUS_AUTH_GAME,    ///< The client is authorizing with game (server) password
	STATUS_AUTH_COMPANY, ///< The client is authorizing with company password
	STATUS_AUTHORIZED,   ///< The client is authorized
	STATUS_MAP_WAIT,     ///< The client is waiting as someone else is downloading the map
	STATUS_MAP,          ///< The client is downloading the map
	STATUS_DONE_MAP,     ///< The client has downloaded the map
	STATUS_PRE_ACTIVE,   ///< The client is catching up the delayed frames
	STATUS_ACTIVE,       ///< The client is active within in the game
	STATUS_END           ///< Must ALWAYS be on the end of this list!! (period)
};

#define DECLARE_GAME_RECEIVE_COMMAND(type) virtual NetworkRecvStatus NetworkPacketReceive_## type ##_command(Packet *p)
#define DEF_GAME_RECEIVE_COMMAND(cls, type) NetworkRecvStatus cls ##NetworkGameSocketHandler::NetworkPacketReceive_ ## type ## _command(Packet *p)

/** Base socket handler for all TCP sockets */
class NetworkGameSocketHandler : public NetworkTCPSocketHandler {
/* TODO: rewrite into a proper class */
private:
	NetworkClientInfo *info;  ///< Client info related to this socket

protected:

	/**
	 * Notification that the server is full.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_FULL);

	/**
	 * Notification that the client trying to join is banned.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_BANNED);

	/**
	 * Try to join the server:
	 * string  OpenTTD revision (norev000 if no revision).
	 * string  Name of the client (max NETWORK_NAME_LENGTH).
	 * uint8   ID of the company to play as (1..MAX_COMPANIES).
	 * uint8   ID of the clients Language.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_JOIN);

	/**
	 * The client made an error:
	 * uint8   Error code caused (see NetworkErrorCode).
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_ERROR);

	/**
	 * Request company information (in detail).
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_COMPANY_INFO);

	/**
	 * Sends information about the companies (one packet per company):
	 * uint8   Version of the structure of this packet (NETWORK_COMPANY_INFO_VERSION).
	 * bool    Contains data (false marks the end of updates).
	 * uint8   ID of the company.
	 * string  Name of the company.
	 * uint32  Year the company was inaugurated.
	 * uint64  Value.
	 * uint64  Money.
	 * uint64  Income.
	 * uint16  Performance (last quarter).
	 * bool    Company is password protected.
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
	 * bool    Company is an AI.
	 * string  Client names (comma separated list)
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_COMPANY_INFO);

	/**
	 * Send information about a client:
	 * uint32  ID of the client (always unique on a server. 1 = server, 0 is invalid).
	 * uint8   ID of the company the client is playing as (255 for spectators).
	 * string  Name of the client.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_CLIENT_INFO);

	/**
	 * Indication to the client that the server needs a game password.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_NEED_GAME_PASSWORD);

	/**
	 * Indication to the client that the server needs a company password:
	 * uint32  Generation seed.
	 * string  Network ID of the server.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_NEED_COMPANY_PASSWORD);

	/**
	 * Send a password to the server to authorize:
	 * uint8   Password type (see NetworkPasswordType).
	 * string  The password.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_GAME_PASSWORD);

	/**
	 * Send a password to the server to authorize
	 * uint8   Password type (see NetworkPasswordType).
	 * string  The password.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_COMPANY_PASSWORD);

	/**
	 * The client is joined and ready to receive his map:
	 * uint32  Own client ID.
	 * uint32  Generation seed.
	 * string  Network ID of the server.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_WELCOME);

	/**
	 * Request the map from the server.
	 * uint32  NewGRF version (release versions of OpenTTD only).
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_GETMAP);

	/**
	 * Notification that another client is currently receiving the map:
	 * uint8   Number of clients awaiting the map.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_WAIT);

	/**
	 * Sends parts of the map to the client:
	 * uint8   packet type (MAP_PACKET_START, MAP_PACKET_NORMAL, MAP_PACKET_END).
	 * If MAP_PACKET_START:
	 *   uint32  Current frame.
	 *   uint32  Size of the map (in bytes).
	 * If MAP_PACKET_NORMAL:
	 *   Part of the map (until max size of packet).
	 * If MAP_PACKET_END:
	 *   No further data sent.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_MAP);

	/**
	 * Tell the server that we are done receiving/loading the map.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_MAP_OK);

	/**
	 * A client joined (PACKET_CLIENT_MAP_OK), what usually directly follows is a PACKET_SERVER_CLIENT_INFO:
	 * uint32  ID of the client that just joined the game.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_JOIN);

	/**
	 * Sends the current frame counter to the client:
	 * uint32  Frame counter
	 * uint32  Frame counter max (how far may the client walk before the server?)
	 * uint32  General seed 1 (dependant on compile settings, not default).
	 * uint32  General seed 2 (dependant on compile settings, not default).
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_FRAME);

	/**
	 * Sends a sync-check to the client:
	 * uint32  Frame counter.
	 * uint32  General seed 1.
	 * uint32  General seed 2 (dependant on compile settings, not default).
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_SYNC);

	/**
	 * Tell the server we are done with this frame:
	 * uint32  Current frame counter of the client.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_ACK);

	/**
	 * Send a DoCommand to the Server:
	 * uint8   ID of the company (0..MAX_COMPANIES-1).
	 * uint32  ID of the command (see command.h).
	 * uint32  P1 (free variables used in DoCommand).
	 * uint32  P2
	 * uint32  Tile where this is taking place.
	 * string  Text.
	 * uint8   ID of the callback.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_COMMAND);

	/**
	 * Sends a DoCommand to the client:
	 * uint8   ID of the company (0..MAX_COMPANIES-1).
	 * uint32  ID of the command (see command.h).
	 * uint32  P1 (free variable used in DoCommand).
	 * uint32  P2.
	 * uint32  Tile where this is taking place.
	 * string  Text.
	 * uint8   ID of the callback.
	 * uint32  Frame of execution.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_COMMAND);

	/**
	 * Sends a chat-packet to the server:
	 * uint8   ID of the action (see NetworkAction).
	 * uint8   ID of the destination type (see DestType).
	 * uint32  ID of the client or company (destination of the chat).
	 * string  Message (max NETWORK_CHAT_LENGTH).
	 * uint64  data (used e.g. for 'give money' actions).
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_CHAT);

	/**
	 * Sends a chat-packet to the client:
	 * uint8   ID of the action (see NetworkAction).
	 * uint32  ID of the client (origin of the chat).
	 * string  Message (max NETWORK_CHAT_LENGTH).
	 * uint64  data (used e.g. for 'give money' actions).
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_CHAT);

	/**
	 * Set the password for the clients current company:
	 * string  The password.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_SET_PASSWORD);

	/**
	 * Gives the client a new name:
	 * string  New name of the client.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_SET_NAME);

	/**
	 * The client is quiting the game.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_QUIT);

	/**
	 * The client made an error and is quiting the game.
	 * uint8   Error of the code caused (see NetworkErrorCode).
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_ERROR);

	/**
	 * Notification that a client left the game:
	 * uint32  ID of the client.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_QUIT);

	/**
	 * Inform all clients that one client made an error and thus has quit/been disconnected:
	 * uint32  ID of the client that caused the error.
	 * uint8   Code of the error caused (see NetworkErrorCode).
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_ERROR_QUIT);

	/**
	 * Let the clients know that the server is closing.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_SHUTDOWN);

	/**
	 * Let the clients know that the server is loading a new map.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_NEWGAME);

	/**
	 * Send the result of an issues RCon command back to the client:
	 * uint16  Colour code.
	 * string  Output of the RCon command
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_RCON);

	/**
	 * Send an RCon command to the server:
	 * string  RCon password.
	 * string  Command to be executed.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_RCON);

	/**
	 * Sends information about all used GRFs to the client:
	 * uint8   Amount of GRFs (the following data is repeated this many times, i.e. per GRF data).
	 * uint32  GRF ID
	 * 16 * uint8   MD5 checksum of the GRF
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_CHECK_NEWGRFS);

	/**
	 * Tell the server that we have the required GRFs
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_NEWGRFS_CHECKED);

	/**
	 * Move a client from one company into another:
	 * uint32  ID of the client.
	 * uint8   ID of the new company.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_MOVE);

	/**
	 * Request the server to move this client into another company:
	 * uint8   ID of the company the client wants to join.
	 * string  Password, if the company is password protected.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_MOVE);

	/**
	 * Update the clients knowledge of which company is password protected:
	 * uint16  Bitwise representation of each company
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_COMPANY_UPDATE);

	/**
	 * Update the clients knowledge of the max settings:
	 * uint8   Maximum number of companies allowed.
	 * uint8   Maximum number of spectators allowed.
	 */
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_CONFIG_UPDATE);

	NetworkRecvStatus HandlePacket(Packet *p);

	NetworkGameSocketHandler(SOCKET s);
public:
	ClientID client_id;          ///< Client identifier
	uint32 last_frame;           ///< Last frame we have executed
	uint32 last_frame_server;    ///< Last frame the server has executed
	CommandQueue incoming_queue; ///< The command-queue awaiting handling

	NetworkRecvStatus CloseConnection(bool error = true);
	virtual NetworkRecvStatus CloseConnection(NetworkRecvStatus status) = 0;
	virtual ~NetworkGameSocketHandler() {}

	inline void SetInfo(NetworkClientInfo *info) { assert(info != NULL && this->info == NULL); this->info = info; }
	inline NetworkClientInfo *GetInfo() const { return this->info; }

	NetworkRecvStatus Recv_Packets();

	const char *Recv_Command(Packet *p, CommandPacket *cp);
	void Send_Command(Packet *p, const CommandPacket *cp);
};

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_TCP_GAME_H */
