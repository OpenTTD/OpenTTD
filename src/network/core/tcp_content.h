/* $Id$ */

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

#ifdef ENABLE_NETWORK

/** The values in the enum are important; they are used as database 'keys' */
enum ContentType {
	CONTENT_TYPE_BEGIN         = 1, ///< Helper to mark the begin of the types
	CONTENT_TYPE_BASE_GRAPHICS = 1, ///< The content consists of base graphics
	CONTENT_TYPE_NEWGRF        = 2, ///< The content consists of a NewGRF
	CONTENT_TYPE_AI            = 3, ///< The content consists of an AI
	CONTENT_TYPE_AI_LIBRARY    = 4, ///< The content consists of an AI library
	CONTENT_TYPE_SCENARIO      = 5, ///< The content consists of a scenario
	CONTENT_TYPE_HEIGHTMAP     = 6, ///< The content consists of a heightmap
	CONTENT_TYPE_BASE_SOUNDS   = 7, ///< The content consists of base sounds
	CONTENT_TYPE_BASE_MUSIC    = 8, ///< The content consists of base music
	CONTENT_TYPE_END,               ///< Helper to mark the end of the types
};

/** Enum with all types of TCP content packets. The order MUST not be changed **/
enum PacketContentType {
	PACKET_CONTENT_CLIENT_INFO_LIST,      ///< Queries the content server for a list of info of a given content type
	PACKET_CONTENT_CLIENT_INFO_ID,        ///< Queries the content server for information about a list of internal IDs
	PACKET_CONTENT_CLIENT_INFO_EXTID,     ///< Queries the content server for information about a list of external IDs
	PACKET_CONTENT_CLIENT_INFO_EXTID_MD5, ///< Queries the content server for information about a list of external IDs and MD5
	PACKET_CONTENT_SERVER_INFO,           ///< Reply of content server with information about content
	PACKET_CONTENT_CLIENT_CONTENT,        ///< Request a content file given an internal ID
	PACKET_CONTENT_SERVER_CONTENT,        ///< Reply with the content of the given ID
	PACKET_CONTENT_END                    ///< Must ALWAYS be on the end of this list!! (period)
};

#define DECLARE_CONTENT_RECEIVE_COMMAND(type) virtual bool NetworkPacketReceive_## type ##_command(Packet *p)
#define DEF_CONTENT_RECEIVE_COMMAND(cls, type) bool cls ##NetworkContentSocketHandler::NetworkPacketReceive_ ## type ## _command(Packet *p)

enum ContentID {
	INVALID_CONTENT_ID = UINT32_MAX
};

/** Container for all important information about a piece of content. */
struct ContentInfo {
	enum State {
		UNSELECTED,     ///< The content has not been selected
		SELECTED,       ///< The content has been manually selected
		AUTOSELECTED,   ///< The content has been selected as dependency
		ALREADY_HERE,   ///< The content is already at the client side
		DOES_NOT_EXIST, ///< The content does not exist in the content system
		INVALID         ///< The content's invalid
	};

	ContentType type;        ///< Type of content
	ContentID id;            ///< Unique (server side) ID for the content
	uint32 filesize;         ///< Size of the file
	char filename[48];       ///< Filename (for the .tar.gz; only valid on download)
	char name[32];           ///< Name of the content
	char version[16];        ///< Version of the content
	char url[96];            ///< URL related to the content
	char description[512];   ///< Description of the content
	uint32 unique_id;        ///< Unique ID; either GRF ID or shortname
	byte md5sum[16];         ///< The MD5 checksum
	uint8 dependency_count;  ///< Number of dependencies
	ContentID *dependencies; ///< Malloced array of dependencies (unique server side ids)
	uint8 tag_count;         ///< Number of tags
	char (*tags)[32];        ///< Malloced array of tags (strings)
	State state;             ///< Whether the content info is selected (for download)
	bool upgrade;            ///< This item is an upgrade

	ContentInfo();
	~ContentInfo();

	void TransferFrom(ContentInfo *other);

	size_t Size() const;
	bool IsSelected() const;
	bool IsValid() const;
};

/** Base socket handler for all Content TCP sockets */
class NetworkContentSocketHandler : public NetworkTCPSocketHandler {
protected:
	NetworkAddress client_addr; ///< The address we're connected to.
	virtual void Close();

	/**
	 * Client requesting a list of content info:
	 *  byte    type
	 *  uint32  openttd version
	 */
	DECLARE_CONTENT_RECEIVE_COMMAND(PACKET_CONTENT_CLIENT_INFO_LIST);

	/**
	 * Client requesting a list of content info:
	 *  uint16  count of ids
	 *  uint32  id (count times)
	 */
	DECLARE_CONTENT_RECEIVE_COMMAND(PACKET_CONTENT_CLIENT_INFO_ID);

	/**
	 * Client requesting a list of content info based on an external
	 * 'unique' id; GRF ID for NewGRFS, shortname and for base
	 * graphics and AIs.
	 * Scenarios and AI libraries are not supported
	 *  uint8   count of requests
	 *  for each request:
	 *    uint8 type
	 *    unique id (uint32)
	 */
	DECLARE_CONTENT_RECEIVE_COMMAND(PACKET_CONTENT_CLIENT_INFO_EXTID);

	/**
	 * Client requesting a list of content info based on an external
	 * 'unique' id; GRF ID + MD5 checksum for NewGRFS, shortname and
	 * xor-ed MD5 checsums for base graphics and AIs.
	 * Scenarios and AI libraries are not supported
	 *  uint8   count of requests
	 *  for each request:
	 *    uint8 type
	 *    unique id (uint32)
	 *    md5 (16 bytes)
	 */
	DECLARE_CONTENT_RECEIVE_COMMAND(PACKET_CONTENT_CLIENT_INFO_EXTID_MD5);

	/**
	 * Server sending list of content info:
	 *  byte    type (invalid ID == does not exist)
	 *  uint32  id
	 *  uint32  file_size
	 *  string  name (max 32 characters)
	 *  string  version (max 16 characters)
	 *  uint32  unique id
	 *  uint8   md5sum (16 bytes)
	 *  uint8   dependency count
	 *  uint32  unique id of dependency (dependency count times)
	 *  uint8   tag count
	 *  string  tag (max 32 characters for tag count times)
	 */
	DECLARE_CONTENT_RECEIVE_COMMAND(PACKET_CONTENT_SERVER_INFO);

	/**
	 * Client requesting the actual content:
	 *  uint16  count of unique ids
	 *  uint32  unique id (count times)
	 */
	DECLARE_CONTENT_RECEIVE_COMMAND(PACKET_CONTENT_CLIENT_CONTENT);

	/**
	 * Server sending list of content info:
	 *  uint32  unique id
	 *  uint32  file size (0 == does not exist)
	 *  string  file name (max 48 characters)
	 * After this initial packet, packets with the actual data are send using
	 * the same packet type.
	 */
	DECLARE_CONTENT_RECEIVE_COMMAND(PACKET_CONTENT_SERVER_CONTENT);

	bool HandlePacket(Packet *p);
public:
	/**
	 * Create a new cs socket handler for a given cs
	 * @param s  the socket we are connected with
	 * @param address IP etc. of the client
	 */
	NetworkContentSocketHandler(SOCKET s = INVALID_SOCKET, const NetworkAddress &address = NetworkAddress()) :
		NetworkTCPSocketHandler(s),
		client_addr(address)
	{
	}

	/** On destructing of this class, the socket needs to be closed */
	virtual ~NetworkContentSocketHandler() { this->Close(); }

	void ReceivePackets();
};

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_TCP_CONTENT_H */
