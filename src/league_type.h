/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file league_type.h basic types related to league tables */

#ifndef LEAGUE_TYPE_H
#define LEAGUE_TYPE_H

/** Types of the possible link targets. */
enum LinkType : byte {
	LT_NONE = 0,         ///< No link
	LT_TILE = 1,         ///< Link a tile
	LT_INDUSTRY = 2,     ///< Link an industry
	LT_TOWN = 3,         ///< Link a town
	LT_COMPANY = 4,      ///< Link a company
	LT_STORY_PAGE = 5,   ///< Link a story page
};

typedef uint32_t LinkTargetID; ///< Contains either tile, industry ID, town ID, story page ID or company ID

struct Link {
	LinkType type;
	LinkTargetID target;
	Link(LinkType type, LinkTargetID target): type{type}, target{target} {}
	Link(): Link(LT_NONE, 0) {}
};

typedef uint8_t LeagueTableID; ///< ID of a league table
struct LeagueTable;
static const LeagueTableID INVALID_LEAGUE_TABLE = 0xFF; ///< Invalid/unknown index of LeagueTable

typedef uint16_t LeagueTableElementID; ///< ID of a league table element
struct LeagueTableElement;
static const LeagueTableElementID INVALID_LEAGUE_TABLE_ELEMENT = 0xFFFF; ///< Invalid/unknown index of LeagueTableElement

#endif /* LEAGUE_TYPE_H */
