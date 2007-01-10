/* $Id$ */

#define TILE_SEQ_LINE(img, dx, dy, sx, sy) { dx, dy, 0, sx, sy, 23, img },
#define TILE_SEQ_END() { (byte)0x80, 0, 0, 0, 0, 0, 0 }


static const DrawTileSeqStruct _depot_gfx_NE[] = {
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_NE | PALETTE_MODIFIER_COLOR, 2, 13, 13, 1)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _depot_gfx_SE[] = {
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_SE_1 | PALETTE_MODIFIER_COLOR,  2, 2, 1, 13)
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_SE_2 | PALETTE_MODIFIER_COLOR, 13, 2, 1, 13)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _depot_gfx_SW[] = {
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_SW_1 | PALETTE_MODIFIER_COLOR, 2,  2, 13, 1)
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_SW_2 | PALETTE_MODIFIER_COLOR, 2, 13, 13, 1)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _depot_gfx_NW[] = {
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_NW | PALETTE_MODIFIER_COLOR, 13, 2, 1, 13)
	TILE_SEQ_END()
};

static const DrawTileSprites _depot_gfx_table[] = {
	{ SPR_FLAT_GRASS_TILE, _depot_gfx_NE },
	{ SPR_RAIL_TRACK_Y,    _depot_gfx_SE },
	{ SPR_RAIL_TRACK_X,    _depot_gfx_SW },
	{ SPR_FLAT_GRASS_TILE, _depot_gfx_NW }
};


static const DrawTileSeqStruct _waypoint_gfx_X[] = {
	TILE_SEQ_LINE(PALETTE_MODIFIER_COLOR | SPR_WAYPOINT_X_1,  0,  0,  16,  5)
	TILE_SEQ_LINE(PALETTE_MODIFIER_COLOR | SPR_WAYPOINT_X_2,  0, 11,  16,  5)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _waypoint_gfx_Y[] = {
	TILE_SEQ_LINE(PALETTE_MODIFIER_COLOR | SPR_WAYPOINT_Y_1,   0,  0, 5, 16)
	TILE_SEQ_LINE(PALETTE_MODIFIER_COLOR | SPR_WAYPOINT_Y_2,  11,  0, 5, 16)
	TILE_SEQ_END()
};

static const DrawTileSprites _waypoint_gfx_table[] = {
	{ SPR_RAIL_TRACK_X, _waypoint_gfx_X },
	{ SPR_RAIL_TRACK_Y, _waypoint_gfx_Y }
};
