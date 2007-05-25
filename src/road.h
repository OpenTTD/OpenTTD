/* $Id$ */

/** @file road.h */

#ifndef ROAD_H
#define ROAD_H

#include "helpers.hpp"

/**
 * The different roadtypes we support
 * @note currently only ROADTYPE_ROAD is supported.
 */
enum RoadType {
	ROADTYPE_ROAD = 0,
	ROADTYPE_TRAM = 1,
	ROADTYPE_HWAY = 2, ///< Only a placeholder. Not sure what we are going to do with this road type.
	ROADTYPE_END,
	INVALID_ROADTYPE = 0xFF
};
DECLARE_POSTFIX_INCREMENT(RoadType);

/**
 * The different roadtypes we support, but then a bitmask of them
 * @note currently only ROADTYPES_ROAD is supported.
 */
enum RoadTypes {
	ROADTYPES_NONE     = 0,
	ROADTYPES_ROAD     = 1 << ROADTYPE_ROAD,
	ROADTYPES_TRAM     = 1 << ROADTYPE_TRAM,
	ROADTYPES_HWAY     = 1 << ROADTYPE_HWAY,
	ROADTYPES_ROADTRAM = ROADTYPES_ROAD | ROADTYPES_TRAM,
	ROADTYPES_ROADHWAY = ROADTYPES_ROAD | ROADTYPES_HWAY,
	ROADTYPES_TRAMHWAY = ROADTYPES_TRAM | ROADTYPES_HWAY,
	ROADTYPES_ALL      = ROADTYPES_ROAD | ROADTYPES_TRAM | ROADTYPES_HWAY,
};
DECLARE_ENUM_AS_BIT_SET(RoadTypes);

/**
 * Whether the given roadtype is valid.
 * @param rt the roadtype to check for validness
 * @return true if and only if valid
 */
static inline bool IsValidRoadType(RoadType rt)
{
	return rt == ROADTYPE_ROAD || rt == ROADTYPE_TRAM;
}

/**
 * Are the given bits pointing to valid roadtypes?
 * @param rts the roadtypes to check for validness
 * @return true if and only if valid
 */
static inline bool AreValidRoadTypes(RoadTypes rts)
{
	return HASBIT(rts, ROADTYPE_ROAD) || HASBIT(rts, ROADTYPE_TRAM);
}

/**
 * Maps a RoadType to the corresponding RoadTypes value
 */
static inline RoadTypes RoadTypeToRoadTypes(RoadType rt)
{
	return (RoadTypes)(1 << rt);
}

static inline RoadTypes ComplementRoadTypes(RoadTypes r)
{
	return (RoadTypes)(ROADTYPES_ALL ^ r);
}

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
 * @param rt        the road type to remove the bits from
 * @return true when it is allowed to remove the road bits
 */
bool CheckAllowRemoveRoad(TileIndex tile, RoadBits remove, Owner owner, bool *edge_road, RoadType rt);

void DrawTramCatenary(TileInfo *ti, RoadBits tram);

#endif /* ROAD_H */
