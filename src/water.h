/* $Id$ */

/** @file water.h Functions related to water (management) */

#ifndef WATER_H
#define WATER_H

void TileLoop_Water(TileIndex tile);
bool FloodHalftile(TileIndex t);

void ConvertGroundTilesIntoWaterTiles();

void DrawShipDepotSprite(int x, int y, int image);
void DrawCanalWater(TileIndex tile);
void DrawShoreTile(Slope tileh);

void MakeWaterOrCanalDependingOnOwner(TileIndex tile, Owner o);
void MakeWaterOrCanalDependingOnSurroundings(TileIndex t, Owner o);

#endif /* WATER_H */
