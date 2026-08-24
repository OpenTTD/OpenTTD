/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tree_func.h Function definitions related to tree tiles. */

#ifndef TREE_FUNC_H
#define TREE_FUNC_H

#include "gfx_type.h"
#include "tree_map.h"
#include "tree_type.h"

void ResetTrees();
void FinaliseTrees();

std::span<const TreeSpec> GetOriginalTreeSpecs();
const TreeSpec &GetTreeSpec(uint16_t tree);

std::span<const TreeType> GetTreeTypes();
PalSpriteID GetTreeSprite(TreeType treetype);

void PlaceTree(TileIndex tile, uint32_t r, bool keep_density = false);
void PlaceTreesRandomly();
uint PlaceTreeGroupAroundTile(TileIndex tile, TreeType treetype, uint radius, uint count, bool set_zone);

#endif /* TREE_FUNC_H */
