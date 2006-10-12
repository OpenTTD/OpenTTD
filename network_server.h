/* $Id$ */

#ifndef NETWORK_SERVER_H
#define NETWORK_SERVER_H

#ifdef ENABLE_NETWORK

DEF_SERVER_SEND_COMMAND(PACKET_SERVER_MAP);
DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_ERROR_QUIT)(NetworkClientState *cs, uint16 client_index, NetworkErrorCode errorno);
DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_ERROR)(NetworkClientState *cs, NetworkErrorCode error);
DEF_SERVER_SEND_COMMAND(PACKET_SERVER_SHUTDOWN);
DEF_SERVER_SEND_COMMAND(PACKET_SERVER_NEWGAME);
DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_RCON)(NetworkClientState *cs, uint16 color, const char *command);

bool NetworkFindName(char new_name[NETWORK_CLIENT_NAME_LENGTH]);
void NetworkServer_HandleChat(NetworkAction action, DestType desttype, int dest, const char *msg, uint16 from_index);

bool NetworkServer_ReadPackets(NetworkClientState *cs);
void NetworkServer_Tick(bool send_frame);
void NetworkServerMonthlyLoop(void);
void NetworkServerYearlyLoop(void);

static inline const char* GetPlayerIP(const NetworkClientInfo* ci)
{
	struct in_addr addr;

	addr.s_addr = ci->client_ip;
	return inet_ntoa(addr);
}

#else /* ENABLE_NETWORK */
/* Network function stubs when networking is disabled */

static inline void NetworkServerMonthlyLoop(void) {}
static inline void NetworkServerYearlyLoop(void) {}

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_SERVER_H */
