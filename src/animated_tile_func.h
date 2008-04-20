/* $Id$ */

/** @file animated_tile.h Tile animation! */

#ifndef ANIMATED_TILE_H
#define ANIMATED_TILE_H

#include "tile_type.h"

void AddAnimatedTile(TileIndex tile);
void DeleteAnimatedTile(TileIndex tile);
void AnimateAnimatedTiles();
void InitializeAnimatedTiles();

#endif /* ANIMATED_TILE_H */
