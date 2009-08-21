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

DEF_CLIENT_SEND_COMMAND(PACKET_CLIENT_GAME_INFO);
DEF_CLIENT_SEND_COMMAND(PACKET_CLIENT_COMPANY_INFO);
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_COMMAND)(const CommandPacket *cp);
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_ERROR)(NetworkErrorCode errorno);
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_QUIT)();
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_CHAT)(NetworkAction action, DestType desttype, int dest, const char *msg, int64 data);
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_PASSWORD)(NetworkPasswordType type, const char *password);
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_SET_PASSWORD)(const char *password);
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_SET_NAME)(const char *name);
DEF_CLIENT_SEND_COMMAND(PACKET_CLIENT_ACK);
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_RCON)(const char *pass, const char *command);
DEF_CLIENT_SEND_COMMAND_PARAM(PACKET_CLIENT_MOVE)(CompanyID company, const char *pass);

NetworkRecvStatus NetworkClient_ReadPackets(NetworkClientSocket *cs);
void NetworkClient_Connected();

extern CompanyID _network_join_as;

extern const char *_network_join_server_password;
extern const char *_network_join_company_password;

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CLIENT_H */
