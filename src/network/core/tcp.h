/* $Id$ */

#ifndef NETWORK_CORE_TCP_H
#define NETWORK_CORE_TCP_H

#ifdef ENABLE_NETWORK

#include "os_abstraction.h"
#include "core.h"
#include "packet.h"

/**
 * @file tcp.h Basic functions to receive and send TCP packets.
 */

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
	PACKET_END                   ///< Must ALWAYS be on the end of this list!! (period)
};

typedef struct CommandPacket {
	struct CommandPacket *next;
	PlayerByte player; ///< player that is executing the command
	uint32 cmd;        ///< command being executed
	uint32 p1;         ///< parameter p1
	uint32 p2;         ///< parameter p2
	TileIndex tile;    ///< tile command being executed on
	char text[80];
	uint32 frame;      ///< the frame in which this packet is executed
	byte callback;     ///< any callback function executed upon successful completion of the command
} CommandPacket;

typedef enum {
	STATUS_INACTIVE,   ///< The client is not connected nor active
	STATUS_AUTH,       ///< The client is authorized
	STATUS_MAP_WAIT,   ///< The client is waiting as someone else is downloading the map
	STATUS_MAP,        ///< The client is downloading the map
	STATUS_DONE_MAP,   ///< The client has downloaded the map
	STATUS_PRE_ACTIVE, ///< The client is catching up the delayed frames
	STATUS_ACTIVE,     ///< The client is an active player in the game
} ClientStatus;

/** Base socket handler for all TCP sockets */
class NetworkTCPSocketHandler : public NetworkSocketHandler {
/* TODO: rewrite into a proper class */
public:
	uint16 index;
	uint32 last_frame;
	uint32 last_frame_server;
	byte lag_test; // This byte is used for lag-testing the client

	ClientStatus status;
	bool writable; // is client ready to write to?

	Packet *packet_queue; // Packets that are awaiting delivery
	Packet *packet_recv; // Partially received packet

	CommandPacket *command_queue; // The command-queue awaiting delivery

	NetworkRecvStatus CloseConnection();
	void Initialize();
};



void NetworkSend_Packet(Packet *packet, NetworkTCPSocketHandler *cs);
Packet *NetworkRecv_Packet(NetworkTCPSocketHandler *cs, NetworkRecvStatus *status);
bool NetworkSend_Packets(NetworkTCPSocketHandler *cs);

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_TCP_H */
