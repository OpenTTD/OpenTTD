/* $Id$ */

/**
 * @file core.h Base for all network types (UDP and TCP)
 */

#ifndef NETWORK_CORE_H
#define NETWORK_CORE_H

#ifdef ENABLE_NETWORK

#include "os_abstraction.h"
#include "../../newgrf_config.h"

bool NetworkCoreInitialize();
void NetworkCoreShutdown();

/** Status of a network client; reasons why a client has quit */
enum NetworkRecvStatus {
	NETWORK_RECV_STATUS_OKAY,             ///< Everything is okay
	NETWORK_RECV_STATUS_DESYNC,           ///< A desync did occur
	NETWORK_RECV_STATUS_NEWGRF_MISMATCH,  ///< We did not have the required NewGRFs
	NETWORK_RECV_STATUS_SAVEGAME,         ///< Something went wrong (down)loading the savegame
	NETWORK_RECV_STATUS_CONN_LOST,        ///< The conection is 'just' lost
	NETWORK_RECV_STATUS_MALFORMED_PACKET, ///< We apparently send a malformed packet
	NETWORK_RECV_STATUS_SERVER_ERROR,     ///< The server told us we made an error
	NETWORK_RECV_STATUS_SERVER_FULL,      ///< The server is full
	NETWORK_RECV_STATUS_SERVER_BANNED,    ///< The server has banned us
	NETWORK_RECV_STATUS_CLOSE_QUERY,      ///< Done quering the server
};

/** Forward declaration due to circular dependencies */
struct Packet;

/**
 * SocketHandler for all network sockets in OpenTTD.
 */
class NetworkSocketHandler {
public:
	/* TODO: make socket & has_quit protected once the TCP stuff
	 *is in a real class too */
	bool has_quit; ///< Whether the current client has quit/send a bad packet
	SOCKET sock;   ///< The socket currently connected to
public:
	/** Create a new unbound socket */
	NetworkSocketHandler() { this->sock = INVALID_SOCKET; this->has_quit = false; }

	/** Close the socket when distructing the socket handler */
	virtual ~NetworkSocketHandler() { this->Close(); }

	/** Really close the socket */
	virtual void Close() {}

	/**
	 * Close the current connection; for TCP this will be mostly equivalent
	 * to Close(), but for UDP it just means the packet has to be dropped.
	 * @return new status of the connection.
	 */
	virtual NetworkRecvStatus CloseConnection() { this->has_quit = true; return NETWORK_RECV_STATUS_OKAY; }

	/**
	 * Whether this socket is currently bound to a socket.
	 * @return true when the socket is bound, false otherwise
	 */
	bool IsConnected() { return this->sock != INVALID_SOCKET; }

	/**
	 * Whether the current client connected to the socket has quit.
	 * In the case of UDP, for example, once a client quits (send bad
	 * data), the socket in not closed; only the packet is dropped.
	 * @return true when the current client has quit, false otherwise
	 */
	bool HasClientQuit() { return this->has_quit; }

	void Send_GRFIdentifier(Packet *p, const GRFIdentifier *grf);
	void Recv_GRFIdentifier(Packet *p, GRFIdentifier *grf);
};

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_H */
