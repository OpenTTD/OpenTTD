/* $Id$ */

/** @file unmovable_land.h Sprites to use and how to display them for unmovable tiles. */

static const DrawTileSeqStruct _draw_tile_transmitterlighthouse_data[] = {
	{   7,  7,  0,  2,  2, 70, {SPR_UNMOVABLE_TRANSMITTER, PAL_NONE}},
	{   4,  4,  0,  7,  7, 61, {SPR_UNMOVABLE_LIGHTHOUSE, PAL_NONE}},
};

#define TILE_SEQ_LINE(sz, img) { 0, 0, 0, 16, 16, sz, {img, PAL_NONE} },
#define TILE_SEQ_END() { (byte)0x80, 0, 0, 0, 0, 0, {0, 0} }

static const DrawTileSeqStruct _unmovable_display_nothing[] = {
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_8[] = {
	TILE_SEQ_LINE(20, SPR_MEDIUMHQ_NORTH_WALL | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_9[] = {
	TILE_SEQ_LINE(20, SPR_MEDIUMHQ_EAST_WALL  | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_10[] = {
	TILE_SEQ_LINE(20, SPR_MEDIUMHQ_WEST_WALL  | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_12[] = {
	TILE_SEQ_LINE(50, SPR_LARGEHQ_NORTH_BUILD | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_13[] = {
	TILE_SEQ_LINE(50, SPR_LARGEHQ_EAST_BUILD  | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_14[] = {
	TILE_SEQ_LINE(50, SPR_LARGEHQ_WEST_BUILD  | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_16[] = {
	TILE_SEQ_LINE(60, SPR_HUGEHQ_NORTH_BUILD  | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_17[] = {
	TILE_SEQ_LINE(60, SPR_HUGEHQ_EAST_BUILD   | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_18[] = {
	TILE_SEQ_LINE(60, SPR_HUGEHQ_WEST_BUILD   | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

#undef TILE_SEQ_LINE
#undef TILE_SEQ_END

#define TILE_SPRITE_LINE(img, dtss) { {img | (1 << PALETTE_MODIFIER_COLOUR), PAL_NONE}, dtss },

static const DrawTileSprites _unmovable_display_datas[] = {
	TILE_SPRITE_LINE(SPR_TINYHQ_NORTH,         _unmovable_display_nothing)
	TILE_SPRITE_LINE(SPR_TINYHQ_EAST,          _unmovable_display_nothing)
	TILE_SPRITE_LINE(SPR_TINYHQ_WEST,          _unmovable_display_nothing)
	TILE_SPRITE_LINE(SPR_TINYHQ_SOUTH,         _unmovable_display_nothing)

	TILE_SPRITE_LINE(SPR_SMALLHQ_NORTH,        _unmovable_display_nothing)
	TILE_SPRITE_LINE(SPR_SMALLHQ_EAST,         _unmovable_display_nothing)
	TILE_SPRITE_LINE(SPR_SMALLHQ_WEST,         _unmovable_display_nothing)
	TILE_SPRITE_LINE(SPR_SMALLHQ_SOUTH,        _unmovable_display_nothing)

	TILE_SPRITE_LINE(SPR_MEDIUMHQ_NORTH,       _unmovable_display_datas_8)
	TILE_SPRITE_LINE(SPR_MEDIUMHQ_EAST,        _unmovable_display_datas_9)
	TILE_SPRITE_LINE(SPR_MEDIUMHQ_WEST,        _unmovable_display_datas_10)
	TILE_SPRITE_LINE(SPR_MEDIUMHQ_SOUTH,       _unmovable_display_nothing)

	TILE_SPRITE_LINE(SPR_LARGEHQ_NORTH_GROUND, _unmovable_display_datas_12)
	TILE_SPRITE_LINE(SPR_LARGEHQ_EAST_GROUND,  _unmovable_display_datas_13)
	TILE_SPRITE_LINE(SPR_LARGEHQ_WEST_GROUND,  _unmovable_display_datas_14)
	TILE_SPRITE_LINE(SPR_LARGEHQ_SOUTH,        _unmovable_display_nothing)

	TILE_SPRITE_LINE(SPR_HUGEHQ_NORTH_GROUND,  _unmovable_display_datas_16)
	TILE_SPRITE_LINE(SPR_HUGEHQ_EAST_GROUND,   _unmovable_display_datas_17)
	TILE_SPRITE_LINE(SPR_HUGEHQ_WEST_GROUND,   _unmovable_display_datas_18)
	TILE_SPRITE_LINE(SPR_HUGEHQ_SOUTH,         _unmovable_display_nothing)
};

#undef TILE_SPRITE_LINE

const UnmovableSpec _original_unmovable[] = {
	{STR_5801_TRANSMITTER,          1,   1},
	{STR_5802_LIGHTHOUSE,           1,   1},
	{STR_2016_STATUE,               1,   1},
	{STR_5805_COMPANY_OWNED_LAND,   10,  2},
	{STR_5803_COMPANY_HEADQUARTERS, 1,   1},
};
