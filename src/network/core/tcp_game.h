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
#include <chrono>

/**
 * Enum with all types of TCP packets.
 * For the exact meaning, look at #NetworkGameSocketHandler.
 */
enum PacketGameType {
	/*
	 * These first four pair of packets (thus eight in
	 * total) must remain in this order for backward
	 * and forward compatibility between clients that
	 * are trying to join directly.
	 */

	/* Packets sent by socket accepting code without ever constructing a client socket instance. */
	PACKET_SERVER_FULL,                  ///< The server is full and has no place for you.
	PACKET_SERVER_BANNED,                ///< The server has banned you.

	/* Packets used by the client to join and an error message when the revision is wrong. */
	PACKET_CLIENT_JOIN,                  ///< The client telling the server it wants to join.
	PACKET_SERVER_ERROR,                 ///< Server sending an error message to the client.

	/* Unused packet types, formerly used for the pre-game lobby. */
	PACKET_CLIENT_UNUSED,                ///< Unused.
	PACKET_SERVER_UNUSED,                ///< Unused.

	/* Packets used to get the game info. */
	PACKET_SERVER_GAME_INFO,             ///< Information about the server.
	PACKET_CLIENT_GAME_INFO,             ///< Request information about the server.

	/*
	 * Packets after here assume that the client
	 * and server are running the same version. As
	 * such ordering is unimportant from here on.
	 *
	 * The following is the remainder of the packets
	 * sent as part of authenticating and getting
	 * the map and other important data.
	 */

	/* After the join step, the first is checking NewGRFs. */
	PACKET_SERVER_CHECK_NEWGRFS,         ///< Server sends NewGRF IDs and MD5 checksums for the client to check.
	PACKET_CLIENT_NEWGRFS_CHECKED,       ///< Client acknowledges that it has all required NewGRFs.

	/* Checking the game, and then company passwords. */
	PACKET_SERVER_NEED_GAME_PASSWORD,    ///< Server requests the (hashed) game password.
	PACKET_CLIENT_GAME_PASSWORD,         ///< Clients sends the (hashed) game password.
	PACKET_SERVER_NEED_COMPANY_PASSWORD, ///< Server requests the (hashed) company password.
	PACKET_CLIENT_COMPANY_PASSWORD,      ///< Client sends the (hashed) company password.

	/* The server welcomes the authenticated client and sends information of other clients. */
	PACKET_SERVER_WELCOME,               ///< Server welcomes you and gives you your #ClientID.
	PACKET_SERVER_CLIENT_INFO,           ///< Server sends you information about a client.

	/* Getting the savegame/map. */
	PACKET_CLIENT_GETMAP,                ///< Client requests the actual map.
	PACKET_SERVER_WAIT,                  ///< Server tells the client there are some people waiting for the map as well.
	PACKET_SERVER_MAP_BEGIN,             ///< Server tells the client that it is beginning to send the map.
	PACKET_SERVER_MAP_SIZE,              ///< Server tells the client what the (compressed) size of the map is.
	PACKET_SERVER_MAP_DATA,              ///< Server sends bits of the map to the client.
	PACKET_SERVER_MAP_DONE,              ///< Server tells it has just sent the last bits of the map to the client.
	PACKET_CLIENT_MAP_OK,                ///< Client tells the server that it received the whole map.

	PACKET_SERVER_JOIN,                  ///< Tells clients that a new client has joined.

	/*
	 * At this moment the client has the map and
	 * the client is fully authenticated. Now the
	 * normal communication starts.
	 */

	/* Game progress monitoring. */
	PACKET_SERVER_FRAME,                 ///< Server tells the client what frame it is in, and thus to where the client may progress.
	PACKET_CLIENT_ACK,                   ///< The client tells the server which frame it has executed.
	PACKET_SERVER_SYNC,                  ///< Server tells the client what the random state should be.

	/* Sending commands around. */
	PACKET_CLIENT_COMMAND,               ///< Client executed a command and sends it to the server.
	PACKET_SERVER_COMMAND,               ///< Server distributes a command to (all) the clients.

	/* Human communication! */
	PACKET_CLIENT_CHAT,                  ///< Client said something that should be distributed.
	PACKET_SERVER_CHAT,                  ///< Server distributing the message of a client (or itself).
	PACKET_SERVER_EXTERNAL_CHAT,         ///< Server distributing the message from external source.

