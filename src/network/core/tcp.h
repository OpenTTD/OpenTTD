/* $Id$ */

/**
 * @file tcp.h Basic functions to receive and send TCP packets.
 */

#ifndef NETWORK_CORE_TCP_H
#define NETWORK_CORE_TCP_H

#ifdef ENABLE_NETWORK

#include "os_abstraction.h"
#include "core.h"
#include "packet.h"

/** Base socket handler for all TCP sockets */
class NetworkTCPSocketHandler : public NetworkSocketHandler {
private:
	Packet *packet_queue;     ///< Packets that are awaiting delivery
	Packet *packet_recv;      ///< Partially received packet
public:
	bool writable;            ///< Can we write to this socket?

	void Send_Packet(Packet *packet);
	bool Send_Packets();
	bool IsPacketQueueEmpty();

	Packet *Recv_Packet(NetworkRecvStatus *status);

	NetworkTCPSocketHandler(SOCKET s = INVALID_SOCKET);
	~NetworkTCPSocketHandler();
};

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_TCP_H */
