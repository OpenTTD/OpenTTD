/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tcp_turn.h Basic functions to receive and send TCP packets to/from the TURN server. */

#ifndef NETWORK_CORE_TCP_TURN_H
#define NETWORK_CORE_TCP_TURN_H

#include "os_abstraction.h"
#include "tcp.h"
#include "packet.h"
#include "network_game_info.h"

/**
 * Enum with all types of TCP TURN packets.
 * @important The order MUST not be changed.
 */
enum class PacketTurnType : uint8_t {
	ServerError, ///< TURN server is unable to relay.
	ClientConnect, ///< Client (or OpenTTD server) is connecting to the TURN server.
	ServerConnected, ///< TURN server indicates the socket is now being relayed.
};
/** Mark PacketTurnType as PacketType. */
template <> struct IsEnumPacketType<PacketTurnType> {
	static constexpr bool value = true; ///< This is an enumeration of a PacketType.
};

/** Base socket handler for all TURN TCP sockets. */
class NetworkTurnSocketHandler : public NetworkTCPSocketHandler {
protected:
	bool ReceiveInvalidPacket(PacketTurnType type);

	/**
	 * TURN server was unable to connect the client or server based on the
	 * token. Most likely cause is an invalid token or the other side that
	 * hasn't connected in a reasonable amount of time.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool ReceiveServerError(Packet &p);

	/**
	 * Client (or OpenTTD server) wants to connect to the TURN server (on request by
	 * the Game Coordinator).
	 *
	 *  uint8_t   Game Coordinator protocol version.
	 *  string  Token to track the current TURN request.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool ReceiveClientConnect(Packet &p);

	/**
	 * TURN server has connected client and OpenTTD server together and will now relay
	 * all packets to each other. No further TURN packets should be send over
	 * this socket, and the socket should be handed over to the game protocol.
	 *
	 *  string  Hostname of the peer. This can be used to check if a client is not banned etc.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool ReceiveServerConnected(Packet &p);

	bool HandlePacket(Packet &p);
public:
	/**
	 * Create a new cs socket handler for a given cs.
	 * @param s The socket we are connected with.
	 */
	NetworkTurnSocketHandler(SOCKET s = INVALID_SOCKET) : NetworkTCPSocketHandler(s) {}

	bool ReceivePackets();
};

#endif /* NETWORK_CORE_TCP_TURN_H */
