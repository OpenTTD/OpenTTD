/* $Id$ */

/** @file town_type.h Types related to towns. */

#ifndef TOWN_TYPE_H
#define TOWN_TYPE_H

#include "core/enum_type.hpp"

typedef uint16 TownID;
typedef uint16 HouseID;
typedef uint16 HouseClassID;

struct Town;
struct HouseSpec;

enum TownSizeMode {
	TSM_RANDOM,
	TSM_FIXED,
	TSM_CITY
};

enum {
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

	RATING_GROWTH_UP_STEP    =   5, ///< when a town grows, all players have rating increased a bit ...
	RATING_GROWTH_MAXIMUM    = RATING_MEDIOCRE, ///< ... up to RATING_MEDIOCRE
	RATING_STATION_UP_STEP   =  12, ///< when a town grows, player gains reputation for all well serviced stations ...
	RATING_STATION_DOWN_STEP = -15, ///< ... but loses for bad serviced stations

	RATING_TUNNEL_BRIDGE_DOWN_STEP = -250,
	RATING_TUNNEL_BRIDGE_MINIMUM   = 0,

	RATING_ROAD_DOWN_STEP_INNER = -50, ///< removing a roadpiece in the middle
	RATING_ROAD_DOWN_STEP_EDGE  = -18, ///< removing a roadpiece at the edge
	RATING_ROAD_MINIMUM   = -100,
	RATING_HOUSE_MINIMUM  = RATING_MINIMUM,

	RATING_BRIBE_UP_STEP = 200,
	RATING_BRIBE_MAXIMUM = 800,
	RATING_BRIBE_DOWN_TO = -50        // XXX SHOULD BE SOMETHING LOWER?
};

/**
 * Town Layouts
 */
enum TownLayout {
	TL_NO_ROADS     = 0, ///< Build no more roads, but still build houses
	TL_ORIGINAL,         ///< Original algorithm (min. 1 distance between roads)
	TL_BETTER_ROADS,     ///< Extended original algorithm (min. 2 distance between roads)
	TL_2X2_GRID,         ///< Geometric 2x2 grid algorithm
	TL_3X3_GRID,         ///< Geometric 3x3 grid algorithm

	TL_RANDOM,           ///< Random town layout

	NUM_TLS,             ///< Number of town layouts
};

/* It needs to be 8bits, because we save and load it as such */
/** Define basic enum properties */
template <> struct EnumPropsT<TownLayout> : MakeEnumPropsT<TownLayout, byte, TL_NO_ROADS, NUM_TLS, NUM_TLS> {};
typedef TinyEnumT<TownLayout> TownLayoutByte; //typedefing-enumification of TownLayout

#endif /* TOWN_TYPE_H */
