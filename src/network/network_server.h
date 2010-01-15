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
