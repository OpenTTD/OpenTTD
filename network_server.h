#ifndef NETWORK_SERVER_H
#define NETWORK_SERVER_H

#ifdef ENABLE_NETWORK

DEF_SERVER_SEND_COMMAND(PACKET_SERVER_MAP);
DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_ERROR_QUIT)(ClientState *cs, uint16 client_index, NetworkErrorCode errorno);
DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_ERROR)(ClientState *cs, NetworkErrorCode error);
DEF_SERVER_SEND_COMMAND(PACKET_SERVER_SHUTDOWN);
DEF_SERVER_SEND_COMMAND(PACKET_SERVER_NEWGAME);

bool NetworkFindName(char new_name[NETWORK_NAME_LENGTH]);
void NetworkServer_HandleChat(NetworkAction action, DestType desttype, int dest, const char *msg, byte from_index);

bool NetworkServer_ReadPackets(ClientState *cs);
void NetworkServer_Tick();

#endif /* ENABLE_NETWORK */

#endif // NETWORK_SERVER_H
