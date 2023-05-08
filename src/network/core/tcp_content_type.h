/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_content_type.h Basic types related to the content on the content server.
 */

#ifndef NETWORK_CORE_TCP_CONTENT_TYPE_H
#define NETWORK_CORE_TCP_CONTENT_TYPE_H

#include "../../3rdparty/md5/md5.h"

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
	CONTENT_TYPE_GAME          = 9, ///< The content consists of a game script
	CONTENT_TYPE_GAME_LIBRARY  = 10, ///< The content consists of a GS library
	CONTENT_TYPE_END,               ///< Helper to mark the end of the types
	INVALID_CONTENT_TYPE       = 0xFF, ///< Invalid/uninitialized content
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
	PACKET_CONTENT_END,                   ///< Must ALWAYS be on the end of this list!! (period)
};

/** Unique identifier for the content. */
enum ContentID {
	INVALID_CONTENT_ID = UINT32_MAX, ///< Sentinel for invalid content.
};

/** Container for all important information about a piece of content. */
struct ContentInfo {
	/** The state the content can be in. */
	enum State {
		UNSELECTED,     ///< The content has not been selected
		SELECTED,       ///< The content has been manually selected
		AUTOSELECTED,   ///< The content has been selected as dependency
		ALREADY_HERE,   ///< The content is already at the client side
		DOES_NOT_EXIST, ///< The content does not exist in the content system
		INVALID,        ///< The content's invalid
	};

	ContentType type = INVALID_CONTENT_TYPE; ///< Type of content
	ContentID id = INVALID_CONTENT_ID;       ///< Unique (server side) ID for the content
	uint32_t filesize = 0;                     ///< Size of the file
	std::string filename;                    ///< Filename (for the .tar.gz; only valid on download)
	std::string name;                        ///< Name of the content
	std::string version;                     ///< Version of the content
	std::string url;                         ///< URL related to the content
	std::string description;                 ///< Description of the content
	uint32_t unique_id = 0;                    ///< Unique ID; either GRF ID or shortname
	MD5Hash md5sum;                          ///< The MD5 checksum
	std::vector<ContentID> dependencies;     ///< The dependencies (unique server side ids)
	StringList tags;                         ///< Tags associated with the content
	State state = State::UNSELECTED;         ///< Whether the content info is selected (for download)
	bool upgrade = false;                    ///< This item is an upgrade

	bool IsSelected() const;
	bool IsValid() const;
	std::optional<std::string> GetTextfile(TextfileType type) const;
};

#endif /* NETWORK_CORE_TCP_CONTENT_TYPE_H */