	/* Remote console. */
	PACKET_CLIENT_RCON,                  ///< Client asks the server to execute some command.
	PACKET_SERVER_RCON,                  ///< Response of the executed command on the server.

	/* Moving a client.*/
	PACKET_CLIENT_MOVE,                  ///< A client would like to be moved to another company.
	PACKET_SERVER_MOVE,                  ///< Server tells everyone that someone is moved to another company.

	/* Configuration updates. */
	PACKET_CLIENT_SET_PASSWORD,          ///< A client (re)sets its company's password.
	PACKET_CLIENT_SET_NAME,              ///< A client changes its name.
	PACKET_SERVER_COMPANY_UPDATE,        ///< Information (password) of a company changed.
	PACKET_SERVER_CONFIG_UPDATE,         ///< Some network configuration important to the client changed.

	/* A server quitting this game. */
	PACKET_SERVER_NEWGAME,               ///< The server is preparing to start a new game.
	PACKET_SERVER_SHUTDOWN,              ///< The server is shutting down.

	/* A client quitting. */
	PACKET_CLIENT_QUIT,                  ///< A client tells the server it is going to quit.
	PACKET_SERVER_QUIT,                  ///< A server tells that a client has quit.
	PACKET_CLIENT_ERROR,                 ///< A client reports an error to the server.
	PACKET_SERVER_ERROR_QUIT,            ///< A server tells that a client has hit an error and did quit.

	PACKET_END,                          ///< Must ALWAYS be on the end of this list!! (period)
};

/** Packet that wraps a command */
struct CommandPacket;

/** A queue of CommandPackets. */
class CommandQueue {
	CommandPacket *first; ///< The first packet in the queue.
	CommandPacket *last;  ///< The last packet in the queue; only valid when first != nullptr.
	uint count;           ///< The number of items in the queue.

public:
	/** Initialise the command queue. */
	CommandQueue() : first(nullptr), last(nullptr), count(0) {}
	/** Clear the command queue. */
	~CommandQueue() { this->Free(); }
	void Append(CommandPacket *p);
	CommandPacket *Pop(bool ignore_paused = false);
	CommandPacket *Peek(bool ignore_paused = false);
	void Free();
	/** Get the number of items in the queue. */
	uint Count() const { return this->count; }
};

/** Base socket handler for all TCP sockets */
class NetworkGameSocketHandler : public NetworkTCPSocketHandler {
/* TODO: rewrite into a proper class */
private:
	NetworkClientInfo *info;          ///< Client info related to this socket
	bool is_pending_deletion = false; ///< Whether this socket is pending deletion

protected:
	NetworkRecvStatus ReceiveInvalidPacket(PacketGameType type);

