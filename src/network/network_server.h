/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_server.h Server part of the network protocol. */

#ifndef NETWORK_SERVER_H
#define NETWORK_SERVER_H

#ifdef ENABLE_NETWORK

#include "network_internal.h"
#include "core/tcp_listen.h"
#include "../thread/thread.h"

class ServerNetworkGameSocketHandler;
/** Make the code look slightliy nicer/simpler. */
typedef ServerNetworkGameSocketHandler NetworkClientSocket;
/** Pool with all client sockets. */
typedef Pool<NetworkClientSocket, ClientIndex, 8, MAX_CLIENT_SLOTS, PT_NCLIENT> NetworkClientSocketPool;
extern NetworkClientSocketPool _networkclientsocket_pool;

/** Class for handling the server side of the game connection. */
class ServerNetworkGameSocketHandler : public NetworkClientSocketPool::PoolItem<&_networkclientsocket_pool>, public NetworkGameSocketHandler, public TCPListenHandler<ServerNetworkGameSocketHandler, PACKET_SERVER_FULL, PACKET_SERVER_BANNED> {
protected:
	virtual NetworkRecvStatus Receive_CLIENT_JOIN(Packet *p);
	virtual NetworkRecvStatus Receive_CLIENT_COMPANY_INFO(Packet *p);
	virtual NetworkRecvStatus Receive_CLIENT_GAME_PASSWORD(Packet *p);
	virtual NetworkRecvStatus Receive_CLIENT_COMPANY_PASSWORD(Packet *p);
	virtual NetworkRecvStatus Receive_CLIENT_GETMAP(Packet *p);
	virtual NetworkRecvStatus Receive_CLIENT_MAP_OK(Packet *p);
	virtual NetworkRecvStatus Receive_CLIENT_ACK(Packet *p);
	virtual NetworkRecvStatus Receive_CLIENT_COMMAND(Packet *p);
	virtual NetworkRecvStatus Receive_CLIENT_CHAT(Packet *p);
	virtual NetworkRecvStatus Receive_CLIENT_SET_PASSWORD(Packet *p);
	virtual NetworkRecvStatus Receive_CLIENT_SET_NAME(Packet *p);
	virtual NetworkRecvStatus Receive_CLIENT_QUIT(Packet *p);
	virtual NetworkRecvStatus Receive_CLIENT_ERROR(Packet *p);
	virtual NetworkRecvStatus Receive_CLIENT_RCON(Packet *p);
	virtual NetworkRecvStatus Receive_CLIENT_NEWGRFS_CHECKED(Packet *p);
	virtual NetworkRecvStatus Receive_CLIENT_MOVE(Packet *p);

	NetworkRecvStatus SendCompanyInfo();
	NetworkRecvStatus SendNewGRFCheck();
	NetworkRecvStatus SendWelcome();
	NetworkRecvStatus SendWait();
	NetworkRecvStatus SendNeedGamePassword();
	NetworkRecvStatus SendNeedCompanyPassword();

public:
	/** Status of a client */
	enum ClientStatus {
		STATUS_INACTIVE,      ///< The client is not connected nor active.
		STATUS_NEWGRFS_CHECK, ///< The client is checking NewGRFs.
		STATUS_AUTH_GAME,     ///< The client is authorizing with game (server) password.
		STATUS_AUTH_COMPANY,  ///< The client is authorizing with company password.
		STATUS_AUTHORIZED,    ///< The client is authorized.
		STATUS_MAP_WAIT,      ///< The client is waiting as someone else is downloading the map.
		STATUS_MAP,           ///< The client is downloading the map.
		STATUS_DONE_MAP,      ///< The client has downloaded the map.
		STATUS_PRE_ACTIVE,    ///< The client is catching up the delayed frames.
		STATUS_ACTIVE,        ///< The client is active within in the game.
		STATUS_END            ///< Must ALWAYS be on the end of this list!! (period).
	};

	byte lag_test;               ///< Byte used for lag-testing the client
	byte last_token;             ///< The last random token we did send to verify the client is listening
	uint32 last_token_frame;     ///< The last frame we received the right token
	ClientStatus status;         ///< Status of this client
	CommandQueue outgoing_queue; ///< The command-queue awaiting delivery
	int receive_limit;           ///< Amount of bytes that we can receive at this moment

	Packet *savegame_packets;      ///< Packet queue of the savegame; send these "slowly" to the client.
	struct PacketWriter *savegame; ///< Writer used to write the savegame.
	ThreadMutex *savegame_mutex;   ///< Mutex for making threaded saving safe.
	NetworkAddress client_address; ///< IP-address of the client (so he can be banned)

	ServerNetworkGameSocketHandler(SOCKET s);
	~ServerNetworkGameSocketHandler();

	virtual Packet *ReceivePacket();
	virtual void SendPacket(Packet *packet);
	NetworkRecvStatus CloseConnection(NetworkRecvStatus status);
	void GetClientName(char *client_name, size_t size) const;

	NetworkRecvStatus SendMap();
	NetworkRecvStatus SendErrorQuit(ClientID client_id, NetworkErrorCode errorno);
	NetworkRecvStatus SendQuit(ClientID client_id);
	NetworkRecvStatus SendShutdown();
	NetworkRecvStatus SendNewGame();
	NetworkRecvStatus SendRConResult(uint16 colour, const char *command);
	NetworkRecvStatus SendMove(ClientID client_id, CompanyID company_id);

	NetworkRecvStatus SendClientInfo(NetworkClientInfo *ci);
	NetworkRecvStatus SendError(NetworkErrorCode error);
	NetworkRecvStatus SendChat(NetworkAction action, ClientID client_id, bool self_send, const char *msg, int64 data);
	NetworkRecvStatus SendJoin(ClientID client_id);
	NetworkRecvStatus SendFrame();
	NetworkRecvStatus SendSync();
	NetworkRecvStatus SendCommand(const CommandPacket *cp);
	NetworkRecvStatus SendCompanyUpdate();
	NetworkRecvStatus SendConfigUpdate();

	static void Send();
	static void AcceptConnection(SOCKET s, const NetworkAddress &address);
	static bool AllowConnection();

	/**
	 * Get the name used by the listener.
	 * @return the name to show in debug logs and the like.
	 */
	static const char *GetName()
	{
		return "server";
	}

	const char *GetClientIP();

	static ServerNetworkGameSocketHandler *GetByClientID(ClientID client_id);
};

void NetworkServer_Tick(bool send_frame);
void NetworkServerSetCompanyPassword(CompanyID company_id, const char *password, bool already_hashed = true);

/**
 * Iterate over all the sockets from a given starting point.
 * @param var The variable to iterate with.
 * @param start The start of the iteration.
 */
#define FOR_ALL_CLIENT_SOCKETS_FROM(var, start) FOR_ALL_ITEMS_FROM(NetworkClientSocket, clientsocket_index, var, start)

/**
 * Iterate over all the sockets.
 * @param var The variable to iterate with.
 */
#define FOR_ALL_CLIENT_SOCKETS(var) FOR_ALL_CLIENT_SOCKETS_FROM(var, 0)

#else /* ENABLE_NETWORK */
/* Network function stubs when networking is disabled */

static inline void NetworkServerMonthlyLoop() {}
static inline void NetworkServerYearlyLoop() {}

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_SERVER_H */
