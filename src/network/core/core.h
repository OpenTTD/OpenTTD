/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file core.h Base for all network types (UDP and TCP)
 */

#ifndef NETWORK_CORE_CORE_H
#define NETWORK_CORE_CORE_H

#include "../../newgrf_config.h"
#include "config.h"

#ifdef ENABLE_NETWORK

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
	bool has_quit; ///< Whether the current client has quit/send a bad packet
public:
	/** Create a new unbound socket */
	NetworkSocketHandler() { this->has_quit = false; }

	/** Close the socket when distructing the socket handler */
	virtual ~NetworkSocketHandler() { this->Close(); }

	/** Really close the socket */
	virtual void Close() {}

	/**
	 * Close the current connection; for TCP this will be mostly equivalent
	 * to Close(), but for UDP it just means the packet has to be dropped.
	 * @param error Whether we quit under an error condition or not.
	 * @return new status of the connection.
	 */
	virtual NetworkRecvStatus CloseConnection(bool error = true) { this->has_quit = true; return NETWORK_RECV_STATUS_OKAY; }

	/**
	 * Whether the current client connected to the socket has quit.
	 * In the case of UDP, for example, once a client quits (send bad
	 * data), the socket in not closed; only the packet is dropped.
	 * @return true when the current client has quit, false otherwise
	 */
	bool HasClientQuit() const { return this->has_quit; }

	/**
	 * Reopen the socket so we can send/receive stuff again.
	 */
	void Reopen() { this->has_quit = false; }

	void SendGRFIdentifier(Packet *p, const GRFIdentifier *grf);
	void ReceiveGRFIdentifier(Packet *p, GRFIdentifier *grf);
	void SendCompanyInformation(Packet *p, const struct Company *c, const struct NetworkCompanyStats *stats, uint max_len = NETWORK_COMPANY_NAME_LENGTH);
};

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_CORE_H */
