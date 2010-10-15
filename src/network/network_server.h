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

/** Class for handling the server side of the game connection. */
class ServerNetworkGameSocketHandler : public NetworkGameSocketHandler {
protected:
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_JOIN);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_COMPANY_INFO);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_GAME_PASSWORD);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_COMPANY_PASSWORD);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_GETMAP);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_MAP_OK);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_ACK);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_COMMAND);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_CHAT);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_SET_PASSWORD);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_SET_NAME);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_QUIT);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_ERROR);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_RCON);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_NEWGRFS_CHECKED);
	DECLARE_GAME_RECEIVE_COMMAND(PACKET_CLIENT_MOVE);
public:
	ServerNetworkGameSocketHandler(SOCKET s);
	~ServerNetworkGameSocketHandler();
};

DEF_SERVER_SEND_COMMAND(PACKET_SERVER_MAP);
DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_ERROR_QUIT)(NetworkClientSocket *cs, ClientID client_id, NetworkErrorCode errorno);
DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_ERROR)(NetworkClientSocket *cs, NetworkErrorCode error);
DEF_SERVER_SEND_COMMAND(PACKET_SERVER_SHUTDOWN);
DEF_SERVER_SEND_COMMAND(PACKET_SERVER_NEWGAME);
DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_RCON)(NetworkClientSocket *cs, uint16 colour, const char *command);
DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_MOVE)(NetworkClientSocket *cs, uint16 client_id, CompanyID company_id);

void NetworkServer_ReadPackets(NetworkClientSocket *cs);
void NetworkServer_Tick(bool send_frame);

#else /* ENABLE_NETWORK */
/* Network function stubs when networking is disabled */

static inline void NetworkServerMonthlyLoop() {}
static inline void NetworkServerYearlyLoop() {}

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_SERVER_H */
