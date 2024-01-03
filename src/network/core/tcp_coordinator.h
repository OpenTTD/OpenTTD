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
#include "network_game_info.h"

/**
 * Enum with all types of TCP Game Coordinator packets. The order MUST not be changed.
 *
 * GC     -> packets from Game Coordinator to either Client or Server.
 * SERVER -> packets from Server to Game Coordinator.
 * CLIENT -> packets from Client to Game Coordinator.
 * SERCLI -> packets from either the Server or Client to Game Coordinator.
 **/
enum PacketCoordinatorType {
	PACKET_COORDINATOR_GC_ERROR,              ///< Game Coordinator indicates there was an error.
	PACKET_COORDINATOR_SERVER_REGISTER,       ///< Server registration.
	PACKET_COORDINATOR_GC_REGISTER_ACK,       ///< Game Coordinator accepts the registration.
	PACKET_COORDINATOR_SERVER_UPDATE,         ///< Server sends an set intervals an update of the server.
	PACKET_COORDINATOR_CLIENT_LISTING,        ///< Client is requesting a listing of all public servers.
	PACKET_COORDINATOR_GC_LISTING,            ///< Game Coordinator returns a listing of all public servers.
	PACKET_COORDINATOR_CLIENT_CONNECT,        ///< Client wants to connect to a server based on an invite code.
	PACKET_COORDINATOR_GC_CONNECTING,         ///< Game Coordinator informs the client of the token assigned to the connection attempt.
	PACKET_COORDINATOR_SERCLI_CONNECT_FAILED, ///< Client/server tells the Game Coordinator the current connection attempt failed.
	PACKET_COORDINATOR_GC_CONNECT_FAILED,     ///< Game Coordinator informs client/server it has given up on the connection attempt.
	PACKET_COORDINATOR_CLIENT_CONNECTED,      ///< Client informs the Game Coordinator the connection with the server is established.
	PACKET_COORDINATOR_GC_DIRECT_CONNECT,     ///< Game Coordinator tells client to directly connect to the hostname:port of the server.
	PACKET_COORDINATOR_GC_STUN_REQUEST,       ///< Game Coordinator tells client/server to initiate a STUN request.
	PACKET_COORDINATOR_SERCLI_STUN_RESULT,    ///< Client/server informs the Game Coordinator of the result of the STUN request.
	PACKET_COORDINATOR_GC_STUN_CONNECT,       ///< Game Coordinator tells client/server to connect() reusing the STUN local address.
	PACKET_COORDINATOR_GC_NEWGRF_LOOKUP,      ///< Game Coordinator informs client about NewGRF lookup table updates needed for GC_LISTING.
	PACKET_COORDINATOR_GC_TURN_CONNECT,       ///< Game Coordinator tells client/server to connect to a specific TURN server.
	PACKET_COORDINATOR_END,                   ///< Must ALWAYS be on the end of this list!! (period)
};

/**
 * The type of connection the Game Coordinator can detect we have.
 */
enum ConnectionType {
	CONNECTION_TYPE_UNKNOWN,  ///< The Game Coordinator hasn't informed us yet what type of connection we have.
	CONNECTION_TYPE_ISOLATED, ///< The Game Coordinator failed to find a way to connect to your server. Nobody will be able to join.
	CONNECTION_TYPE_DIRECT,   ///< The Game Coordinator can directly connect to your server.
	CONNECTION_TYPE_STUN,     ///< The Game Coordinator can connect to your server via a STUN request.
	CONNECTION_TYPE_TURN,     ///< The Game Coordinator needs you to connect to a relay.
};

/**
 * The type of error from the Game Coordinator.
 */
enum NetworkCoordinatorErrorType {
	NETWORK_COORDINATOR_ERROR_UNKNOWN,              ///< There was an unknown error.
	NETWORK_COORDINATOR_ERROR_REGISTRATION_FAILED,  ///< Your request for registration failed.
	NETWORK_COORDINATOR_ERROR_INVALID_INVITE_CODE,  ///< The invite code given is invalid.
	NETWORK_COORDINATOR_ERROR_REUSE_OF_INVITE_CODE, ///< The invite code is used by another (newer) server.
};

/** Base socket handler for all Game Coordinator TCP sockets. */
class NetworkCoordinatorSocketHandler : public NetworkTCPSocketHandler {
protected:
	bool ReceiveInvalidPacket(PacketCoordinatorType type);

