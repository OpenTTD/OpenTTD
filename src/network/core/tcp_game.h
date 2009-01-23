/* $Id$ */

/**
 * @file tcp_game.h Basic functions to receive and send TCP packets for game purposes.
 */

#ifndef NETWORK_CORE_TCP_GAME_H
#define NETWORK_CORE_TCP_GAME_H

#ifdef ENABLE_NETWORK

#include "os_abstraction.h"
#include "tcp.h"
#include "packet.h"

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
	PACKET_SERVER_MOVE,
	PACKET_CLIENT_MOVE,
	PACKET_SERVER_COMPANY_UPDATE,
	PACKET_SERVER_CONFIG_UPDATE,
	PACKET_END                   ///< Must ALWAYS be on the end of this list!! (period)
};

/** Packet that wraps a command */
struct CommandPacket;

/** Status of a client */
enum ClientStatus {
	STATUS_INACTIVE,   ///< The client is not connected nor active
	STATUS_AUTHORIZING,///< The client is authorizing
	STATUS_AUTH,       ///< The client is authorized
	STATUS_MAP_WAIT,   ///< The client is waiting as someone else is downloading the map
	STATUS_MAP,        ///< The client is downloading the map
	STATUS_DONE_MAP,   ///< The client has downloaded the map
	STATUS_PRE_ACTIVE, ///< The client is catching up the delayed frames
	STATUS_ACTIVE,     ///< The client is active within in the game
};


class NetworkClientSocket;
DECLARE_OLD_POOL(NetworkClientSocket, NetworkClientSocket, NCI_BITS_PER_POOL_BLOCK, MAX_CLIENT_SLOTS >> NCI_BITS_PER_POOL_BLOCK);

/** Base socket handler for all TCP sockets */
class NetworkClientSocket : public PoolItem<NetworkClientSocket, ClientIndex, &_NetworkClientSocket_pool>, public NetworkTCPSocketHandler {
/* TODO: rewrite into a proper class */
private:
	NetworkClientInfo *info;  ///< Client info related to this socket
public:
	ClientID client_id;       ///< Client identifier
	uint32 last_frame;        ///< Last frame we have executed
	uint32 last_frame_server; ///< Last frame the server has executed
	byte lag_test;            ///< Byte used for lag-testing the client

	ClientStatus status;      ///< Status of this client

	CommandPacket *command_queue; ///< The command-queue awaiting delivery

	NetworkRecvStatus CloseConnection();

	NetworkClientSocket(ClientID client_id = INVALID_CLIENT_ID);
	~NetworkClientSocket();

	inline bool IsValid() const { return this->IsConnected(); }
	inline void SetInfo(NetworkClientInfo *info) { assert(info != NULL && this->info == NULL); this->info = info; }
	inline NetworkClientInfo *GetInfo() const { return this->info; }

	const char *Recv_Command(Packet *p, CommandPacket *cp);
	void Send_Command(Packet *p, const CommandPacket *cp);
};

static inline bool IsValidNetworkClientSocketIndex(ClientIndex index)
{
	return (uint)index < GetNetworkClientSocketPoolSize() && GetNetworkClientSocket(index)->IsValid();
}

#define FOR_ALL_CLIENT_SOCKETS_FROM(d, start) for (d = GetNetworkClientSocket(start); d != NULL; d = (d->index + 1U < GetNetworkClientSocketPoolSize()) ? GetNetworkClientSocket(d->index + 1U) : NULL) if (d->IsValid())
#define FOR_ALL_CLIENT_SOCKETS(d) FOR_ALL_CLIENT_SOCKETS_FROM(d, 0)

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_TCP_GAME_H */
