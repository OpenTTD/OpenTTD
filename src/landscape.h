/* $Id$ */

/** @file landscape.h */

#ifndef LANDSCAPE_H
#define LANDSCAPE_H

#include "core/geometry_type.hpp"

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

bool IsValidTile(TileIndex tile);

uint GetPartialZ(int x, int y, Slope corners);
uint GetSlopeZ(int x, int y);
void GetSlopeZOnEdge(Slope tileh, DiagDirection edge, int *z1, int *z2);
int GetSlopeZInCorner(Slope tileh, Corner corner);

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

void ConvertGroundTilesIntoWaterTiles();

TileIndex AdjustTileCoordRandomly(TileIndex a, byte rng);

#endif /* LANDSCAPE_H */