	/**
	 * Game Coordinator indicates there was an error. This can either be a
	 * permanent error causing the connection to be dropped, or in response
	 * to a request that is invalid.
	 *
	 *  uint8_t   Type of error (see NetworkCoordinatorErrorType).
	 *  string  Details of the error.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_GC_ERROR(Packet *p);

	/**
	 * Server is starting a multiplayer game and wants to let the
	 * Game Coordinator know.
	 *
	 *  uint8_t   Game Coordinator protocol version.
	 *  uint8_t   Type of game (see ServerGameType).
	 *  uint16_t  Local port of the server.
	 *  string  Invite code the server wants to use (can be empty; coordinator will assign a new invite code).
	 *  string  Secret that belongs to the invite code (empty if invite code is empty).
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERVER_REGISTER(Packet *p);

	/**
	 * Game Coordinator acknowledges the registration.
	 *
	 *  string  Invite code that can be used to join this server.
	 *  string  Secret that belongs to the invite code (only needed if reusing the invite code on next SERVER_REGISTER).
	 *  uint8_t   Type of connection was detected (see ConnectionType).
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_GC_REGISTER_ACK(Packet *p);

	/**
	 * Send an update of the current state of the server to the Game Coordinator.
	 *
	 *  uint8_t   Game Coordinator protocol version.
	 *  Serialized NetworkGameInfo. See game_info.hpp for details.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERVER_UPDATE(Packet *p);

	/**
	 * Client requests a list of all public servers.
	 *
	 *  uint8_t   Game Coordinator protocol version.
	 *  uint8_t   Game-info version used by this client.
	 *  string  Revision of the client.
	 *  uint32_t  (Game Coordinator protocol >= 4) Cursor as received from GC_NEWGRF_LOOKUP, or zero.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_LISTING(Packet *p);

	/**
	 * Game Coordinator replies with a list of all public servers. Multiple
	 * of these packets are received after a request till all servers are
	 * sent over. Last packet will have server count of 0.
	 *
	 *  uint16_t  Amount of public servers in this packet.
	 *  For each server:
	 *    string  Connection string for this server.
	 *    Serialized NetworkGameInfo. See game_info.hpp for details.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_GC_LISTING(Packet *p);

	/**
	 * Client wants to connect to a Server.
	 *
	 *  uint8_t   Game Coordinator protocol version.
	 *  string  Invite code of the Server to join.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_CONNECT(Packet *p);

	/**
	 * Game Coordinator informs the Client under what token it will start the
	 * attempt to connect the Server and Client together.
	 *
	 *  string  Token to track the current connect request.
	 *  string  Invite code of the Server to join.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_GC_CONNECTING(Packet *p);

	/**
	 * Client or Server failed to connect to the remote side.
	 *
	 *  uint8_t   Game Coordinator protocol version.
	 *  string  Token to track the current connect request.
	 *  uint8_t   Tracking number to track current connect request.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERCLI_CONNECT_FAILED(Packet *p);

	/**
	 * Game Coordinator informs the Client that it failed to find a way to
	 * connect the Client to the Server. Any open connections for this token
	 * should be closed now.
	 *
	 *  string  Token to track the current connect request.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_GC_CONNECT_FAILED(Packet *p);

	/**
	 * Client informs the Game Coordinator the connection with the Server is
	 * established. The Client will disconnect from the Game Coordinator next.
	 *
	 *  uint8_t   Game Coordinator protocol version.
	 *  string  Token to track the current connect request.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_CONNECTED(Packet *p);

	/**
	 * Game Coordinator requests that the Client makes a direct connection to
	 * the indicated peer, which is a Server.
	 *
	 *  string  Token to track the current connect request.
	 *  uint8_t   Tracking number to track current connect request.
	 *  string  Hostname of the peer.
	 *  uint16_t  Port of the peer.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_GC_DIRECT_CONNECT(Packet *p);

	/**
	 * Game Coordinator requests the client/server to do a STUN request to the
	 * STUN server. Important is to remember the local port these STUN requests
	 * are sent from, as this will be needed for later conenctions too.
	 * The client/server should do multiple STUN requests for every available
	 * interface that connects to the Internet (e.g., once for IPv4 and once
	 * for IPv6).
	 *
	 *  string  Token to track the current connect request.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_GC_STUN_REQUEST(Packet *p);

	/**
	 * Client/server informs the Game Coordinator the result of a STUN request.
	 *
	 *  uint8_t   Game Coordinator protocol version.
	 *  string  Token to track the current connect request.
	 *  uint8_t   Interface number, as given during STUN request.
	 *  bool    Whether the STUN connection was successful.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERCLI_STUN_RESULT(Packet *p);

	/**
	 * Game Coordinator informs the client/server of its STUN peer (the host:ip
	 * of the other side). It should start a connect() to this peer ASAP with
	 * the local address as used with the STUN request.
	 *
	 *  string  Token to track the current connect request.
	 *  uint8_t   Tracking number to track current connect request.
	 *  uint8_t   Interface number, as given during STUN request.
	 *  string  Host of the peer.
	 *  uint16_t  Port of the peer.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_GC_STUN_CONNECT(Packet *p);

	/**
	 * Game Coordinator informs the client of updates for the NewGRFs lookup table
	 * as used by the NewGRF deserialization in GC_LISTING.
	 * This packet is sent after a CLIENT_LISTING request, but before GC_LISTING.
	 *
	 *  uint32_t   Lookup table cursor.
	 *  uint16_t   Number of NewGRFs in the packet, with for each of the NewGRFs:
	 *      uint32_t   Lookup table index for the NewGRF.
	 *      uint32_t   Unique NewGRF ID.
	 *      byte[16] MD5 checksum of the NewGRF
	 *      string   Name of the NewGRF.
	 *
	 * The lookup table built using these packets are used by the deserialisation
	 * of the NewGRFs for servers in the GC_LISTING. These updates are additive,
	 * i.e. each update will add NewGRFs but never remove them. However, this
	 * lookup table is specific to the connection with the Game Coordinator, and
	 * should be considered invalid after disconnecting from the Game Coordinator.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_GC_NEWGRF_LOOKUP(Packet *p);

	/**
	 * Game Coordinator requests that we make a connection to the indicated
	 * peer, which is a TURN server.
	 *
	 *  string  Token to track the current connect request.
	 *  uint8_t   Tracking number to track current connect request.
	 *  string  Ticket to hand over to the TURN server.
	 *  string  Connection string of the TURN server.
	 *
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_GC_TURN_CONNECT(Packet *p);

	bool HandlePacket(Packet *p);
public:
	/**
	 * Create a new cs socket handler for a given cs.
	 * @param s The socket we are connected with.
	 */
	NetworkCoordinatorSocketHandler(SOCKET s = INVALID_SOCKET) : NetworkTCPSocketHandler(s) {}

	bool ReceivePackets();
};

#endif /* NETWORK_CORE_TCP_COORDINATOR_H */