	/**
	 * Notification that the server is full.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_FULL(Packet *p);

	/**
	 * Notification that the client trying to join is banned.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_BANNED(Packet *p);

	/**
	 * Try to join the server:
	 * string  OpenTTD revision (norev000 if no revision).
	 * string  Name of the client (max NETWORK_NAME_LENGTH).
	 * uint8_t   ID of the company to play as (1..MAX_COMPANIES).
	 * uint8_t   ID of the clients Language.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_CLIENT_JOIN(Packet *p);

	/**
	 * The client made an error:
	 * uint8_t   Error code caused (see NetworkErrorCode).
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_ERROR(Packet *p);

	/**
	 * Request game information.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_CLIENT_GAME_INFO(Packet *p);

	/**
	 * Sends information about the game.
	 * Serialized NetworkGameInfo. See game_info.h for details.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_GAME_INFO(Packet *p);

	/**
	 * Send information about a client:
	 * uint32_t  ID of the client (always unique on a server. 1 = server, 0 is invalid).
	 * uint8_t   ID of the company the client is playing as (255 for spectators).
	 * string  Name of the client.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_CLIENT_INFO(Packet *p);

	/**
	 * Indication to the client that the server needs a game password.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_NEED_GAME_PASSWORD(Packet *p);

	/**
	 * Indication to the client that the server needs a company password:
	 * uint32_t  Generation seed.
	 * string  Network ID of the server.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_NEED_COMPANY_PASSWORD(Packet *p);

	/**
	 * Send a password to the server to authorize:
	 * uint8_t   Password type (see NetworkPasswordType).
	 * string  The password.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_CLIENT_GAME_PASSWORD(Packet *p);

	/**
	 * Send a password to the server to authorize
	 * uint8_t   Password type (see NetworkPasswordType).
	 * string  The password.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_CLIENT_COMPANY_PASSWORD(Packet *p);

	/**
	 * The client is joined and ready to receive their map:
	 * uint32_t  Own client ID.
	 * uint32_t  Generation seed.
	 * string  Network ID of the server.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_WELCOME(Packet *p);

	/**
	 * Request the map from the server.
	 * uint32_t  NewGRF version (release versions of OpenTTD only).
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_CLIENT_GETMAP(Packet *p);

	/**
	 * Notification that another client is currently receiving the map:
	 * uint8_t   Number of clients waiting in front of you.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_WAIT(Packet *p);

	/**
	 * Sends that the server will begin with sending the map to the client:
	 * uint32_t  Current frame.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_MAP_BEGIN(Packet *p);

	/**
	 * Sends the size of the map to the client.
	 * uint32_t  Size of the (compressed) map (in bytes).
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_MAP_SIZE(Packet *p);

	/**
	 * Sends the data of the map to the client:
	 * Contains a part of the map (until max size of packet).
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_MAP_DATA(Packet *p);

	/**
	 * Sends that all data of the map are sent to the client:
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_MAP_DONE(Packet *p);

	/**
	 * Tell the server that we are done receiving/loading the map.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_CLIENT_MAP_OK(Packet *p);

	/**
	 * A client joined (PACKET_CLIENT_MAP_OK), what usually directly follows is a PACKET_SERVER_CLIENT_INFO:
	 * uint32_t  ID of the client that just joined the game.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_JOIN(Packet *p);

	/**
	 * Sends the current frame counter to the client:
	 * uint32_t  Frame counter
	 * uint32_t  Frame counter max (how far may the client walk before the server?)
	 * uint32_t  General seed 1 (dependent on compile settings, not default).
	 * uint32_t  General seed 2 (dependent on compile settings, not default).
	 * uint8_t   Random token to validate the client is actually listening (only occasionally present).
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_FRAME(Packet *p);

	/**
	 * Sends a sync-check to the client:
	 * uint32_t  Frame counter.
	 * uint32_t  General seed 1.
	 * uint32_t  General seed 2 (dependent on compile settings, not default).
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_SYNC(Packet *p);

	/**
	 * Tell the server we are done with this frame:
	 * uint32_t  Current frame counter of the client.
	 * uint8_t   The random token that the server sent in the PACKET_SERVER_FRAME packet.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_CLIENT_ACK(Packet *p);

	/**
	 * Send a DoCommand to the Server:
	 * uint8_t   ID of the company (0..MAX_COMPANIES-1).
	 * uint32_t  ID of the command (see command.h).
	 * <var>   Command specific buffer with encoded parameters of variable length.
	 *         The content differs per command and can change without notification.
	 * uint8_t   ID of the callback.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_CLIENT_COMMAND(Packet *p);

	/**
	 * Sends a DoCommand to the client:
	 * uint8_t   ID of the company (0..MAX_COMPANIES-1).
	 * uint32_t  ID of the command (see command.h).
	 * <var>   Command specific buffer with encoded parameters of variable length.
	 *         The content differs per command and can change without notification.
	 * uint8_t   ID of the callback.
	 * uint32_t  Frame of execution.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_COMMAND(Packet *p);

	/**
	 * Sends a chat-packet to the server:
	 * uint8_t   ID of the action (see NetworkAction).
	 * uint8_t   ID of the destination type (see DestType).
	 * uint32_t  ID of the client or company (destination of the chat).
	 * string  Message (max NETWORK_CHAT_LENGTH).
	 * uint64_t  data (used e.g. for 'give money' actions).
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_CLIENT_CHAT(Packet *p);

	/**
	 * Sends a chat-packet to the client:
	 * uint8_t   ID of the action (see NetworkAction).
	 * uint32_t  ID of the client (origin of the chat).
	 * string  Message (max NETWORK_CHAT_LENGTH).
	 * uint64_t  data (used e.g. for 'give money' actions).
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_CHAT(Packet *p);

	/**
	 * Sends a chat-packet for external source to the client:
	 * string  Name of the source this message came from.
	 * uint16_t  TextColour to use for the message.
	 * string  Name of the user who sent the messsage.
	 * string  Message (max NETWORK_CHAT_LENGTH).
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_EXTERNAL_CHAT(Packet *p);

	/**
	 * Set the password for the clients current company:
	 * string  The password.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_CLIENT_SET_PASSWORD(Packet *p);

	/**
	 * Gives the client a new name:
	 * string  New name of the client.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_CLIENT_SET_NAME(Packet *p);

	/**
	 * The client is quitting the game.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_CLIENT_QUIT(Packet *p);

	/**
	 * The client made an error and is quitting the game.
	 * uint8_t   Error of the code caused (see NetworkErrorCode).
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_CLIENT_ERROR(Packet *p);

	/**
	 * Notification that a client left the game:
	 * uint32_t  ID of the client.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_QUIT(Packet *p);

	/**
	 * Inform all clients that one client made an error and thus has quit/been disconnected:
	 * uint32_t  ID of the client that caused the error.
	 * uint8_t   Code of the error caused (see NetworkErrorCode).
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_ERROR_QUIT(Packet *p);

	/**
	 * Let the clients know that the server is closing.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_SHUTDOWN(Packet *p);

	/**
	 * Let the clients know that the server is loading a new map.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_NEWGAME(Packet *p);

	/**
	 * Send the result of an issues RCon command back to the client:
	 * uint16_t  Colour code.
	 * string  Output of the RCon command
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_RCON(Packet *p);

	/**
	 * Send an RCon command to the server:
	 * string  RCon password.
	 * string  Command to be executed.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_CLIENT_RCON(Packet *p);

	/**
	 * Sends information about all used GRFs to the client:
	 * uint8_t   Amount of GRFs (the following data is repeated this many times, i.e. per GRF data).
	 * uint32_t  GRF ID
	 * 16 * uint8_t   MD5 checksum of the GRF
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_CHECK_NEWGRFS(Packet *p);

	/**
	 * Tell the server that we have the required GRFs
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_CLIENT_NEWGRFS_CHECKED(Packet *p);

	/**
	 * Move a client from one company into another:
	 * uint32_t  ID of the client.
	 * uint8_t   ID of the new company.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_MOVE(Packet *p);

	/**
	 * Request the server to move this client into another company:
	 * uint8_t   ID of the company the client wants to join.
	 * string  Password, if the company is password protected.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_CLIENT_MOVE(Packet *p);

	/**
	 * Update the clients knowledge of which company is password protected:
	 * uint16_t  Bitwise representation of each company
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_COMPANY_UPDATE(Packet *p);

	/**
	 * Update the clients knowledge of the max settings:
	 * uint8_t   Maximum number of companies allowed.
	 * uint8_t   Maximum number of spectators allowed.
	 * @param p The packet that was just received.
	 */
	virtual NetworkRecvStatus Receive_SERVER_CONFIG_UPDATE(Packet *p);

