/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file core/udp.h Basic functions to receive and send UDP packets.
 */

#ifndef NETWORK_CORE_UDP_H
#define NETWORK_CORE_UDP_H

#include "address.h"
#include "game.h"
#include "packet.h"

/** Enum with all types of UDP packets. The order MUST not be changed **/
enum PacketUDPType {
	PACKET_UDP_CLIENT_FIND_SERVER,   ///< Queries a game server for game information
	PACKET_UDP_SERVER_RESPONSE,      ///< Reply of the game server with game information
	PACKET_UDP_CLIENT_DETAIL_INFO,   ///< Queries a game server about details of the game, such as companies
	PACKET_UDP_SERVER_DETAIL_INFO,   ///< Reply of the game server about details of the game, such as companies
	PACKET_UDP_SERVER_REGISTER,      ///< Packet to register itself to the master server
	PACKET_UDP_MASTER_ACK_REGISTER,  ///< Packet indicating registration has succeeded
	PACKET_UDP_CLIENT_GET_LIST,      ///< Request for serverlist from master server
	PACKET_UDP_MASTER_RESPONSE_LIST, ///< Response from master server with server ip's + port's
	PACKET_UDP_SERVER_UNREGISTER,    ///< Request to be removed from the server-list
	PACKET_UDP_CLIENT_GET_NEWGRFS,   ///< Requests the name for a list of GRFs (GRF_ID and MD5)
	PACKET_UDP_SERVER_NEWGRFS,       ///< Sends the list of NewGRF's requested.
	PACKET_UDP_MASTER_SESSION_KEY,   ///< Sends a fresh session key to the client
	PACKET_UDP_END,                  ///< Must ALWAYS be on the end of this list!! (period)
};

/** The types of server lists we can get */
enum ServerListType {
	SLT_IPv4 = 0,   ///< Get the IPv4 addresses
	SLT_IPv6 = 1,   ///< Get the IPv6 addresses
	SLT_AUTODETECT, ///< Autodetect the type based on the connection

	SLT_END = SLT_AUTODETECT, ///< End of 'arrays' marker
};

/** Base socket handler for all UDP sockets */
class NetworkUDPSocketHandler : public NetworkSocketHandler {
protected:
	/** The address to bind to. */
	NetworkAddressList bind;
	/** The opened sockets. */
	SocketList sockets;

	NetworkRecvStatus CloseConnection(bool error = true) override;

	void ReceiveInvalidPacket(PacketUDPType, NetworkAddress *client_addr);

