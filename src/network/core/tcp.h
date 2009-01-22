/* $Id$ */

/**
 * @file tcp.h Basic functions to receive and send TCP packets.
 */

#ifndef NETWORK_CORE_TCP_H
#define NETWORK_CORE_TCP_H

#ifdef ENABLE_NETWORK

#include "os_abstraction.h"
#include "address.h"
#include "core.h"
#include "packet.h"

/** Base socket handler for all TCP sockets */
class NetworkTCPSocketHandler : public NetworkSocketHandler {
private:
	Packet *packet_queue;     ///< Packets that are awaiting delivery
	Packet *packet_recv;      ///< Partially received packet
public:
	bool writable;            ///< Can we write to this socket?

	virtual NetworkRecvStatus CloseConnection();
	void Send_Packet(Packet *packet);
	bool Send_Packets();
	bool IsPacketQueueEmpty();

	Packet *Recv_Packet(NetworkRecvStatus *status);

	NetworkTCPSocketHandler(SOCKET s = INVALID_SOCKET);
	~NetworkTCPSocketHandler();
};

/**
 * "Helper" class for creating TCP connections in a non-blocking manner
 */
class TCPConnecter {
private:
	class ThreadObject *thread; ///< Thread used to create the TCP connection
	bool connected;             ///< Whether we succeeded in making the connection
	bool aborted;               ///< Whether we bailed out (i.e. connection making failed)
	bool killed;                ///< Whether we got killed
	SOCKET sock;                ///< The socket we're connecting with

	/** The actual connection function */
	void Connect();

	/**
	 * Entry point for the new threads.
	 * @param param the TCPConnecter instance to call Connect on.
	 */
	static void ThreadEntry(void *param);

protected:
	/** Address we're connecting to */
	NetworkAddress address;

public:
	/**
	 * Create a new connecter for the given address
	 * @param address the (un)resolved address to connect to
	 */
	TCPConnecter(const NetworkAddress &address);
	/** Silence the warnings */
	virtual ~TCPConnecter() {}

	/**
	 * Callback when the connection succeeded.
	 * @param s the socket that we opened
	 */
	virtual void OnConnect(SOCKET s) {}

	/**
	 * Callback for when the connection attempt failed.
	 */
	virtual void OnFailure() {}

	/**
	 * Check whether we need to call the callback, i.e. whether we
	 * have connected or aborted and call the appropriate callback
	 * for that. It's done this way to ease on the locking that
	 * would otherwise be needed everywhere.
	 */
	static void CheckCallbacks();

	/** Kill all connection attempts. */
	static void KillAll();
};

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_TCP_H */
