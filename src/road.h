/* $Id$ */

/** @file road.h */

#ifndef ROAD_H
#define ROAD_H

#include "helpers.hpp"

/**
 * The different roadtypes we support
 *
 * @note currently only ROADTYPE_ROAD and ROADTYPE_TRAM are supported.
 */
enum RoadType {
	ROADTYPE_ROAD = 0,      ///< Basic road type
	ROADTYPE_TRAM = 1,      ///< Trams
	ROADTYPE_HWAY = 2,      ///< Only a placeholder. Not sure what we are going to do with this road type.
	ROADTYPE_END,           ///< Used for iterations
	INVALID_ROADTYPE = 0xFF ///< flag for invalid roadtype
};
DECLARE_POSTFIX_INCREMENT(RoadType);

/**
 * The different roadtypes we support, but then a bitmask of them
 * @note currently only roadtypes with ROADTYPE_ROAD and ROADTYPE_TRAM are supported.
 */
enum RoadTypes {
	ROADTYPES_NONE     = 0,                                                 ///< No roadtypes
	ROADTYPES_ROAD     = 1 << ROADTYPE_ROAD,                                ///< Road
	ROADTYPES_TRAM     = 1 << ROADTYPE_TRAM,                                ///< Trams
	ROADTYPES_HWAY     = 1 << ROADTYPE_HWAY,                                ///< Highway (or whatever substitute)
	ROADTYPES_ROADTRAM = ROADTYPES_ROAD | ROADTYPES_TRAM,                   ///< Road + trams
	ROADTYPES_ROADHWAY = ROADTYPES_ROAD | ROADTYPES_HWAY,                   ///< Road + highway (or whatever substitute)
	ROADTYPES_TRAMHWAY = ROADTYPES_TRAM | ROADTYPES_HWAY,                   ///< Trams + highway (or whatever substitute)
	ROADTYPES_ALL      = ROADTYPES_ROAD | ROADTYPES_TRAM | ROADTYPES_HWAY,  ///< Road + trams + highway (or whatever substitute)
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
 *
 * @param rt the roadtype to get the roadtypes from
 * @return the roadtypes with the given roadtype
 */
static inline RoadTypes RoadTypeToRoadTypes(RoadType rt)
{
	return (RoadTypes)(1 << rt);
}

/**
 * Returns the RoadTypes which are not present in the given RoadTypes
 *
 * This function returns the complement of a given RoadTypes.
 *
 * @param r The given RoadTypes
 * @return The complement of the given RoadTypes
 * @note The unused value ROADTYPES_HWAY will be used, too.
 */
static inline RoadTypes ComplementRoadTypes(RoadTypes r)
{
	return (RoadTypes)(ROADTYPES_ALL ^ r);
}

/**
 * Enumeration for the road parts on a tile.
 *
 * This enumeration defines the possible road parts which
 * can be build on a tile.
 */
enum RoadBits {
	ROAD_NONE = 0U,                  ///< No road-part is build
	ROAD_NW   = 1U,                  ///< North-west part
	ROAD_SW   = 2U,                  ///< South-west part
	ROAD_SE   = 4U,                  ///< South-east part
	ROAD_NE   = 8U,                  ///< North-east part
	ROAD_X    = ROAD_SW | ROAD_NE,   ///< Full road along the x-axis (south-west + north-east)
	ROAD_Y    = ROAD_NW | ROAD_SE,   ///< Full road along the y-axis (north-west + south-east)
	ROAD_ALL  = ROAD_X  | ROAD_Y     ///< Full 4-way crossing
};

DECLARE_ENUM_AS_BIT_SET(RoadBits);

/**
 * Calculate the complement of a RoadBits value
 *
 * Simply flips all bits in the RoadBits value to get the complement
 * of the RoadBits.
 *
 * @param r The given RoadBits value
 * @return the complement
 */
static inline RoadBits ComplementRoadBits(RoadBits r)
{
	return (RoadBits)(ROAD_ALL ^ r);
}

/**
 * Create the road-part which belongs to the given DiagDirection
 *
 * This function returns a RoadBits value which belongs to
 * the given DiagDirection.
 *
 * @param d The DiagDirection
 * @return The result RoadBits which the selected road-part set
 */
static inline RoadBits DiagDirToRoadBits(DiagDirection d)
{
	return (RoadBits)(1U << (3 ^ d));
}

/**
 * Checks whether the trackdir means that we are reversing.
 * @param dir the trackdir to check
 * @return true if it is a reversing road trackdir
 */
static inline bool IsReversingRoadTrackdir(Trackdir dir)
{
	return (dir & 0x07) >= 6;
}

/**
 * Checks whether the given trackdir is a straight road
 * @param dir the trackdir to check
 * @return true if it is a straight road trackdir
 */
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

/**
 * Draw the catenary for tram road bits
 * @param ti   information about the tile (position, slope)
 * @param tram the roadbits to draw the catenary for
 */
void DrawTramCatenary(TileInfo *ti, RoadBits tram);

#endif /* ROAD_H */