	/**
	 * Queries to the server for information about the game.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_CLIENT_FIND_SERVER(Packet *p, NetworkAddress *client_addr);

	/**
	 * Return of server information to the client.
	 * This packet has several legacy versions, so we list the version and size of each "field":
	 *
	 * Version: Bytes:  Description:
	 *   all      1       the version of this packet's structure
	 *
	 *   4+       1       number of GRFs attached (n)
	 *   4+       n * 20  unique identifier for GRF files. Consists of:
	 *                     - one 4 byte variable with the GRF ID
	 *                     - 16 bytes (sent sequentially) for the MD5 checksum
	 *                       of the GRF
	 *
	 *   3+       4       current game date in days since 1-1-0 (DMY)
	 *   3+       4       game introduction date in days since 1-1-0 (DMY)
	 *
	 *   2+       1       maximum number of companies allowed on the server
	 *   2+       1       number of companies on the server
	 *   2+       1       maximum number of spectators allowed on the server
	 *
	 *   1+       var     string with the name of the server
	 *   1+       var     string with the revision of the server
	 *   1+       1       the language run on the server
	 *                    (0 = any, 1 = English, 2 = German, 3 = French)
	 *   1+       1       whether the server uses a password (0 = no, 1 = yes)
	 *   1+       1       maximum number of clients allowed on the server
	 *   1+       1       number of clients on the server
	 *   1+       1       number of spectators on the server
	 *   1 & 2    2       current game date in days since 1-1-1920 (DMY)
	 *   1 & 2    2       game introduction date in days since 1-1-1920 (DMY)
	 *   1+       var     string with the name of the map
	 *   1+       2       width of the map in tiles
	 *   1+       2       height of the map in tiles
	 *   1+       1       type of map:
	 *                    (0 = temperate, 1 = arctic, 2 = desert, 3 = toyland)
	 *   1+       1       whether the server is dedicated (0 = no, 1 = yes)
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_SERVER_RESPONSE(Packet *p, NetworkAddress *client_addr);

	/**
	 * Query for detailed information about companies.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_CLIENT_DETAIL_INFO(Packet *p, NetworkAddress *client_addr);

	/**
	 * Reply with detailed company information.
	 * uint8   Version of the packet.
	 * uint8   Number of companies.
	 * For each company:
	 *   uint8   ID of the company.
	 *   string  Name of the company.
	 *   uint32  Year the company was inaugurated.
	 *   uint64  Value.
	 *   uint64  Money.
	 *   uint64  Income.
	 *   uint16  Performance (last quarter).
	 *   bool    Company is password protected.
	 *   uint16  Number of trains.
	 *   uint16  Number of lorries.
	 *   uint16  Number of busses.
	 *   uint16  Number of planes.
	 *   uint16  Number of ships.
	 *   uint16  Number of train stations.
	 *   uint16  Number of lorry stations.
	 *   uint16  Number of bus stops.
	 *   uint16  Number of airports and heliports.
	 *   uint16  Number of harbours.
	 *   bool    Company is an AI.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_SERVER_DETAIL_INFO(Packet *p, NetworkAddress *client_addr);

	/**
	 * Registers the server to the master server.
	 * string  The "welcome" message to root out other binary packets.
	 * uint8   Version of the protocol.
	 * uint16  The port to unregister.
	 * uint64  The session key.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_SERVER_REGISTER(Packet *p, NetworkAddress *client_addr);

	/**
	 * The master server acknowledges the registration.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_MASTER_ACK_REGISTER(Packet *p, NetworkAddress *client_addr);

	/**
	 * The client requests a list of servers.
	 * uint8   The protocol version.
	 * uint8   The type of server to look for: IPv4, IPv6 or based on the received packet.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_CLIENT_GET_LIST(Packet *p, NetworkAddress *client_addr);

	/**
	 * The server sends a list of servers.
	 * uint8   The protocol version.
	 * For each server:
	 *   4 or 16 bytes of IPv4 or IPv6 address.
	 *   uint8   The port.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_MASTER_RESPONSE_LIST(Packet *p, NetworkAddress *client_addr);

	/**
	 * A server unregisters itself at the master server.
	 * uint8   Version of the protocol.
	 * uint16  The port to unregister.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_SERVER_UNREGISTER(Packet *p, NetworkAddress *client_addr);

	/**
	 * The client requests information about some NewGRFs.
	 * uint8   The number of NewGRFs information is requested about.
	 * For each NewGRF:
	 *   uint32      The GRFID.
	 *   16 * uint8  MD5 checksum of the GRF.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_CLIENT_GET_NEWGRFS(Packet *p, NetworkAddress *client_addr);

	/**
	 * The server returns information about some NewGRFs.
	 * uint8   The number of NewGRFs information is requested about.
	 * For each NewGRF:
	 *   uint32      The GRFID.
	 *   16 * uint8  MD5 checksum of the GRF.
	 *   string      The name of the NewGRF.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_SERVER_NEWGRFS(Packet *p, NetworkAddress *client_addr);

	/**
	 * The master server sends us a session key.
	 * uint64  The session key.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_MASTER_SESSION_KEY(Packet *p, NetworkAddress *client_addr);

	void HandleUDPPacket(Packet *p, NetworkAddress *client_addr);

	/**
	 * Function that is called for every GRFConfig that is read when receiving
	 * a NetworkGameInfo. Only grfid and md5sum are set, the rest is zero. This
	 * function must set all appropriate fields. This GRF is later appended to
	 * the grfconfig list of the NetworkGameInfo.
	 * @param config the GRF to handle
	 */
	virtual void HandleIncomingNetworkGameInfoGRFConfig(GRFConfig *config) { NOT_REACHED(); }
public:
	NetworkUDPSocketHandler(const NetworkAddressList &bind);

	/** On destructing of this class, the socket needs to be closed */
	virtual ~NetworkUDPSocketHandler() { this->Close(); }

	bool Listen();
	void Close() override;

	void SendPacket(Packet *p, NetworkAddress *recv, bool all = false, bool broadcast = false);
	void ReceivePackets();

	void SendNetworkGameInfo(Packet *p, const NetworkGameInfo *info);
	void ReceiveNetworkGameInfo(Packet *p, NetworkGameInfo *info);
};

#endif /* NETWORK_CORE_UDP_H */
