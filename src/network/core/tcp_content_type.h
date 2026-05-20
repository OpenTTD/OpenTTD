/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tcp_content_type.h Basic types related to the content on the content server. */

#ifndef NETWORK_CORE_TCP_CONTENT_TYPE_H
#define NETWORK_CORE_TCP_CONTENT_TYPE_H

#include "../../3rdparty/md5/md5.h"

#include "../../string_type.h"
#include "../../textfile_type.h"

/** The values in the enum are important; they are received over the network from the content servers. */
enum class ContentType : uint8_t {
	Begin = 1, ///< Helper to mark the begin of the types

	BaseGraphics = 1, ///< The content consists of base graphics
	NewGRF = 2, ///< The content consists of a NewGRF
	Ai = 3, ///< The content consists of an AI
	AiLibrary = 4, ///< The content consists of an AI library
	Scenario = 5, ///< The content consists of a scenario
	Heightmap = 6, ///< The content consists of a heightmap
	BaseSounds = 7, ///< The content consists of base sounds
	BaseMusic = 8, ///< The content consists of base music
	Gs = 9, ///< The content consists of a game script
	GsLibrary = 10, ///< The content consists of a GS library

	End, ///< Helper to mark the end of the types

	Invalid = 0xFF, ///< Invalid/uninitialized content
};
using ContentTypes = EnumBitSet<ContentType, uint16_t, ContentType::End>; ///< Bitset of chosen content types.

DECLARE_INCREMENT_DECREMENT_OPERATORS(ContentType)

/** Unique identifier for the content. */
using ContentID = uint32_t;

static constexpr ContentID INVALID_CONTENT_ID = UINT32_MAX; ///< Sentinel for invalid content.

/** Container for all important information about a piece of content. */
struct ContentInfo {
	/** The state the content can be in. */
	enum class State : uint8_t {
		Unselected, ///< The content has not been selected
		Selected, ///< The content has been manually selected
		Autoselected, ///< The content has been selected as dependency
		AlreadyHere, ///< The content is already at the client side
		DoesNotExist, ///< The content does not exist in the content system
		Invalid, ///< The content's invalid
	};

	ContentType type = ContentType::Invalid; ///< Type of content
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
	State state = State::Unselected;         ///< Whether the content info is selected (for download)
	bool upgrade = false;                    ///< This item is an upgrade

	bool IsSelected() const;
	bool IsValid() const;
	std::optional<std::string> GetTextfile(TextfileType type) const;
};

#endif /* NETWORK_CORE_TCP_CONTENT_TYPE_H */
