/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file league_type.h Basic types related to league tables. */

#ifndef LEAGUE_TYPE_H
#define LEAGUE_TYPE_H

#include "core/pool_type.hpp"

/** Types of the possible link targets. */
enum class LinkType : uint8_t {
	None = 0, ///< No link
	Tile = 1, ///< Link a tile
	Industry = 2, ///< Link an industry
	Town = 3, ///< Link a town
	Company = 4, ///< Link a company
	StoryPage = 5, ///< Link a story page
};

typedef uint32_t LinkTargetID; ///< Contains either tile, industry ID, town ID, story page ID or company ID

struct Link {
	LinkType type = LinkType::None;
	LinkTargetID target = 0;

	Link() {}
	Link(LinkType type, LinkTargetID target) : type{type}, target{target} {}
};

using LeagueTableID = PoolID<uint8_t, struct LeagueTableIDTag, 255, 0xFF>; ///< ID of a league table
struct LeagueTable;

using LeagueTableElementID = PoolID<uint16_t, struct LeagueTableElementIDTag, 64000, 0xFFFF>; ///< ID of a league table
struct LeagueTableElement;

#endif /* LEAGUE_TYPE_H */
