/* $Id$ */

/**
 * @file tcp.h Basic functions to receive and send TCP packets.
 */

#ifndef NETWORK_CORE_TCP_H
#define NETWORK_CORE_TCP_H

#ifdef ENABLE_NETWORK

#include "os_abstraction.h"
#include "core.h"
#include "packet.h"
#include "../../tile_type.h"

/**
 * Enum with all types of UDP packets.
 * The order of the first 4 packets MUST not be changed, as
 * it protects old clients from joining newer servers
 * (because SERVER_ERROR is the respond to a wrong revision)
 */
enum {
	PACKET_SERVER_FULL,
	PACKET_SERVER_BANNED,
	PACKET_CLIENT_JOIN,
	PACKET_SERVER_ERROR,
	PACKET_CLIENT_COMPANY_INFO,
	PACKET_SERVER_COMPANY_INFO,
	PACKET_SERVER_CLIENT_INFO,
	PACKET_SERVER_NEED_PASSWORD,
	PACKET_CLIENT_PASSWORD,
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
	PACKET_END                   ///< Must ALWAYS be on the end of this list!! (period)
};

/** Packet that wraps a command */
struct CommandPacket {
	CommandPacket *next; ///< the next command packet (if in queue)
	PlayerByte player; ///< player that is executing the command
	uint32 cmd;        ///< command being executed
	uint32 p1;         ///< parameter p1
	uint32 p2;         ///< parameter p2
	TileIndex tile;    ///< tile command being executed on
	char text[80];     ///< possible text sent for name changes etc
	uint32 frame;      ///< the frame in which this packet is executed
	byte callback;     ///< any callback function executed upon successful completion of the command
	bool my_cmd;       ///< did the command originate from "me"
};

/** Status of a client */
enum ClientStatus {
	STATUS_INACTIVE,   ///< The client is not connected nor active
	STATUS_AUTHORIZING,///< The client is authorizing
	STATUS_AUTH,       ///< The client is authorized
	STATUS_MAP_WAIT,   ///< The client is waiting as someone else is downloading the map
	STATUS_MAP,        ///< The client is downloading the map
	STATUS_DONE_MAP,   ///< The client has downloaded the map
	STATUS_PRE_ACTIVE, ///< The client is catching up the delayed frames
	STATUS_ACTIVE,     ///< The client is an active player in the game
};

/** Base socket handler for all TCP sockets */
class NetworkTCPSocketHandler : public NetworkSocketHandler {
/* TODO: rewrite into a proper class */
private:
	Packet *packet_queue;     ///< Packets that are awaiting delivery
	Packet *packet_recv;      ///< Partially received packet
public:
	uint16 index;             ///< Client index
	uint32 last_frame;        ///< Last frame we have executed
	uint32 last_frame_server; ///< Last frame the server has executed
	byte lag_test;            ///< Byte used for lag-testing the client

	ClientStatus status;      ///< Status of this client
	bool writable;            ///< Can we write to this socket?

	CommandPacket *command_queue; ///< The command-queue awaiting delivery

	NetworkRecvStatus CloseConnection();
	void Initialize();
	void Destroy();

	void Send_Packet(Packet *packet);
	bool Send_Packets();
	bool IsPacketQueueEmpty();

	Packet *Recv_Packet(NetworkRecvStatus *status);
};

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_TCP_H */
