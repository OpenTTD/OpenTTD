/* $Id$ */

/** @file network_udp.h Sending and receiving UDP messages. */

#ifndef NETWORK_UDP_H
#define NETWORK_UDP_H

#ifdef ENABLE_NETWORK

void NetworkUDPInitialize();
void NetworkUDPSearchGame();
void NetworkUDPQueryMasterServer();
void NetworkUDPQueryServer(const char* host, unsigned short port, bool manually = false);
void NetworkUDPAdvertise();
void NetworkUDPRemoveAdvertise();
void NetworkUDPShutdown();

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_UDP_H */
