/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_trees.h NewGRF handling of trees and tree tiles. */

#ifndef NEWGRF_TREES_H
#define NEWGRF_TREES_H

#include "gfx_type.h"
#include "newgrf_callbacks.h"
#include "tile_cmd.h"
#include "tree_map.h"

struct TreeListEnt : PalSpriteID, Coord2D<int8_t> {};

uint16_t GetTreeTileCallback(CallbackID callback, uint32_t param1, uint32_t param2, TreeType treetype, TileIndex tile, std::span<int32_t> regs100 = {});
PalSpriteID GetCustomTreeSprite(TileIndex tile, TreeType treetype);
bool GetNewTreeList(const TileInfo *ti, TreeType treetype, uint trees, std::array<TreeListEnt, 4> &te);
// bool DrawNewTrees(const TileInfo *ti, TreeType treetype);

#endif /* NEWGRF_TREES_H */
