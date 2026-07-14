/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file void_cmd.cpp Handling of void tiles. */

#include "stdafx.h"
#include "landscape.h"
#include "command_func.h"
#include "viewport_func.h"
#include "slope_func.h"
#include "water.h"

#include "table/strings.h"
#include "table/sprites.h"

#include "safeguards.h"

/** @copydoc DrawTileProc */
static void DrawTile_Void(TileInfo *ti)
{
	/* If freeform edges are off, draw infinite water off the edges of the map. */
	if (!_settings_game.construction.freeform_edges) {
		DrawGroundSprite(SPR_FLAT_WATER_TILE + SlopeToSpriteOffset(ti->tileh), PAL_NONE);
	} else {
		DrawGroundSprite(SPR_FLAT_BARE_LAND + SlopeToSpriteOffset(ti->tileh), PALETTE_ALL_BLACK);
	}
}

/** @copydoc GetSlopePixelZProc */
static int GetSlopePixelZ_Void([[maybe_unused]] TileIndex tile, uint x, uint y, [[maybe_unused]] bool ground_vehicle)
{
	/* This function may be called on tiles outside the map, don't assume
	 * that 'tile' is a valid tile index. See GetSlopePixelZOutsideMap. */
	auto [tileh, z] = GetTilePixelSlopeOutsideMap(x >> 4, y >> 4);

	return z + GetPartialPixelZ(x & 0xF, y & 0xF, tileh);
}

/** @copydoc GetTileDescProc */
static void GetTileDesc_Void([[maybe_unused]] TileIndex tile, TileDesc &td)
{
	td.str = STR_EMPTY;
	td.owner[0] = OWNER_NONE;
}

/** @copydoc TileLoopProc */
static void TileLoop_Void(TileIndex tile)
{
	/* Floods adjacent edge tile to prevent maps without water. */
	TileLoop_Water(tile);
}

/** TileTypeProcs definitions for TileType::Void tiles. */
extern const TileTypeProcs _tile_type_void_procs = {
	.draw_tile_proc = DrawTile_Void,
	.get_slope_pixel_z_proc = GetSlopePixelZ_Void,
	.clear_tile_proc = [](TileIndex, DoCommandFlags) { return CommandCost(STR_ERROR_OFF_EDGE_OF_MAP); },
	.get_tile_desc_proc = GetTileDesc_Void,
	.tile_loop_proc = TileLoop_Void,
	.terraform_tile_proc = [](TileIndex, DoCommandFlags, int, Slope) { return CommandCost(STR_ERROR_OFF_EDGE_OF_MAP); },
};