	NetworkRecvStatus HandlePacket(Packet *p);

	NetworkGameSocketHandler(SOCKET s);
public:
	ClientID client_id;          ///< Client identifier
	uint32_t last_frame;           ///< Last frame we have executed
	uint32_t last_frame_server;    ///< Last frame the server has executed
	CommandQueue incoming_queue; ///< The command-queue awaiting handling
	std::chrono::steady_clock::time_point last_packet; ///< Time we received the last frame.

	NetworkRecvStatus CloseConnection(bool error = true) override;

	/**
	 * Close the network connection due to the given status.
	 * @param status The reason the connection got closed.
	 */
	virtual NetworkRecvStatus CloseConnection(NetworkRecvStatus status) = 0;
	virtual ~NetworkGameSocketHandler() = default;

	/**
	 * Sets the client info for this socket handler.
	 * @param info The new client info.
	 */
	inline void SetInfo(NetworkClientInfo *info)
	{
		assert(info != nullptr && this->info == nullptr);
		this->info = info;
	}

	/**
	 * Gets the client info of this socket handler.
	 * @return The client info.
	 */
	inline NetworkClientInfo *GetInfo() const
	{
		return this->info;
	}

	NetworkRecvStatus ReceivePackets();

	const char *ReceiveCommand(Packet *p, CommandPacket *cp);
	void SendCommand(Packet *p, const CommandPacket *cp);

	bool IsPendingDeletion() const { return this->is_pending_deletion; }

	void DeferDeletion();
	static void ProcessDeferredDeletions();
};

#endif /* NETWORK_CORE_TCP_GAME_H */
