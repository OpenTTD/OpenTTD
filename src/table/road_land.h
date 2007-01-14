/* $Id$ */

#define TILE_SEQ_LINE(img, pal, dx, dy, sx, sy) { dx, dy, 0, sx, sy, 20, img, pal },
#define TILE_SEQ_END() { 0, 0, 0, 0, 0, 0, 0, 0 }

static const DrawTileSeqStruct _road_depot_NE[] = {
	TILE_SEQ_LINE(0x584 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, 0, 15, 16, 1)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _road_depot_SE[] = {
	TILE_SEQ_LINE(0x580, PAL_NONE, 0, 0, 1, 16)
	TILE_SEQ_LINE(0x581 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, 15, 0, 1, 16)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _road_depot_SW[] = {
	TILE_SEQ_LINE(0x582, PAL_NONE, 0, 0, 16, 1)
	TILE_SEQ_LINE(0x583 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, 0, 15, 16, 1)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _road_depot_NW[] = {
	TILE_SEQ_LINE(0x585 | (1 << PALETTE_MODIFIER_COLOR), PAL_NONE, 15, 0, 1, 16)
	TILE_SEQ_END()
};

static const DrawTileSprites _road_depot[] = {
	{ 0xA4A, PAL_NONE, _road_depot_NE },
	{ 0xA4A, PAL_NONE, _road_depot_SE },
	{ 0xA4A, PAL_NONE, _road_depot_SW },
	{ 0xA4A, PAL_NONE, _road_depot_NW }
};

#undef TILE_SEQ_BEGIN
#undef TILE_SEQ_LINE
#undef TILE_SEQ_END


static const SpriteID _road_tile_sprites_1[16] = {
	0,     0x546, 0x545, 0x53B, 0x544, 0x534, 0x53E, 0x539,
  0x543, 0x53C, 0x535, 0x538, 0x53D, 0x537, 0x53A, 0x536
};



#define MAKELINE(a, b, c) { a, b, c },
#define ENDLINE { 0, 0, 0 }
static const DrawRoadTileStruct _road_display_datas2_0[] = {
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_1[] = {
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_2[] = {
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_3[] = {
	MAKELINE(0x57f,  1,  8)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_4[] = {
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_5[] = {
	MAKELINE(0x57f,  1,  8)
	MAKELINE(0x57e, 14,  8)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_6[] = {
	MAKELINE(0x57e,  8,  1)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_7[] = {
	MAKELINE(0x57f,  1,  8)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_8[] = {
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_9[] = {
	MAKELINE(0x57f,  8, 14)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_10[] = {
	MAKELINE(0x57f,  8, 14)
	MAKELINE(0x57e,  8,  1)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_11[] = {
	MAKELINE(0x57f,  8, 14)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_12[] = {
	MAKELINE(0x57e,  8,  1)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_13[] = {
	MAKELINE(0x57e, 14,  8)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_14[] = {
	MAKELINE(0x57e,  8,  1)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_15[] = {
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_16[] = {
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_17[] = {
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_18[] = {
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_19[] = {
	MAKELINE(0x1212,  0,  2)
	MAKELINE(0x1212,  3,  9)
	MAKELINE(0x1212, 10, 12)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_20[] = {
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_21[] = {
	MAKELINE(0x1212,  0,  2)
	MAKELINE(0x1212,  0, 10)
	MAKELINE(0x1212, 12,  2)
	MAKELINE(0x1212, 12, 10)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_22[] = {
	MAKELINE(0x1212, 10,  0)
	MAKELINE(0x1212,  3,  3)
	MAKELINE(0x1212,  0, 10)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_23[] = {
	MAKELINE(0x1212,  0,  2)
	MAKELINE(0x1212,  0, 10)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_24[] = {
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_25[] = {
	MAKELINE(0x1212, 12,  2)
	MAKELINE(0x1212,  9,  9)
	MAKELINE(0x1212,  2, 12)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_26[] = {
	MAKELINE(0x1212,  2,  0)
	MAKELINE(0x1212, 10,  0)
	MAKELINE(0x1212,  2, 12)
	MAKELINE(0x1212, 10, 12)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_27[] = {
	MAKELINE(0x1212,  2, 12)
	MAKELINE(0x1212, 10, 12)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_28[] = {
	MAKELINE(0x1212,  2,  0)
	MAKELINE(0x1212,  9,  3)
	MAKELINE(0x1212, 12, 10)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_29[] = {
	MAKELINE(0x1212, 12,  2)
	MAKELINE(0x1212, 12, 10)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_30[] = {
	MAKELINE(0x1212,  2, 0)
	MAKELINE(0x1212, 10, 0)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_31[] = {
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_32[] = {
	ENDLINE
};

#undef MAKELINE
#undef ENDLINE

static const DrawRoadTileStruct* const _road_display_table_1[] = {
	_road_display_datas2_32,_road_display_datas2_32,
	_road_display_datas2_32,_road_display_datas2_32,
	_road_display_datas2_32,_road_display_datas2_32,
	_road_display_datas2_32,_road_display_datas2_32,
	_road_display_datas2_32,_road_display_datas2_32,
	_road_display_datas2_32,_road_display_datas2_32,
	_road_display_datas2_32,_road_display_datas2_32,
	_road_display_datas2_32,_road_display_datas2_32,
};

static const DrawRoadTileStruct* const _road_display_table_2[] = {
	_road_display_datas2_0,
	_road_display_datas2_1,
	_road_display_datas2_2,
	_road_display_datas2_3,
	_road_display_datas2_4,
	_road_display_datas2_5,
	_road_display_datas2_6,
	_road_display_datas2_7,
	_road_display_datas2_8,
	_road_display_datas2_9,
	_road_display_datas2_10,
	_road_display_datas2_11,
	_road_display_datas2_12,
	_road_display_datas2_13,
	_road_display_datas2_14,
	_road_display_datas2_15,
};

static const DrawRoadTileStruct* const _road_display_table_3[] = {
	_road_display_datas2_16,
	_road_display_datas2_17,
	_road_display_datas2_18,
	_road_display_datas2_19,
	_road_display_datas2_20,
	_road_display_datas2_21,
	_road_display_datas2_22,
	_road_display_datas2_23,

	_road_display_datas2_24,
	_road_display_datas2_25,
	_road_display_datas2_26,
	_road_display_datas2_27,
	_road_display_datas2_28,
	_road_display_datas2_29,
	_road_display_datas2_30,
	_road_display_datas2_31,
};

static const DrawRoadTileStruct* const * const _road_display_table[] = {
	_road_display_table_1,
	_road_display_table_1,
	_road_display_table_1,
	_road_display_table_2,
	_road_display_table_1,
	_road_display_table_3,
};
