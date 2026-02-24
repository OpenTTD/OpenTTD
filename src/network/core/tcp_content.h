/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tcp_content.h Basic functions to receive and send TCP packets to/from the content server. */

#ifndef NETWORK_CORE_TCP_CONTENT_H
#define NETWORK_CORE_TCP_CONTENT_H

#include "os_abstraction.h"
#include "tcp.h"
#include "packet.h"
#include "../../debug.h"
#include "tcp_content_type.h"

/**
 * Enum with all types of TCP content packets.
 * @important The order MUST not be changed.
 */
enum class PacketContentType : uint8_t {
	ClientInfoList, ///< Queries the content server for a list of info of a given content type
	ClientInfoID, ///< Queries the content server for information about a list of internal IDs
	ClientInfoExternalID, ///< Queries the content server for information about a list of external IDs
	ClientInfoExternalIDMD5, ///< Queries the content server for information about a list of external IDs and MD5
	ServerInfo, ///< Reply of content server with information about content
	ClientContent, ///< Request a content file given an internal ID
	ServerContent, ///< Reply with the content of the given ID
};
/** Mark PacketContentType as PacketType. */
template <> struct IsEnumPacketType<PacketContentType> {
	static constexpr bool value = true; ///< This is an enumeration of a PacketType.
};

/** Base socket handler for all Content TCP sockets */
class NetworkContentSocketHandler : public NetworkTCPSocketHandler {
protected:
	bool ReceiveInvalidPacket(PacketContentType type);

	/**
	 * Client requesting a list of content info:
	 *  uint8_t    type
	 *  uint32_t  openttd version (or 0xFFFFFFFF if using a list)
	 * Only if the above value is 0xFFFFFFFF:
	 *  uint8_t   count
	 *  string  branch-name ("vanilla" for upstream OpenTTD)
	 *  string  release version (like "12.0")
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool ReceiveClientInfoList(Packet &p);

	/**
	 * Client requesting a list of content info:
	 *  uint16_t  count of ids
	 *  uint32_t  id (count times)
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool ReceiveClientInfoID(Packet &p);

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
	virtual bool ReceiveClientInfoExternalID(Packet &p);

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
	virtual bool ReceiveClientInfoExternalIDMD5(Packet &p);

	/**
	 * Server sending list of content info:
	 *  uint8_t    type (invalid ID == does not exist)
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
	virtual bool ReceiveServerInfo(Packet &p);

	/**
	 * Client requesting the actual content:
	 *  uint16_t  count of unique ids
	 *  uint32_t  unique id (count times)
	 * @param p The packet that was just received.
	 * @return True upon success, otherwise false.
	 */
	virtual bool ReceiveClientContent(Packet &p);

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
	virtual bool ReceiveServerContent(Packet &p);

	bool HandlePacket(Packet &p);
public:
	/**
	 * Create a new cs socket handler for a given cs
	 * @param s The socket we are connected with.
	 */
	NetworkContentSocketHandler(SOCKET s = INVALID_SOCKET) :
		NetworkTCPSocketHandler(s)
	{
	}

	/** On destructing of this class, the socket needs to be closed */
	~NetworkContentSocketHandler() override
	{
		/* Virtual functions get called statically in destructors, so make it explicit to remove any confusion. */
		this->CloseSocket();
	}

	bool ReceivePackets();
};

Subdirectory GetContentInfoSubDir(ContentType type);

#endif /* NETWORK_CORE_TCP_CONTENT_H */
