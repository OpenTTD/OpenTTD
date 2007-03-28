/* $Id$ */

/** @file road.h */

#ifndef ROAD_H
#define ROAD_H

#include "helpers.hpp"

enum RoadBits {
	ROAD_NONE = 0U,
	ROAD_NW  = 1U,
	ROAD_SW  = 2U,
	ROAD_SE  = 4U,
	ROAD_NE  = 8U,
	ROAD_X   = ROAD_SW | ROAD_NE,
	ROAD_Y   = ROAD_NW | ROAD_SE,
	ROAD_ALL = ROAD_X  | ROAD_Y
};

DECLARE_ENUM_AS_BIT_SET(RoadBits);

static inline RoadBits ComplementRoadBits(RoadBits r)
{
	return (RoadBits)(ROAD_ALL ^ r);
}

static inline RoadBits DiagDirToRoadBits(DiagDirection d)
{
	return (RoadBits)(1U << (3 ^ d));
}

/** Checks whether the trackdir means that we are reversing */
static inline bool IsReversingRoadTrackdir(Trackdir dir)
{
	return (dir & 0x07) >= 6;
}

/** Checks whether the given trackdir is a straight road */
static inline bool IsStraightRoadTrackdir(Trackdir dir)
{
	return (dir & 0x06) == 0;
}

/**
 * Is it allowed to remove the given road bits from the given tile?
 * @param tile      the tile to remove the road from
 * @param remove    the roadbits that are going to be removed
 * @param owner     the actual owner of the roadbits of the tile
 * @param edge_road are the removed bits from a town?
 * @return true when it is allowed to remove the road bits
 */
bool CheckAllowRemoveRoad(TileIndex tile, RoadBits remove, Owner owner, bool *edge_road);

#endif /* ROAD_H */
