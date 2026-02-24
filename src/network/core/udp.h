/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file udp.h Basic functions to receive and send UDP packets. */

#ifndef NETWORK_CORE_UDP_H
#define NETWORK_CORE_UDP_H

#include "address.h"
#include "packet.h"

/**
 * Enum with all types of UDP packets.
 * @important The order MUST not be changed.
 */
enum class PacketUDPType : uint8_t {
	ClientFindServer, ///< Queries a game server for game information
	ServerResponse, ///< Reply of the game server with game information
};
/** Mark PacketUDPType as PacketType. */
template <> struct IsEnumPacketType<PacketUDPType> {
	static constexpr bool value = true; ///< This is an enumeration of a PacketType.
};

/** Base socket handler for all UDP sockets */
class NetworkUDPSocketHandler : public NetworkSocketHandler {
protected:
	/** The address to bind to. */
	NetworkAddressList bind;
	/** The opened sockets. */
	SocketList sockets;

	void ReceiveInvalidPacket(PacketUDPType, NetworkAddress &client_addr);

	/**
	 * Queries to the server for information about the game.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void ReceiveClientFindServer(Packet &p, NetworkAddress &client_addr);

	/**
	 * Response to a query letting the client know we are here.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void ReceiveServerResponse(Packet &p, NetworkAddress &client_addr);

	void HandleUDPPacket(Packet &p, NetworkAddress &client_addr);
public:
	NetworkUDPSocketHandler(NetworkAddressList *bind = nullptr);

	/** On destructing of this class, the socket needs to be closed */
	~NetworkUDPSocketHandler() override { this->CloseSocket(); }

	bool Listen();
	void CloseSocket();

	void SendPacket(Packet &p, NetworkAddress &recv, bool all = false, bool broadcast = false);
	void ReceivePackets();
};

#endif /* NETWORK_CORE_UDP_H */
