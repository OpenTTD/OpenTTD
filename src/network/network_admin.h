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

#include "network_internal.h"
#include "core/tcp_listen.h"
#include "core/tcp_admin.h"

extern AdminIndex _redirect_console_to_admin;

class ServerNetworkAdminSocketHandler;
/** Pool with all admin connections. */
typedef Pool<ServerNetworkAdminSocketHandler, AdminIndex, 2, MAX_ADMINS, PT_NADMIN> NetworkAdminSocketPool;
extern NetworkAdminSocketPool _networkadminsocket_pool;

/** Class for handling the server side of the game connection. */
class ServerNetworkAdminSocketHandler : public NetworkAdminSocketPool::PoolItem<&_networkadminsocket_pool>, public NetworkAdminSocketHandler, public TCPListenHandler<ServerNetworkAdminSocketHandler, ADMIN_PACKET_SERVER_FULL, ADMIN_PACKET_SERVER_BANNED> {
protected:
	NetworkRecvStatus Receive_ADMIN_JOIN(Packet *p) override;
	NetworkRecvStatus Receive_ADMIN_QUIT(Packet *p) override;
	NetworkRecvStatus Receive_ADMIN_UPDATE_FREQUENCY(Packet *p) override;
	NetworkRecvStatus Receive_ADMIN_POLL(Packet *p) override;
	NetworkRecvStatus Receive_ADMIN_CHAT(Packet *p) override;
	NetworkRecvStatus Receive_ADMIN_RCON(Packet *p) override;
	NetworkRecvStatus Receive_ADMIN_GAMESCRIPT(Packet *p) override;
	NetworkRecvStatus Receive_ADMIN_PING(Packet *p) override;

	NetworkRecvStatus SendProtocol();
	NetworkRecvStatus SendPong(uint32 d1);
public:
	AdminUpdateFrequency update_frequency[ADMIN_UPDATE_END]; ///< Admin requested update intervals.
	uint32 realtime_connect;                                 ///< Time of connection.
	NetworkAddress address;                                  ///< Address of the admin.

	ServerNetworkAdminSocketHandler(SOCKET s);
	~ServerNetworkAdminSocketHandler();

	NetworkRecvStatus SendError(NetworkErrorCode error);
	NetworkRecvStatus SendWelcome();
	NetworkRecvStatus SendNewGame();
	NetworkRecvStatus SendShutdown();

	NetworkRecvStatus SendDate();
	NetworkRecvStatus SendClientJoin(ClientID client_id);
	NetworkRecvStatus SendClientInfo(const NetworkClientSocket *cs, const NetworkClientInfo *ci);
	NetworkRecvStatus SendClientUpdate(const NetworkClientInfo *ci);
	NetworkRecvStatus SendClientQuit(ClientID client_id);
	NetworkRecvStatus SendClientError(ClientID client_id, NetworkErrorCode error);
	NetworkRecvStatus SendCompanyNew(CompanyID company_id);
	NetworkRecvStatus SendCompanyInfo(const Company *c);
	NetworkRecvStatus SendCompanyUpdate(const Company *c);
	NetworkRecvStatus SendCompanyRemove(CompanyID company_id, AdminCompanyRemoveReason bcrr);
	NetworkRecvStatus SendCompanyEconomy();
	NetworkRecvStatus SendCompanyStats();

	NetworkRecvStatus SendChat(NetworkAction action, DestType desttype, ClientID client_id, const char *msg, int64 data);
	NetworkRecvStatus SendRcon(uint16 colour, const char *command);
	NetworkRecvStatus SendConsole(const char *origin, const char *command);
	NetworkRecvStatus SendGameScript(const char *json);
	NetworkRecvStatus SendCmdNames();
	NetworkRecvStatus SendCmdLogging(ClientID client_id, const CommandPacket *cp);
	NetworkRecvStatus SendRconEnd(const char *command);

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

/**
 * Iterate over all the sockets from a given starting point.
 * @param var The variable to iterate with.
 * @param start The start of the iteration.
 */
#define FOR_ALL_ADMIN_SOCKETS_FROM(var, start) FOR_ALL_ITEMS_FROM(ServerNetworkAdminSocketHandler, adminsocket_index, var, start)

/**
 * Iterate over all the sockets.
 * @param var The variable to iterate with.
 */
#define FOR_ALL_ADMIN_SOCKETS(var) FOR_ALL_ADMIN_SOCKETS_FROM(var, 0)

/**
 * Iterate over all the active sockets.
 * @param var The variable to iterate with.
 */
#define FOR_ALL_ACTIVE_ADMIN_SOCKETS(var) \
	FOR_ALL_ADMIN_SOCKETS(var) \
		if (var->GetAdminStatus() == ADMIN_STATUS_ACTIVE)

void NetworkAdminClientInfo(const NetworkClientSocket *cs, bool new_client = false);
void NetworkAdminClientUpdate(const NetworkClientInfo *ci);
void NetworkAdminClientQuit(ClientID client_id);
void NetworkAdminClientError(ClientID client_id, NetworkErrorCode error_code);
void NetworkAdminCompanyInfo(const Company *company, bool new_company);
void NetworkAdminCompanyUpdate(const Company *company);
void NetworkAdminCompanyRemove(CompanyID company_id, AdminCompanyRemoveReason bcrr);

void NetworkAdminChat(NetworkAction action, DestType desttype, ClientID client_id, const char *msg, int64 data = 0, bool from_admin = false);
void NetworkAdminUpdate(AdminUpdateFrequency freq);
void NetworkServerSendAdminRcon(AdminIndex admin_index, TextColour colour_code, const char *string);
void NetworkAdminConsole(const char *origin, const char *string);
void NetworkAdminGameScript(const char *json);
void NetworkAdminCmdLogging(const NetworkClientSocket *owner, const CommandPacket *cp);

#endif /* NETWORK_ADMIN_H */
