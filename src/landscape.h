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
#include "slope_type.h"
#include "direction_type.h"

enum {
	SNOW_LINE_MONTHS = 12, ///< Number of months in the snow line table.
	SNOW_LINE_DAYS   = 32, ///< Number of days in each month in the snow line table.
};

/** Structure describing the height of the snow line each day of the year
 * @ingroup SnowLineGroup */
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

uint GetPartialZ(int x, int y, Slope corners);
uint GetSlopeZ(int x, int y);
void GetSlopeZOnEdge(Slope tileh, DiagDirection edge, int *z1, int *z2);
int GetSlopeZInCorner(Slope tileh, Corner corner);
Slope GetFoundationSlope(TileIndex tile, uint *z);

static inline Point RemapCoords(int x, int y, int z)
{
	Point pt;
	pt.x = (y - x) * 2;
	pt.y = y + x - z;
	return pt;
}

static inline Point RemapCoords2(int x, int y)
{
	return RemapCoords(x, y, GetSlopeZ(x, y));
}

uint ApplyFoundationToSlope(Foundation f, Slope *s);
void DrawFoundation(TileInfo *ti, Foundation f);

void DoClearSquare(TileIndex tile);
void RunTileLoop();

void InitializeLandscape();
void GenerateLandscape(byte mode);

#endif /* LANDSCAPE_H */
