#include "stdafx.h"
#include "ttd.h"
#include "viewport.h"
#include "command.h"

static void DrawTile_Dummy(TileInfo *ti)
{
	DrawGroundSpriteAt(0x3EC, ti->x, ti->y, ti->z);
}


static uint GetSlopeZ_Dummy(TileInfo *ti) {
	return GetPartialZ(ti->x&0xF, ti->y&0xF, ti->tileh) + ti->z;
}

static uint GetSlopeTileh_Dummy(TileInfo *ti) {
	return ti->tileh;
}

static int32 ClearTile_Dummy(uint tile, byte flags) {
	return_cmd_error(STR_0001_OFF_EDGE_OF_MAP);
}


static void GetAcceptedCargo_Dummy(uint tile, AcceptedCargo *ac)
{
	/* not used */
}

static void GetTileDesc_Dummy(uint tile, TileDesc *td)
{
	td->str = STR_EMPTY;
	td->owner = OWNER_NONE;
}

static void AnimateTile_Dummy(uint tile)
{
	/* not used */
}

static void TileLoop_Dummy(uint tile)
{
	/* not used */
}

static void ClickTile_Dummy(uint tile)
{
	/* not used */
}

static void ChangeTileOwner_Dummy(uint tile, byte old_player, byte new_player)
{
	/* not used */
}

static uint32 GetTileTrackStatus_Dummy(uint tile, int mode)
{
	return 0;
}

const TileTypeProcs _tile_type_dummy_procs = {
	DrawTile_Dummy,						/* draw_tile_proc */
	GetSlopeZ_Dummy,					/* get_slope_z_proc */
	ClearTile_Dummy,					/* clear_tile_proc */
	GetAcceptedCargo_Dummy,		/* get_accepted_cargo_proc */
	GetTileDesc_Dummy,				/* get_tile_desc_proc */
	GetTileTrackStatus_Dummy,	/* get_tile_track_status_proc */
	ClickTile_Dummy,					/* click_tile_proc */
	AnimateTile_Dummy,				/* animate_tile_proc */
	TileLoop_Dummy,						/* tile_loop_clear */
	ChangeTileOwner_Dummy,		/* change_tile_owner_clear */
	NULL,											/* get_produced_cargo_proc */
	NULL,											/* vehicle_enter_tile_proc */
	NULL,											/* vehicle_leave_tile_proc */
	GetSlopeTileh_Dummy,			/* get_slope_tileh_proc */
};

