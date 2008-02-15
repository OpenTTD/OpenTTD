/* $Id$ */

struct DrawTileUnmovableStruct {
	uint16 image;
	byte subcoord_x;
	byte subcoord_y;
	byte width;
	byte height;
	byte z_size;
	byte unused;
};

static const DrawTileUnmovableStruct _draw_tile_unmovable_data[] = {
	{0xA29, 7, 7, 2, 2, 70, 0},
	{0xA2A, 4, 4, 7, 7, 61, 0},
};

#define TILE_SEQ_LINE(sz, img) { 0, 0, 0, 16, 16, sz, img, PAL_NONE },
#define TILE_SEQ_END() { (byte)0x80, 0, 0, 0, 0, 0, 0, 0 }

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

static const DrawTileSprites _unmovable_display_datas[] = {
	{ 0xA2B | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_nothing },
	{ 0xA2C | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_nothing },
	{ 0xA2D | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_nothing },
	{ 0xA2E | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_nothing },
	{ 0xA2F | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_nothing },
	{ 0xA30 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_nothing },
	{ 0xA31 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_nothing },
	{ 0xA32 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_nothing },
	{ 0xA33 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_datas_8 },
	{ 0xA35 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_datas_9 },
	{ 0xA37 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_datas_10 },
	{ 0xA39 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_nothing },
	{ 0xA3A | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_datas_12 },
	{ 0xA3C | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_datas_13 },
	{ 0xA3E | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_datas_14 },
	{ 0xA40 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_nothing },
	{ 0xA41 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_datas_16 },
	{ 0xA43 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_datas_17 },
	{ 0xA45 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_datas_18 },
	{ 0xA47 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, _unmovable_display_nothing },
};
