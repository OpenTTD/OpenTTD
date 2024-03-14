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
#include "../network_crypto.h"
#include "config.h"

bool NetworkCoreInitialize();
void NetworkCoreShutdown();

/** Status of a network client; reasons why a client has quit */
enum NetworkRecvStatus {
	NETWORK_RECV_STATUS_OKAY,             ///< Everything is okay.
	NETWORK_RECV_STATUS_DESYNC,           ///< A desync did occur.
	NETWORK_RECV_STATUS_NEWGRF_MISMATCH,  ///< We did not have the required NewGRFs.
	NETWORK_RECV_STATUS_SAVEGAME,         ///< Something went wrong (down)loading the savegame.
	NETWORK_RECV_STATUS_CLIENT_QUIT,      ///< The connection is lost gracefully. Other clients are already informed of this leaving client.
	NETWORK_RECV_STATUS_MALFORMED_PACKET, ///< We apparently send a malformed packet.
	NETWORK_RECV_STATUS_SERVER_ERROR,     ///< The server told us we made an error.
	NETWORK_RECV_STATUS_SERVER_FULL,      ///< The server is full.
	NETWORK_RECV_STATUS_SERVER_BANNED,    ///< The server has banned us.
	NETWORK_RECV_STATUS_CLOSE_QUERY,      ///< Done querying the server.
	NETWORK_RECV_STATUS_CONNECTION_LOST,  ///< The connection is lost unexpectedly.
};

/** Forward declaration due to circular dependencies */
struct Packet;

/**
 * SocketHandler for all network sockets in OpenTTD.
 */
class NetworkSocketHandler {
private:
	bool has_quit; ///< Whether the current client has quit/send a bad packet

protected:
	friend struct Packet;
	std::unique_ptr<class NetworkEncryptionHandler> receive_encryption_handler; ///< The handler for decrypting received packets.
	std::unique_ptr<class NetworkEncryptionHandler> send_encryption_handler; ///< The handler for encrypting sent packets.

public:
	/** Create a new unbound socket */
	NetworkSocketHandler() { this->has_quit = false; }

	/** Close the socket when destructing the socket handler */
	virtual ~NetworkSocketHandler() = default;

	/**
	 * Mark the connection as closed.
	 *
	 * This doesn't mean the actual connection is closed, but just that we
	 * act like it is. This is useful for UDP, which doesn't normally close
	 * a socket, but its handler might need to pretend it does.
	 */
	void MarkClosed() { this->has_quit = true; }

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
};

#endif /* NETWORK_CORE_CORE_H */
