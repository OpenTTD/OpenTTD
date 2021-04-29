/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_coordinator.h Part of the network protocol handling Game Coordinator requests. */

#ifndef NETWORK_COORDINATOR_H
#define NETWORK_COORDINATOR_H

#include "core/tcp_coordinator.h"

/**
 * Game Coordinator communication.
 *
 * For servers:
 *  - Server sends CLIENT_REGISTER.
 *  - GC probes server to check if it can directly connect, needs STUN, etc.
 *  - GC sends SERVER_REGISTER_ACK with type of connection.
 *  - Server sends every 30 seconds CLIENT_UPDATE.
 *
 * For clients (listing):
 *  - Client sends CLIENT_LISTING.
 *  - GC returns the full list of public servers via SERVER_LISTING (multiple packets).
 */

/** Class for handling the client side of the Game Coordinator connection. */
class ClientNetworkCoordinatorSocketHandler : public NetworkCoordinatorSocketHandler {
private:
	std::chrono::steady_clock::time_point next_update; ///< When to send the next update (if server and public).

protected:
	bool Receive_SERVER_ERROR(Packet *p) override;
	bool Receive_SERVER_REGISTER_ACK(Packet *p) override;
	bool Receive_SERVER_LISTING(Packet *p) override;

public:
	/** The idle timeout; when to close the connection because it's idle. */
	static constexpr std::chrono::seconds IDLE_TIMEOUT = std::chrono::seconds(60);

	std::chrono::steady_clock::time_point last_activity;  ///< The last time there was network activity.
	bool connecting; ///< Are we connecting to the Game Coordinator?

	ClientNetworkCoordinatorSocketHandler() : connecting(false) {}

	NetworkRecvStatus CloseConnection(bool error = true) override;
	void SendReceive();

	void Connect();

	void Register();
	void SendServerUpdate();
	void GetListing();
};

extern ClientNetworkCoordinatorSocketHandler _network_coordinator_client;

#endif /* NETWORK_COORDINATOR_H */
