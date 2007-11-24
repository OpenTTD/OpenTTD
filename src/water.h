/* $Id$ */

/** @file water.h Functions related to water (management) */

#ifndef WATER_H
#define WATER_H

void TileLoop_Water(TileIndex tile);
void DrawShipDepotSprite(int x, int y, int image);
void DrawCanalWater(TileIndex tile);
void MakeWaterOrCanalDependingOnSurroundings(TileIndex t, Owner o);

#endif /* WATER_H */
