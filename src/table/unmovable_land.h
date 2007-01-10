/* $Id$ */

typedef struct DrawTileUnmovableStruct {
	uint16 image;
	byte subcoord_x;
	byte subcoord_y;
	byte width;
	byte height;
	byte z_size;
	byte unused;
} DrawTileUnmovableStruct;

#define TILE_SEQ_END() { (byte)0x80, 0, 0, 0, 0, 0, 0 }

static const DrawTileUnmovableStruct _draw_tile_unmovable_data[] = {
	{0xA29, 7, 7, 2, 2, 70, 0},
	{0xA2A, 4, 4, 7, 7, 61, 0},
};


static const DrawTileSeqStruct _unmovable_display_datas_0[] = {
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_1[] = {
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_2[] = {
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_3[] = {
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_4[] = {
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_5[] = {
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_6[] = {
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_7[] = {
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_8[] = {
	{   0,  0,  0, 16, 16, 20, 0xA34 | PALETTE_MODIFIER_COLOR },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_9[] = {
	{   0,  0,  0, 16, 16, 20, 0xA36 | PALETTE_MODIFIER_COLOR },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_10[] = {
	{   0,  0,  0, 16, 16, 20, 0xA38 | PALETTE_MODIFIER_COLOR },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_11[] = {
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_12[] = {
	{   0,  0,  0, 16, 16, 50, 0xA3B | PALETTE_MODIFIER_COLOR },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_13[] = {
	{   0,  0,  0, 16, 16, 50, 0xA3D | PALETTE_MODIFIER_COLOR },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_14[] = {
	{   0,  0,  0, 16, 16, 50, 0xA3F | PALETTE_MODIFIER_COLOR },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_15[] = {
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_16[] = {
	{   0,  0,  0, 16, 16, 60, 0xA42 | PALETTE_MODIFIER_COLOR },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_17[] = {
	{   0,  0,  0, 16, 16, 60, 0xA44 | PALETTE_MODIFIER_COLOR },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_18[] = {
	{   0,  0,  0, 16, 16, 60, 0xA46 | PALETTE_MODIFIER_COLOR },
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_display_datas_19[] = {
	TILE_SEQ_END()
};

static const DrawTileSprites _unmovable_display_datas[] = {
	{ 0xA2B | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_0 },
	{ 0xA2C | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_1 },
	{ 0xA2D | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_2 },
	{ 0xA2E | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_3 },
	{ 0xA2F | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_4 },
	{ 0xA30 | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_5 },
	{ 0xA31 | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_6 },
	{ 0xA32 | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_7 },
	{ 0xA33 | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_8 },
	{ 0xA35 | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_9 },
	{ 0xA37 | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_10 },
	{ 0xA39 | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_11 },
	{ 0xA3A | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_12 },
	{ 0xA3C | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_13 },
	{ 0xA3E | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_14 },
	{ 0xA40 | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_15 },
	{ 0xA41 | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_16 },
	{ 0xA43 | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_17 },
	{ 0xA45 | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_18 },
	{ 0xA47 | PALETTE_MODIFIER_COLOR, _unmovable_display_datas_19 },
};
