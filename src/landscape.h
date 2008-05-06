/* $Id$ */

/** @file landscape.h Functions related to OTTD's landscape. */

#ifndef LANDSCAPE_H
#define LANDSCAPE_H

#include "core/geometry_type.hpp"
#include "tile_cmd.h"
#include "slope_type.h"
#include "direction_type.h"

enum {
	SNOW_LINE_MONTHS = 12,
	SNOW_LINE_DAYS   = 32,
};

struct SnowLine {
	byte table[SNOW_LINE_MONTHS][SNOW_LINE_DAYS];
	byte highest_value;
};

bool IsSnowLineSet(void);
void SetSnowLine(byte table[SNOW_LINE_MONTHS][SNOW_LINE_DAYS]);
byte GetSnowLine(void);
byte HighestSnowLine(void);
void ClearSnowLine(void);

uint GetPartialZ(int x, int y, Slope corners);
uint GetSlopeZ(int x, int y);
void GetSlopeZOnEdge(Slope tileh, DiagDirection edge, int *z1, int *z2);
int GetSlopeZInCorner(Slope tileh, Corner corner);
Slope GetFoundationSlope(TileIndex tile, uint* z);

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

TileIndex AdjustTileCoordRandomly(TileIndex a, byte rng);

#endif /* LANDSCAPE_H */
