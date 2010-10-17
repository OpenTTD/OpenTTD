/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_admin.h Server part of the admin network protocol. */

#ifndef NETWORK_ADMIN_H
#define NETWORK_ADMIN_H

#ifdef ENABLE_NETWORK

#include "network_internal.h"
#include "core/tcp_listen.h"
#include "core/tcp_admin.h"

class ServerNetworkAdminSocketHandler;
typedef Pool<ServerNetworkAdminSocketHandler, AdminIndex, 2, MAX_ADMINS> NetworkAdminSocketPool;
extern NetworkAdminSocketPool _networkadminsocket_pool;

/** Class for handling the server side of the game connection. */
class ServerNetworkAdminSocketHandler : public NetworkAdminSocketPool::PoolItem<&_networkadminsocket_pool>, public NetworkAdminSocketHandler, public TCPListenHandler<ServerNetworkAdminSocketHandler, ADMIN_PACKET_SERVER_FULL, ADMIN_PACKET_SERVER_BANNED> {
protected:
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_ADMIN_JOIN);
	DECLARE_ADMIN_RECEIVE_COMMAND(ADMIN_PACKET_ADMIN_QUIT);

	NetworkRecvStatus SendProtocol();
public:
	NetworkAddress address;                                  ///< Address of the admin.

	ServerNetworkAdminSocketHandler(SOCKET s);
	~ServerNetworkAdminSocketHandler();

	NetworkRecvStatus SendError(NetworkErrorCode error);
	NetworkRecvStatus SendWelcome();
	NetworkRecvStatus SendNewGame();
	NetworkRecvStatus SendShutdown();

	static void Send();
	static void AcceptConnection(SOCKET s, const NetworkAddress &address);
	static bool AllowConnection();
	static void WelcomeAll();

	/**
	 * Get the name used by the listener.
	 * @return the name to show in debug logs and the like.
	 */
	static const char *GetName()
	{
		return "admin";
	}
};

#define FOR_ALL_ADMIN_SOCKETS_FROM(var, start) FOR_ALL_ITEMS_FROM(ServerNetworkAdminSocketHandler, adminsocket_index, var, start)
#define FOR_ALL_ADMIN_SOCKETS(var) FOR_ALL_ADMIN_SOCKETS_FROM(var, 0)

#endif /* ENABLE_NETWORK */
#endif /* NETWORK_ADMIN_H */
