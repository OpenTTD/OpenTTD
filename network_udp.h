#ifndef NETWORK_LAN_H
#define NETWORK_LAN_H

#ifdef ENABLE_NETWORK

void NetworkUDPInitialize(void);
bool NetworkUDPListen(SOCKET *udp, uint32 host, uint16 port, bool broadcast);
void NetworkUDPReceive(SOCKET udp);
void NetworkUDPSearchGame(void);
void NetworkUDPQueryMasterServer(void);
NetworkGameList *NetworkUDPQueryServer(const byte* host, unsigned short port);
void NetworkUDPAdvertise(void);
void NetworkUDPRemoveAdvertise(void);

#endif

#endif /* NETWORK_LAN_H */
