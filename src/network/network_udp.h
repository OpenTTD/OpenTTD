/* $Id$ */

#ifndef NETWORK_UDP_H
#define NETWORK_UDP_H

#ifdef ENABLE_NETWORK

void NetworkUDPInitialize(void);
void NetworkUDPSearchGame(void);
void NetworkUDPQueryMasterServer(void);
NetworkGameList *NetworkUDPQueryServer(const char* host, unsigned short port);
void NetworkUDPAdvertise(void);
void NetworkUDPRemoveAdvertise(void);
void NetworkUDPShutdown(void);

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_UDP_H */
