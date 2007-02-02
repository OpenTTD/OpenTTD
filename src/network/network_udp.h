/* $Id$ */

#ifndef NETWORK_UDP_H
#define NETWORK_UDP_H

#ifdef ENABLE_NETWORK

void NetworkUDPInitialize(void);
void NetworkUDPSearchGame(void);
void NetworkUDPQueryMasterServer(void);
void NetworkUDPQueryServer(const char* host, unsigned short port, bool manually = false);
void NetworkUDPAdvertise(void);
void NetworkUDPRemoveAdvertise(void);
void NetworkUDPShutdown(void);

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_UDP_H */
