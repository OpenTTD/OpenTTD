/* $Id$ */

/** @file water.h Functions related to water (management) */

#ifndef WATER_H
#define WATER_H

void TileLoop_Water(TileIndex tile);
bool FloodHalftile(TileIndex t);

void ConvertGroundTilesIntoWaterTiles();

void DrawShipDepotSprite(int x, int y, int image);
void DrawWaterClassGround(const struct TileInfo *ti);
void DrawShoreTile(Slope tileh);

void MakeWaterKeepingClass(TileIndex tile, Owner o);
void SetWaterClassDependingOnSurroundings(TileIndex t);

#endif /* WATER_H */
