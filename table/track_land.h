typedef struct DrawTrackSeqStruct {
	uint16 image;
	byte subcoord_x;
	byte subcoord_y;
	byte width;
	byte height;
} DrawTrackSeqStruct;

#define TILE_SEQ_BEGIN(x) { x, 0, 0, 0, 0 },
#define TILE_SEQ_LINE(a, b, c, d, e) { a, b, c, d, e },
#define TILE_SEQ_END() { 0, 0, 0, 0, 0 }

static const DrawTrackSeqStruct _track_depot_layout_table_0[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(0x842B, 2, 13, 13, 1)
	TILE_SEQ_END()
};

static const DrawTrackSeqStruct _track_depot_layout_table_1[] = {
	TILE_SEQ_BEGIN(0x83F3)
	TILE_SEQ_LINE(0x8427, 2, 2, 1, 13)
	TILE_SEQ_LINE(0x8428, 13, 2, 1, 13)
	TILE_SEQ_END()
};

static const DrawTrackSeqStruct _track_depot_layout_table_2[] = {
	TILE_SEQ_BEGIN(0x83F4)
	TILE_SEQ_LINE(0x8429, 2, 2, 13, 1)
	TILE_SEQ_LINE(0x842A, 2, 13, 13, 1)
	TILE_SEQ_END()
};

static const DrawTrackSeqStruct _track_depot_layout_table_3[] = {
	TILE_SEQ_BEGIN(0xF8D)
	TILE_SEQ_LINE(0x842C, 13, 2, 1, 13)
	TILE_SEQ_END()
};

static const DrawTrackSeqStruct _track_waypoint_table_0[] = {
	TILE_SEQ_BEGIN(0x83F4)
	TILE_SEQ_LINE(0x8000 + SPR_OPENTTD_BASE+18,  0,  0,  16,  5)
	TILE_SEQ_LINE(0x8000 + +19,  0, 11,  16,  5)
	TILE_SEQ_END()
};

static const DrawTrackSeqStruct _track_waypoint_table_1[] = {
	TILE_SEQ_BEGIN(0x83F3)
	TILE_SEQ_LINE(0x8000 + SPR_OPENTTD_BASE+20,   0,  0, 5, 16)
	TILE_SEQ_LINE(0x8000 + SPR_OPENTTD_BASE+21,  11,  0, 5, 16)
	TILE_SEQ_END()
};


static const DrawTrackSeqStruct* const _track_depot_layout_table[] = {
	_track_depot_layout_table_0,
	_track_depot_layout_table_1,
	_track_depot_layout_table_2,
	_track_depot_layout_table_3,

	_track_waypoint_table_0,
	_track_waypoint_table_1,
};

static const byte _track_sloped_sprites[14] = {
	14, 15, 22, 13,
	 0, 21, 17, 12,
	23,  0, 18, 20,
	19, 16
};
