/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_content.h Basic functions to receive and send TCP packets to/from the content server.
 */

#ifndef NETWORK_CORE_TCP_CONTENT_H
#define NETWORK_CORE_TCP_CONTENT_H

#include "os_abstraction.h"
#include "tcp.h"
#include "packet.h"
#include "../../debug.h"
#include "tcp_content_type.h"

/** Base socket handler for all Content TCP sockets */
class NetworkContentSocketHandler : public NetworkTCPSocketHandler {
protected:
	bool ReceiveInvalidPacket(PacketContentType type);

	/**
	 * Client requesting a list of content info:
	 *  byte    type
	 *  uint32_t  openttd version (or 0xFFFFFFFF if using a list)
	 * Only if the above value is 0xFFFFFFFF:
	 *  uint8_t   count
	 *  string  branch-name ("vanilla" for upstream OpenTTD)
	 *  string  release version (like "12.0")
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_INFO_LIST(Packet *p);

	/**
	 * Client requesting a list of content info:
	 *  uint16_t  count of ids
	 *  uint32_t  id (count times)
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_INFO_ID(Packet *p);

	/**
	 * Client requesting a list of content info based on an external
	 * 'unique' id; GRF ID for NewGRFS, shortname and for base
	 * graphics and AIs.
	 * Scenarios and AI libraries are not supported
	 *  uint8_t   count of requests
	 *  for each request:
	 *    uint8_t type
	 *    unique id (uint32_t)
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_INFO_EXTID(Packet *p);

	/**
	 * Client requesting a list of content info based on an external
	 * 'unique' id; GRF ID + MD5 checksum for NewGRFS, shortname and
	 * xor-ed MD5 checksums for base graphics and AIs.
	 * Scenarios and AI libraries are not supported
	 *  uint8_t   count of requests
	 *  for each request:
	 *    uint8_t type
	 *    unique id (uint32_t)
	 *    md5 (16 bytes)
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_INFO_EXTID_MD5(Packet *p);

	/**
	 * Server sending list of content info:
	 *  byte    type (invalid ID == does not exist)
	 *  uint32_t  id
	 *  uint32_t  file_size
	 *  string  name (max 32 characters)
	 *  string  version (max 16 characters)
	 *  uint32_t  unique id
	 *  uint8_t   md5sum (16 bytes)
	 *  uint8_t   dependency count
	 *  uint32_t  unique id of dependency (dependency count times)
	 *  uint8_t   tag count
	 *  string  tag (max 32 characters for tag count times)
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERVER_INFO(Packet *p);

	/**
	 * Client requesting the actual content:
	 *  uint16_t  count of unique ids
	 *  uint32_t  unique id (count times)
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_CLIENT_CONTENT(Packet *p);

	/**
	 * Server sending list of content info:
	 *  uint32_t  unique id
	 *  uint32_t  file size (0 == does not exist)
	 *  string  file name (max 48 characters)
	 * After this initial packet, packets with the actual data are send using
	 * the same packet type.
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool Receive_SERVER_CONTENT(Packet *p);

	bool HandlePacket(Packet *p);
public:
	/**
	 * Create a new cs socket handler for a given cs
	 * @param s  the socket we are connected with
	 * @param address IP etc. of the client
	 */
	NetworkContentSocketHandler(SOCKET s = INVALID_SOCKET) :
		NetworkTCPSocketHandler(s)
	{
	}

	/** On destructing of this class, the socket needs to be closed */
	virtual ~NetworkContentSocketHandler()
	{
		/* Virtual functions get called statically in destructors, so make it explicit to remove any confusion. */
		this->CloseSocket();
	}

	bool ReceivePackets();
};

Subdirectory GetContentInfoSubDir(ContentType type);

#endif /* NETWORK_CORE_TCP_CONTENT_H */
