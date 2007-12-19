/* $Id$ */

/** @file water.h Functions related to water (management) */

#ifndef WATER_H
#define WATER_H

void TileLoop_Water(TileIndex tile);
void DrawShipDepotSprite(int x, int y, int image);
void DrawCanalWater(TileIndex tile);
void MakeWaterOrCanalDependingOnOwner(TileIndex tile, Owner o);
void MakeWaterOrCanalDependingOnSurroundings(TileIndex t, Owner o);
void FloodHalftile(TileIndex t);

#endif /* WATER_H */
