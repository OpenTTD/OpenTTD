/* $Id$ */

/** @file network_server.h Server part of the network protocol. */

#ifndef NETWORK_SERVER_H
#define NETWORK_SERVER_H

#ifdef ENABLE_NETWORK

DEF_SERVER_SEND_COMMAND(PACKET_SERVER_MAP);
DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_ERROR_QUIT)(NetworkTCPSocketHandler *cs, uint16 client_index, NetworkErrorCode errorno);
DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_ERROR)(NetworkTCPSocketHandler *cs, NetworkErrorCode error);
DEF_SERVER_SEND_COMMAND(PACKET_SERVER_SHUTDOWN);
DEF_SERVER_SEND_COMMAND(PACKET_SERVER_NEWGAME);
DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_RCON)(NetworkTCPSocketHandler *cs, uint16 color, const char *command);

bool NetworkServer_ReadPackets(NetworkTCPSocketHandler *cs);
void NetworkServer_Tick(bool send_frame);

#else /* ENABLE_NETWORK */
/* Network function stubs when networking is disabled */

static inline void NetworkServerMonthlyLoop() {}
static inline void NetworkServerYearlyLoop() {}

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_SERVER_H */
