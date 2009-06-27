/* $Id$ */

/** @file dummy_land.cpp Handling of void (or dummy) tiles. */

#include "stdafx.h"
#include "tile_cmd.h"
#include "command_func.h"
#include "viewport_func.h"
#include "tile_map.h"

#include "table/strings.h"
#include "table/sprites.h"

static void DrawTile_Dummy(TileInfo *ti)
{
	DrawGroundSpriteAt(SPR_SHADOW_CELL, PAL_NONE, ti->x, ti->y, ti->z);
}


static uint GetSlopeZ_Dummy(TileIndex tile, uint x, uint y)
{
	return TilePixelHeight(tile);
}

static Foundation GetFoundation_Dummy(TileIndex tile, Slope tileh)
{
	return FOUNDATION_NONE;
}

static CommandCost ClearTile_Dummy(TileIndex tile, DoCommandFlag flags)
{
	return_cmd_error(STR_ERROR_OFF_EDGE_OF_MAP);
}


static void GetTileDesc_Dummy(TileIndex tile, TileDesc *td)
{
	td->str = STR_EMPTY;
	td->owner[0] = OWNER_NONE;
}

static void TileLoop_Dummy(TileIndex tile)
{
	/* not used */
}

static void ChangeTileOwner_Dummy(TileIndex tile, Owner old_owner, Owner new_owner)
{
	/* not used */
}

static TrackStatus GetTileTrackStatus_Dummy(TileIndex tile, TransportType mode, uint sub_mode, DiagDirection side)
{
	return 0;
}

static CommandCost TerraformTile_Dummy(TileIndex tile, DoCommandFlag flags, uint z_new, Slope tileh_new)
{
	return_cmd_error(STR_ERROR_OFF_EDGE_OF_MAP);
}

extern const TileTypeProcs _tile_type_dummy_procs = {
	DrawTile_Dummy,           // draw_tile_proc
	GetSlopeZ_Dummy,          // get_slope_z_proc
	ClearTile_Dummy,          // clear_tile_proc
	NULL,                     // add_accepted_cargo_proc
	GetTileDesc_Dummy,        // get_tile_desc_proc
	GetTileTrackStatus_Dummy, // get_tile_track_status_proc
	NULL,                     // click_tile_proc
	NULL,                     // animate_tile_proc
	TileLoop_Dummy,           // tile_loop_clear
	ChangeTileOwner_Dummy,    // change_tile_owner_clear
	NULL,                     // add_produced_cargo_proc
	NULL,                     // vehicle_enter_tile_proc
	GetFoundation_Dummy,      // get_foundation_proc
	TerraformTile_Dummy,      // terraform_tile_proc
};
