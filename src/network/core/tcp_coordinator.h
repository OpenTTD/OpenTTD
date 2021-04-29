/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_coordinator.h Basic functions to receive and send TCP packets to/from the Game Coordinator server.
 */

#ifndef NETWORK_CORE_TCP_COORDINATOR_H
#define NETWORK_CORE_TCP_COORDINATOR_H

#include "os_abstraction.h"
#include "tcp.h"
#include "packet.h"
#include "game_info.h"

/** Enum with all types of TCP Game Coordinator packets. The order MUST not be changed **/
enum PacketCoordinatorType {
	PACKET_COORDINATOR_SERVER_ERROR,          ///< Game Coordinator indicates there was an error.
	PACKET_COORDINATOR_CLIENT_REGISTER,       ///< Server registration.
	PACKET_COORDINATOR_SERVER_REGISTER_ACK,   ///< Game Coordinator accepts the registration.
	PACKET_COORDINATOR_CLIENT_UPDATE,         ///< Server sends an set intervals an update of the server.
	PACKET_COORDINATOR_END,                   ///< Must ALWAYS be on the end of this list!! (period)
};

/**
 * The type of connection the Game Coordinator can detect we have.
 */
enum ConnectionType {
	CONNECTION_TYPE_UNKNOWN,  ///< The Game Coordinator hasn't informed us yet what type of connection we have.
	CONNECTION_TYPE_ISOLATED, ///< The Game Coordinator failed to find a way to connect to your server. Nobody will be able to join.
	CONNECTION_TYPE_DIRECT,   ///< The Game Coordinator can directly connect to your server.
};

/**
 * The type of error from the Game Coordinator.
 */
enum NetworkCoordinatorErrorType {
	NETWORK_COORDINATOR_ERROR_UNKNOWN,             ///< There was an unknown error.
	NETWORK_COORDINATOR_ERROR_REGISTRATION_FAILED, ///< Your request for registration failed.
};

/** Base socket handler for all Game Coordinator TCP sockets */
class NetworkCoordinatorSocketHandler : public NetworkTCPSocketHandler {
protected:
	bool ReceiveInvalidPacket(PacketCoordinatorType type);

	/**
	 * Game Coordinator indicates there was an error. This can either be a
	 * permanent error causing the connection to be dropped, or in response
	 * to a request that is invalid.
	 *
	 *  uint8   Type of error (see NetworkCoordinatorErrorType).
	 *  string  Details of the error.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERVER_ERROR(Packet *p);

	/**
	 * Client is starting a multiplayer game and wants to let the
	 * Game Coordinator know.
	 *
	 *  uint8   Game Coordinator protocol version.
	 *  uint8   Type of game (0 = friends-only, 1 = public).
	 *  uint16  Local port of the server.
	 *  string  Join-key the server wants to use (can be empty; coordinator will assign a new join-key).
	 *  string  Secret that belongs to the join-key (empty if join-key is empty).
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_REGISTER(Packet *p);

	/**
	 * Game Coordinator acknowledges the registration.
	 *
	 *  string  Join-key that can be used to join this server.
	 *  string  Secret that belongs to the join-key (only needed if reusing the join-key on next CLIENT_REGISTER).
	 *  uint8   Type of connection was detected (see ConnectionType).
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERVER_REGISTER_ACK(Packet *p);

	/**
	 * Send an update of the current state of the server to the Game Coordinator.
	 *
	 *  uint8   Game Coordinator protocol version.
	 *  Serialized NetworkGameInfo. See game_info.hpp for details.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_UPDATE(Packet *p);

	bool HandlePacket(Packet *p);
public:
	/**
	 * Create a new cs socket handler for a given cs
	 * @param s  the socket we are connected with
	 * @param address IP etc. of the client
	 */
	NetworkCoordinatorSocketHandler(SOCKET s = INVALID_SOCKET) : NetworkTCPSocketHandler(s) {}

	bool ReceivePackets();
};

#endif /* NETWORK_CORE_TCP_COORDINATOR_H */
