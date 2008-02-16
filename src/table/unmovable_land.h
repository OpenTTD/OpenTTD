/* $Id$ */


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
	TILE_SEQ_LINE(20, 0xA34 | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_9[] = {
	TILE_SEQ_LINE(20, 0xA36 | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_10[] = {
	TILE_SEQ_LINE(20, 0xA38 | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_12[] = {
	TILE_SEQ_LINE(50, 0xA3B | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_13[] = {
	TILE_SEQ_LINE(50, 0xA3D | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_14[] = {
	TILE_SEQ_LINE(50, 0xA3F | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_16[] = {
	TILE_SEQ_LINE(60, 0xA42 | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_17[] = {
	TILE_SEQ_LINE(60, 0xA44 | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_18[] = {
	TILE_SEQ_LINE(60, 0xA46 | (1 << PALETTE_MODIFIER_COLOR))
	TILE_SEQ_END()
};

#undef TILE_SEQ_LINE
#undef TILE_SEQ_END

#define TILE_SPRITE_LINE(img, dtss) { {img | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE}, dtss },

static const DrawTileSprites _unmovable_display_datas[] = {
	TILE_SPRITE_LINE(0xA2B, _unmovable_display_nothing)
	TILE_SPRITE_LINE(0xA2C, _unmovable_display_nothing)
	TILE_SPRITE_LINE(0xA2D, _unmovable_display_nothing)
	TILE_SPRITE_LINE(0xA2E, _unmovable_display_nothing)
	TILE_SPRITE_LINE(0xA2F, _unmovable_display_nothing)
	TILE_SPRITE_LINE(0xA30, _unmovable_display_nothing)
	TILE_SPRITE_LINE(0xA31, _unmovable_display_nothing)
	TILE_SPRITE_LINE(0xA32, _unmovable_display_nothing)
	TILE_SPRITE_LINE(0xA33, _unmovable_display_datas_8)
	TILE_SPRITE_LINE(0xA35, _unmovable_display_datas_9)
	TILE_SPRITE_LINE(0xA37, _unmovable_display_datas_10)
	TILE_SPRITE_LINE(0xA39, _unmovable_display_nothing)
	TILE_SPRITE_LINE(0xA3A, _unmovable_display_datas_12)
	TILE_SPRITE_LINE(0xA3C, _unmovable_display_datas_13)
	TILE_SPRITE_LINE(0xA3E, _unmovable_display_datas_14)
	TILE_SPRITE_LINE(0xA40, _unmovable_display_nothing)
	TILE_SPRITE_LINE(0xA41, _unmovable_display_datas_16)
	TILE_SPRITE_LINE(0xA43, _unmovable_display_datas_17)
	TILE_SPRITE_LINE(0xA45, _unmovable_display_datas_18)
	TILE_SPRITE_LINE(0xA47, _unmovable_display_nothing)
};

#undef TILE_SPRITE_LINE
