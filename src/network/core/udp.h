/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file udp.h Basic functions to receive and send UDP packets.
 *
 *
 * *** Requesting game information from a server ***
 *
 * This describes the on-the-wire structure of the request and reply
 * packet of the NetworkGameInfo (see game.h) data.
 *
 * --- Points of attention ---
 *  - all > 1 byte integral values are written in little endian,
 *    unless specified otherwise.
 *      Thus, 0x01234567 would be sent as {0x67, 0x45, 0x23, 0x01}.
 *  - all sent strings are of variable length and terminated by a '\0'.
 *      Thus, the length of the strings is not sent.
 *  - years that are leap years in the 'days since X' to 'date' calculations:
 *     (year % 4 == 0) and ((year % 100 != 0) or (year % 400 == 0))
 *
 * --- Request ---
 * Bytes:  Description:
 *   2       size of the whole packet, in this case 3
 *   1       type of packet, in this case PACKET_UDP_CLIENT_FIND_SERVER (0)
 * This packet would look like: { 0x03, 0x00, 0x00 }
 *
 * --- Reply ---
 * Version: Bytes:  Description:
 *   all      2       size of the whole packet
 *   all      1       type of packet, in this case PACKET_UDP_SERVER_RESPONSE (1)
 *   all      1       the version of this packet's structure
 *
 *   4+       1       number of GRFs attached (n)
 *   4+       n * 20  unique identifier for GRF files. Constists of:
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
 */

#ifndef NETWORK_CORE_UDP_H
#define NETWORK_CORE_UDP_H

#include "address.h"
#include "game.h"
#include "packet.h"

#ifdef ENABLE_NETWORK

/** Enum with all types of UDP packets. The order MUST not be changed **/
enum PacketUDPType {
	PACKET_UDP_CLIENT_FIND_SERVER,   ///< Queries a game server for game information
	PACKET_UDP_SERVER_RESPONSE,      ///< Reply of the game server with game information
	PACKET_UDP_CLIENT_DETAIL_INFO,   ///< Queries a game server about details of the game, such as companies
	PACKET_UDP_SERVER_DETAIL_INFO,   ///< Reply of the game server about details of the game, such as companies
	PACKET_UDP_SERVER_REGISTER,      ///< Packet to register itself to the master server
	PACKET_UDP_MASTER_ACK_REGISTER,  ///< Packet indicating registration has succedeed
	PACKET_UDP_CLIENT_GET_LIST,      ///< Request for serverlist from master server
	PACKET_UDP_MASTER_RESPONSE_LIST, ///< Response from master server with server ip's + port's
	PACKET_UDP_SERVER_UNREGISTER,    ///< Request to be removed from the server-list
	PACKET_UDP_CLIENT_GET_NEWGRFS,   ///< Requests the name for a list of GRFs (GRF_ID and MD5)
	PACKET_UDP_SERVER_NEWGRFS,       ///< Sends the list of NewGRF's requested.
	PACKET_UDP_MASTER_SESSION_KEY,   ///< Sends a fresh session key to the client
	PACKET_UDP_END                   ///< Must ALWAYS be on the end of this list!! (period)
};

/** The types of server lists we can get */
enum ServerListType {
	SLT_IPv4 = 0,   ///< Get the IPv4 addresses
	SLT_IPv6 = 1,   ///< Get the IPv6 addresses
	SLT_AUTODETECT, ///< Autodetect the type based on the connection

	SLT_END = SLT_AUTODETECT ///< End of 'arrays' marker
};

#define DECLARE_UDP_RECEIVE_COMMAND(type) virtual void NetworkPacketReceive_## type ##_command(Packet *p, NetworkAddress *client_addr)
#define DEF_UDP_RECEIVE_COMMAND(cls, type) void cls ##NetworkUDPSocketHandler::NetworkPacketReceive_ ## type ## _command(Packet *p, NetworkAddress *client_addr)

/** Base socket handler for all UDP sockets */
class NetworkUDPSocketHandler : public NetworkSocketHandler {
protected:
	/** The address to bind to. */
	NetworkAddressList bind;
	/** The opened sockets. */
	SocketList sockets;

	NetworkRecvStatus CloseConnection(bool error = true);

	/* Declare all possible packets here. If it can be received by the
	 * a specific handler, it has to be implemented. */
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_CLIENT_FIND_SERVER);
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_SERVER_RESPONSE);
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_CLIENT_DETAIL_INFO);
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_SERVER_DETAIL_INFO);
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_SERVER_REGISTER);
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_MASTER_ACK_REGISTER);
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_CLIENT_GET_LIST);
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_MASTER_RESPONSE_LIST);
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_SERVER_UNREGISTER);
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_CLIENT_GET_NEWGRFS);
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_SERVER_NEWGRFS);
	DECLARE_UDP_RECEIVE_COMMAND(PACKET_UDP_MASTER_SESSION_KEY);

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
	NetworkUDPSocketHandler(NetworkAddressList *bind = NULL);

	/** On destructing of this class, the socket needs to be closed */
	virtual ~NetworkUDPSocketHandler() { this->Close(); }

	bool Listen();
	void Close();

	void SendPacket(Packet *p, NetworkAddress *recv, bool all = false, bool broadcast = false);
	void ReceivePackets();

	void SendNetworkGameInfo(Packet *p, const NetworkGameInfo *info);
	void ReceiveNetworkGameInfo(Packet *p, NetworkGameInfo *info);
};

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_UDP_H */
