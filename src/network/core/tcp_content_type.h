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
#ifndef OPENTTD_MSU
	const char *GetTextfile(TextfileType type) const;
#endif /* OPENTTD_MSU */
};

#endif /* NETWORK_CORE_TCP_CONTENT_TYPE_H */
