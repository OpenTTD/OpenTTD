/* $Id$ */

/** @file track_land.h Sprites to use and how to display them for train depot tiles. */

#define TILE_SEQ_LINE(img, dx, dy, sx, sy) { dx, dy, 0, sx, sy, 23, {img, PAL_NONE} },
#define TILE_SEQ_END() { (int8)0x80, 0, 0, 0, 0, 0, {0, 0} }


static const DrawTileSeqStruct _depot_gfx_NE[] = {
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_NE | (1 << PALETTE_MODIFIER_COLOUR), 2, 13, 13, 1)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _depot_gfx_SE[] = {
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_SE_1 | (1 << PALETTE_MODIFIER_COLOUR),  2, 2, 1, 13)
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_SE_2 | (1 << PALETTE_MODIFIER_COLOUR), 13, 2, 1, 13)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _depot_gfx_SW[] = {
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_SW_1 | (1 << PALETTE_MODIFIER_COLOUR), 2,  2, 13, 1)
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_SW_2 | (1 << PALETTE_MODIFIER_COLOUR), 2, 13, 13, 1)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _depot_gfx_NW[] = {
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_NW | (1 << PALETTE_MODIFIER_COLOUR), 13, 2, 1, 13)
	TILE_SEQ_END()
};

static const DrawTileSprites _depot_gfx_table[] = {
	{ {SPR_FLAT_GRASS_TILE, PAL_NONE}, _depot_gfx_NE },
	{ {SPR_RAIL_TRACK_Y,    PAL_NONE}, _depot_gfx_SE },
	{ {SPR_RAIL_TRACK_X,    PAL_NONE}, _depot_gfx_SW },
	{ {SPR_FLAT_GRASS_TILE, PAL_NONE}, _depot_gfx_NW }
};

static const DrawTileSprites _depot_invisible_gfx_table[] = {
	{ {SPR_RAIL_TRACK_X, PAL_NONE}, _depot_gfx_NE },
	{ {SPR_RAIL_TRACK_Y, PAL_NONE}, _depot_gfx_SE },
	{ {SPR_RAIL_TRACK_X, PAL_NONE}, _depot_gfx_SW },
	{ {SPR_RAIL_TRACK_Y, PAL_NONE}, _depot_gfx_NW }
};

#undef TILE_SEQ_LINE
#undef TILE_SEQ_END

