/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_client.h Client part of the network protocol. */

#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#ifdef ENABLE_NETWORK

#include "network_internal.h"

/** Class for handling the client side of the game connection. */
class ClientNetworkGameSocketHandler : public NetworkGameSocketHandler {
protected:
	static ClientNetworkGameSocketHandler *my_client;

	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_FULL);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_BANNED);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_ERROR);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_COMPANY_INFO);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_CLIENT_INFO);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_NEED_GAME_PASSWORD);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_NEED_COMPANY_PASSWORD);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_WELCOME);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_WAIT);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_MAP);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_JOIN);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_FRAME);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_SYNC);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_COMMAND);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_CHAT);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_QUIT);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_ERROR_QUIT);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_SHUTDOWN);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_NEWGAME);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_RCON);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_CHECK_NEWGRFS);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_MOVE);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_COMPANY_UPDATE);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_SERVER_CONFIG_UPDATE);

	static NetworkRecvStatus SendNewGRFsOk();
	static NetworkRecvStatus SendGetMap();
	static NetworkRecvStatus SendMapOk();
public:
	ClientNetworkGameSocketHandler(SOCKET s);
	~ClientNetworkGameSocketHandler();

	static NetworkRecvStatus SendCompanyInformationQuery();

	static NetworkRecvStatus SendJoin();
	static NetworkRecvStatus SendCommand(const CommandPacket *cp);
	static NetworkRecvStatus SendError(NetworkErrorCode errorno);
	static NetworkRecvStatus SendQuit();
	static NetworkRecvStatus SendAck();

	static NetworkRecvStatus SendGamePassword(const char *password);
	static NetworkRecvStatus SendCompanyPassword(const char *password);

	static NetworkRecvStatus SendChat(NetworkAction action, DestType type, int dest, const char *msg, int64 data);
	static NetworkRecvStatus SendSetPassword(const char *password);
	static NetworkRecvStatus SendSetName(const char *name);
	static NetworkRecvStatus SendRCon(const char *password, const char *command);
	static NetworkRecvStatus SendMove(CompanyID company, const char *password);
};

typedef ClientNetworkGameSocketHandler MyClient;

void NetworkClient_Connected();

extern CompanyID _network_join_as;

extern const char *_network_join_server_password;
extern const char *_network_join_company_password;

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CLIENT_H */
