/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_tree.h NewGRF handling of tree tiles. */

#ifndef NEWGRF_TREE_H
#define NEWGRF_TREE_H

#include "gfx_type.h"
#include "newgrf_callbacks.h"
#include "tile_type.h"

uint16_t GetTreeTileCallback(CallbackID callback, uint32_t param1, uint32_t param2, uint16_t tree, TileIndex tile, std::span<int32_t> regs100 = {});
SpriteID GetCustomTreeSprite(TileIndex tile, uint16_t tree, uint8_t slot);

#endif /* NEWGRF_TREE_H */
