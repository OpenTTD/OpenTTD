/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file town_type.h Types related to towns. */

#ifndef TOWN_TYPE_H
#define TOWN_TYPE_H

#include "core/enum_type.hpp"

typedef uint16 TownID;
struct Town;

/** Supported initial town sizes */
enum TownSize {
	TSZ_SMALL,  ///< Small town.
	TSZ_MEDIUM, ///< Medium town.
	TSZ_LARGE,  ///< Large town.
	TSZ_RANDOM, ///< Random size, bigger than small, smaller than large.

	TSZ_END,    ///< Number of available town sizes.
};
template <> struct EnumPropsT<TownSize> : MakeEnumPropsT<TownSize, byte, TSZ_SMALL, TSZ_END, TSZ_END, 2> {};

enum Ratings {
	/* These refer to the maximums, so Appalling is -1000 to -400
	 * MAXIMUM RATINGS BOUNDARIES */
	RATING_MINIMUM     = -1000,
	RATING_APPALLING   =  -400,
	RATING_VERYPOOR    =  -200,
	RATING_POOR        =     0,
	RATING_MEDIOCRE    =   200,
	RATING_GOOD        =   400,
	RATING_VERYGOOD    =   600,
	RATING_EXCELLENT   =   800,
	RATING_OUTSTANDING =  1000,         ///< OUTSTANDING

	RATING_MAXIMUM = RATING_OUTSTANDING,

	RATING_INITIAL = 500, ///< initial rating

	/* RATINGS AFFECTING NUMBERS */
	RATING_TREE_DOWN_STEP = -35,
	RATING_TREE_MINIMUM   = RATING_MINIMUM,
	RATING_TREE_UP_STEP   = 7,
	RATING_TREE_MAXIMUM   = 220,

	RATING_GROWTH_UP_STEP    =   5, ///< when a town grows, all companies have rating increased a bit ...
	RATING_GROWTH_MAXIMUM    = RATING_MEDIOCRE, ///< ... up to RATING_MEDIOCRE
	RATING_STATION_UP_STEP   =  12, ///< when a town grows, company gains reputation for all well serviced stations ...
	RATING_STATION_DOWN_STEP = -15, ///< ... but loses for bad serviced stations

	RATING_TUNNEL_BRIDGE_DOWN_STEP = -250, ///< penalty for removing town owned tunnel or bridge
	RATING_TUNNEL_BRIDGE_MINIMUM   =    0, ///< minimum rating after removing tunnel or bridge
	RATING_TUNNEL_BRIDGE_NEEDED_PERMISSIVE = 144, ///< rating needed, "Permissive" difficulty settings
	RATING_TUNNEL_BRIDGE_NEEDED_NEUTRAL    = 208, ///< "Neutral"
	RATING_TUNNEL_BRIDGE_NEEDED_HOSTILE    = 400, ///< "Hostile"

	RATING_ROAD_DOWN_STEP_INNER =  -50, ///< removing a roadpiece in the middle
	RATING_ROAD_DOWN_STEP_EDGE  =  -18, ///< removing a roadpiece at the edge
	RATING_ROAD_MINIMUM         = -100, ///< minimum rating after removing town owned road
	RATING_ROAD_NEEDED_PERMISSIVE =  16, ///< rating needed, "Permissive" difficulty settings
	RATING_ROAD_NEEDED_NEUTRAL    =  64, ///< "Neutral"
	RATING_ROAD_NEEDED_HOSTILE    = 112, ///< "Hostile"

	RATING_HOUSE_MINIMUM  = RATING_MINIMUM,

	RATING_BRIBE_UP_STEP = 200,
	RATING_BRIBE_MAXIMUM = 800,
	RATING_BRIBE_DOWN_TO = -50        // XXX SHOULD BE SOMETHING LOWER?
};

/**
 * Town Layouts
 */
enum TownLayout {
	TL_BEGIN = 0,
	TL_ORIGINAL = 0,     ///< Original algorithm (min. 1 distance between roads)
	TL_BETTER_ROADS,     ///< Extended original algorithm (min. 2 distance between roads)
	TL_2X2_GRID,         ///< Geometric 2x2 grid algorithm
	TL_3X3_GRID,         ///< Geometric 3x3 grid algorithm

	TL_RANDOM,           ///< Random town layout

	NUM_TLS,             ///< Number of town layouts
};
template <> struct EnumPropsT<TownLayout> : MakeEnumPropsT<TownLayout, byte, TL_BEGIN, NUM_TLS, NUM_TLS, 3> {};
/** It needs to be 8bits, because we save and load it as such */
typedef SimpleTinyEnumT<TownLayout, byte> TownLayoutByte; // typedefing-enumification of TownLayout

/** Town founding setting values */
enum TownFounding {
	TF_BEGIN = 0,     ///< Used for iterations and limit testing
	TF_FORBIDDEN = 0, ///< Forbidden
	TF_ALLOWED,       ///< Allowed
	TF_CUSTOM_LAYOUT, ///< Allowed, with custom town layout
	TF_END,           ///< Used for iterations and limit testing
};
/** It needs to be 8bits, because we save and load it as such */
typedef SimpleTinyEnumT<TownFounding, byte> TownFoundingByte;

static const uint MAX_LENGTH_TOWN_NAME_CHARS  =  32; ///< The maximum length of a town name in characters including '\0'
static const uint MAX_LENGTH_TOWN_NAME_PIXELS = 130; ///< The maximum length of a town name in pixels

#endif /* TOWN_TYPE_H */
