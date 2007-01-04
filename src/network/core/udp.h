/* $Id$ */

#ifndef NETWORK_CORE_UDP_H
#define NETWORK_CORE_UDP_H

#ifdef ENABLE_NETWORK

/**
 * @file udp.h Basic functions to receive and send UDP packets.
 */

///** Sending/receiving of UDP packets **////

bool NetworkUDPListen(SOCKET *udp, uint32 host, uint16 port, bool broadcast);
void NetworkUDPClose(SOCKET *udp);

void NetworkSendUDP_Packet(SOCKET udp, Packet *p, struct sockaddr_in *recv);
void NetworkUDPReceive(SOCKET udp);

/**
 * Function that is called for every received UDP packet.
 * @param udp         the socket the packet is received on
 * @param packet      the received packet
 * @param client_addr the address of the sender of the packet
 */
void NetworkHandleUDPPacket(SOCKET udp, Packet *p, struct sockaddr_in *client_addr);


///** Sending/receiving of (large) chuncks of UDP packets **////


/** Enum with all types of UDP packets. The order MUST not be changed **/
enum {
	PACKET_UDP_CLIENT_FIND_SERVER,   ///< Queries a game server for game information
	PACKET_UDP_SERVER_RESPONSE,      ///< Reply of the game server with game information
	PACKET_UDP_CLIENT_DETAIL_INFO,   ///< Queries a game server about details of the game, such as companies
	PACKET_UDP_SERVER_DETAIL_INFO,   ///< Reply of the game server about details of the game, such as companies
	PACKET_UDP_SERVER_REGISTER,      ///< Packet to register itself to the master server
	PACKET_UDP_MASTER_ACK_REGISTER,  ///< Packet indicating registration has succedeed
	PACKET_UDP_CLIENT_GET_LIST,      ///< Request for serverlist from master server
	PACKET_UDP_MASTER_RESPONSE_LIST, ///< Response from master server with server ip's + port's
	PACKET_UDP_SERVER_UNREGISTER,    ///< Request to be removed from the server-list
	PACKET_UDP_CLIENT_GET_NEWGRFS,   ///< Requests the name for a list of GRFs (GRF_ID and MD5)
	PACKET_UDP_SERVER_NEWGRFS,       ///< Sends the list of NewGRF's requested.
	PACKET_UDP_END                   ///< Must ALWAYS be on the end of this list!! (period)
};

void NetworkSend_GRFIdentifier(Packet *p, const GRFConfig *c);
void NetworkSend_NetworkGameInfo(Packet *p, const NetworkGameInfo *info);

void NetworkRecv_GRFIdentifier(NetworkClientState *cs, Packet *p, GRFConfig *c);
void NetworkRecv_NetworkGameInfo(NetworkClientState *cs, Packet *p, NetworkGameInfo *info);

/**
 * Function that is called for every GRFConfig that is read when receiving
 * a NetworkGameInfo. Only grfid and md5sum are set, the rest is zero. This
 * function must set all appropriate fields. This GRF is later appended to
 * the grfconfig list of the NetworkGameInfo.
 * @param config the GRF to handle
 */
void HandleIncomingNetworkGameInfoGRFConfig(GRFConfig *config);

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_UDP_H */
