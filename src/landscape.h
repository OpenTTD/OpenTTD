/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file landscape.h Functions related to OTTD's landscape. */

#ifndef LANDSCAPE_H
#define LANDSCAPE_H

#include "core/geometry_type.hpp"
#include "tile_cmd.h"

static const uint SNOW_LINE_MONTHS = 12; ///< Number of months in the snow line table.
static const uint SNOW_LINE_DAYS   = 32; ///< Number of days in each month in the snow line table.

/**
 * Structure describing the height of the snow line each day of the year
 * @ingroup SnowLineGroup
 */
struct SnowLine {
	byte table[SNOW_LINE_MONTHS][SNOW_LINE_DAYS]; ///< Height of the snow line each day of the year
	byte highest_value; ///< Highest snow line of the year
	byte lowest_value;  ///< Lowest snow line of the year
};

bool IsSnowLineSet();
void SetSnowLine(byte table[SNOW_LINE_MONTHS][SNOW_LINE_DAYS]);
byte GetSnowLine();
byte HighestSnowLine();
byte LowestSnowLine();
void ClearSnowLine();

int GetSlopeZInCorner(Slope tileh, Corner corner);
Slope GetFoundationSlope(TileIndex tile, int *z = NULL);

uint GetPartialPixelZ(int x, int y, Slope corners);
int GetSlopePixelZ(int x, int y);
int GetSlopePixelZOutsideMap(int x, int y);
void GetSlopePixelZOnEdge(Slope tileh, DiagDirection edge, int *z1, int *z2);

/**
 * Determine the Z height of a corner relative to TileZ.
 *
 * @pre The slope must not be a halftile slope.
 *
 * @param tileh The slope.
 * @param corner The corner.
 * @return Z position of corner relative to TileZ.
 */
static inline int GetSlopePixelZInCorner(Slope tileh, Corner corner)
{
	return GetSlopeZInCorner(tileh, corner) * TILE_HEIGHT;
}

/**
 * Get slope of a tile on top of a (possible) foundation
 * If a tile does not have a foundation, the function returns the same as GetTilePixelSlope.
 *
 * @param tile The tile of interest.
 * @param z returns the z of the foundation slope. (Can be NULL, if not needed)
 * @return The slope on top of the foundation.
 */
static inline Slope GetFoundationPixelSlope(TileIndex tile, int *z)
{
	assert(z != NULL);
	Slope s = GetFoundationSlope(tile, z);
	*z *= TILE_HEIGHT;
	return s;
}

/**
 * Map 3D world or tile coordinate to equivalent 2D coordinate as used in the viewports and smallmap.
 * @param x X world or tile coordinate (runs in SW direction in the 2D view).
 * @param y Y world or tile coordinate (runs in SE direction in the 2D view).
 * @param z Z world or tile coordinate (runs in N direction in the 2D view).
 * @return Equivalent coordinate in the 2D view.
 * @see RemapCoords2
 */
static inline Point RemapCoords(int x, int y, int z)
{
	Point pt;
	pt.x = (y - x) * 2 * ZOOM_LVL_BASE;
	pt.y = (y + x - z) * ZOOM_LVL_BASE;
	return pt;
}

/**
 * Map 3D world or tile coordinate to equivalent 2D coordinate as used in the viewports and smallmap.
 * Same as #RemapCoords, except the Z coordinate is read from the map.
 * @param x X world or tile coordinate (runs in SW direction in the 2D view).
 * @param y Y world or tile coordinate (runs in SE direction in the 2D view).
 * @return Equivalent coordinate in the 2D view.
 * @see RemapCoords
 */
static inline Point RemapCoords2(int x, int y)
{
	return RemapCoords(x, y, GetSlopePixelZ(x, y));
}

/**
 * Map 2D viewport or smallmap coordinate to 3D world or tile coordinate.
 * Function assumes <tt>z == 0</tt>. For other values of \p z, add \p z to \a y before the call.
 * @param x X coordinate of the 2D coordinate.
 * @param y Y coordinate of the 2D coordinate.
 * @return X and Y components of equivalent world or tile coordinate.
 * @note Inverse of #RemapCoords function. Smaller values may get rounded.
 * @see InverseRemapCoords2
 */
static inline Point InverseRemapCoords(int x, int y)
{
	Point pt = {(y * 2 - x) >> (2 + ZOOM_LVL_SHIFT), (y * 2 + x) >> (2 + ZOOM_LVL_SHIFT)};
	return pt;
}

Point InverseRemapCoords2(int x, int y, bool clamp_to_map = false, bool *clamped = NULL);

uint ApplyFoundationToSlope(Foundation f, Slope *s);
/**
 * Applies a foundation to a slope.
 *
 * @pre      Foundation and slope must be valid combined.
 * @param f  The #Foundation.
 * @param s  The #Slope to modify.
 * @return   Increment to the tile Z coordinate.
 */
static inline uint ApplyPixelFoundationToSlope(Foundation f, Slope *s)
{
	return ApplyFoundationToSlope(f, s) * TILE_HEIGHT;
}

void DrawFoundation(TileInfo *ti, Foundation f);
bool HasFoundationNW(TileIndex tile, Slope slope_here, uint z_here);
bool HasFoundationNE(TileIndex tile, Slope slope_here, uint z_here);

void DoClearSquare(TileIndex tile);
void RunTileLoop();

void InitializeLandscape();
void GenerateLandscape(byte mode);

#endif /* LANDSCAPE_H */
