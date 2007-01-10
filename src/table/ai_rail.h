/* $Id$ */

typedef struct {
	byte mode;
	byte attr;
	TileIndexDiffC tileoffs;
} AiDefaultBlockData;

typedef struct {
	byte p0;
	byte p1;
	byte p2;
	byte p3;
	byte dir;
	const AiDefaultBlockData *data;
} AiDefaultRailBlock;

typedef struct {
	byte dir;
	const AiDefaultBlockData *data;
} AiDefaultRoadBlock;


#define MKHDR(a,b,c,d,e) a,b,c,d,e,
#define MKDEPOT(a, b, c)   {0, a, {b, c}}
#define MKSTATION(a, b, c) {1, a, {b, c}}
#define MKRAIL(a, b, c)    {2, a, {b, c}}
#define MKCLRRAIL(a, b, c) {3, a, {b, c}}
#define MKEND              {4, 0, {0, 0}}

static const AiDefaultBlockData _raildata_ai_0_data[] = {
	MKDEPOT(2, -1, 1),
	MKSTATION(0x15, 0, -1),
	MKRAIL(0x26, 0, 1),
	MKCLRRAIL(1, 0, 2),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_0 = {
	1, 2, 1, 0, 1, _raildata_ai_0_data
};

static const AiDefaultBlockData _raildata_ai_1_data[] = {
	MKDEPOT(2, -1, -1),
	MKRAIL(0x26, 0, -1),
	MKSTATION(0x15, 0, 0),
	MKCLRRAIL(3, 0, -2),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_1 = {
	1, 2, 1, 0, 3, _raildata_ai_1_data
};

static const AiDefaultBlockData _raildata_ai_2_data[] = {
	MKDEPOT(1, -1, -1),
	MKRAIL(0x15, -1, 0),
	MKSTATION(0x14, 0, 0),
	MKCLRRAIL(0, -2, 0),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_2 = {
	1, 2, 1, 0, 0, _raildata_ai_2_data
};

static const AiDefaultBlockData _raildata_ai_3_data[] = {
	MKDEPOT(1, 1, -1),
	MKRAIL(0x15, 1, 0),
	MKSTATION(0x14, -1, 0),
	MKCLRRAIL(2, 2, 0),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_3 = {
	1, 2, 1, 0, 2, _raildata_ai_3_data
};

static const AiDefaultBlockData _raildata_ai_4_data[] = {
	MKSTATION(0x15, 0, 0),
	MKCLRRAIL(3, 0, -1),
	MKCLRRAIL(1, 0, 2),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_4 = {
	1, 2, 2, 0, 255, _raildata_ai_4_data
};

static const AiDefaultBlockData _raildata_ai_5_data[] = {
	MKSTATION(0x14, 0, 0),
	MKCLRRAIL(0, -1, 0),
	MKCLRRAIL(2, 2, 0),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_5 = {
	1, 2, 2, 0, 255, _raildata_ai_5_data
};

static const AiDefaultBlockData _raildata_ai_6_data[] = {
	MKSTATION(0x27, 0, -2),
	MKRAIL(0xC2, 0, 1),
	MKRAIL(0xC2, 1, 1),
	MKRAIL(0x1A, 0, 2),
	MKRAIL(0x26, 1, 2),
	MKDEPOT(3, 1, 3),
	MKCLRRAIL(1, 0, 3),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_6 = {
	2, 3, 1, 0, 1, _raildata_ai_6_data
};

static const AiDefaultBlockData _raildata_ai_7_data[] = {
	MKDEPOT(1, 0, -3),
	MKRAIL(0x1A, 0, -2),
	MKRAIL(0x26, 1, -2),
	MKRAIL(0xC2, 0, -1),
	MKRAIL(0xC2, 1, -1),
	MKSTATION(0x27, 0, 0),
	MKCLRRAIL(3, 1, -3),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_7 = {
	2, 3, 1, 0, 3, _raildata_ai_7_data
};

static const AiDefaultBlockData _raildata_ai_8_data[] = {
	MKSTATION(0x26, -2, 0),
	MKRAIL(0xC1, 1, 0),
	MKRAIL(0xC1, 1, 1),
	MKRAIL(0x29, 2, 0),
	MKRAIL(0x15, 2, 1),
	MKDEPOT(0, 3, 0),
	MKCLRRAIL(2, 3, 1),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_8 = {
	2, 3, 1, 0, 2, _raildata_ai_8_data
};

static const AiDefaultBlockData _raildata_ai_9_data[] = {
	MKDEPOT(2, -3, -1),
	MKRAIL(0x29, -2, -1),
	MKRAIL(0x15, -2, 0),
	MKRAIL(0xC1, -1, -1),
	MKRAIL(0xC1, -1, 0),
	MKSTATION(0x26, 0, -1),
	MKCLRRAIL(0, -3, 0),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_9 = {
	2, 3, 1, 0, 0, _raildata_ai_9_data
};

static const AiDefaultBlockData _raildata_ai_10_data[] = {
	MKRAIL(0x1A, 0, -3),
	MKRAIL(0x20, 1, -3),
	MKRAIL(0xC2, 0, -2),
	MKRAIL(0xC2, 1, -2),
	MKSTATION(0x27, 0, -1),
	MKRAIL(0xC2, 0, 2),
	MKRAIL(0xC2, 1, 2),
	MKRAIL(0x1A, 0, 3),
	MKRAIL(0x4, 1, 3),
	MKCLRRAIL(3, 0, -4),
	MKCLRRAIL(1, 0, 4),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_10 = {
	2, 3, 2, 0, 255, _raildata_ai_10_data
};

static const AiDefaultBlockData _raildata_ai_11_data[] = {
	MKRAIL(0x29, -3, 0),
	MKRAIL(0x10, -3, 1),
	MKRAIL(0xC1, -2, 0),
	MKRAIL(0xC1, -2, 1),
	MKSTATION(0x26, -1, 0),
	MKRAIL(0xC1, 2, 0),
	MKRAIL(0xC1, 2, 1),
	MKRAIL(0x29, 3, 0),
	MKRAIL(0x4, 3, 1),
	MKCLRRAIL(0, -4, 0),
	MKCLRRAIL(2, 4, 0),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_11 = {
	2, 3, 2, 0, 255, _raildata_ai_11_data
};

static const AiDefaultBlockData _raildata_ai_12_data[] = {
	MKRAIL(0x88, -1, -3),
	MKRAIL(0x6, 0, -3),
	MKRAIL(0x2, -1, -2),
	MKRAIL(0x42, 0, -2),
	MKRAIL(0x2, -1, -1),
	MKRAIL(0x2, 0, -1),
	MKRAIL(0x2, -1, 0),
	MKRAIL(0x2, 0, 0),
	MKRAIL(0x82, -1, 1),
	MKRAIL(0x2, 0, 1),
	MKRAIL(0xA, -1, 2),
	MKRAIL(0x44, 0, 2),
	MKCLRRAIL(3, 0, -4),
	MKCLRRAIL(1, -1, 3),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_12 = {
	2, 3, 2, 1, 3, _raildata_ai_12_data
};

static const AiDefaultBlockData _raildata_ai_13_data[] = {
	MKRAIL(0x21, -3, 0),
	MKRAIL(0x50, -3, 1),
	MKRAIL(0x81, -2, 0),
	MKRAIL(0x1, -2, 1),
	MKRAIL(0x1, -1, 0),
	MKRAIL(0x1, -1, 1),
	MKRAIL(0x1, 0, 0),
	MKRAIL(0x1, 0, 1),
	MKRAIL(0x1, 1, 0),
	MKRAIL(0x41, 1, 1),
	MKRAIL(0xA0, 2, 0),
	MKRAIL(0x11, 2, 1),
	MKCLRRAIL(0, -4, 0),
	MKCLRRAIL(2, 3, 1),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_13 = {
	2, 3, 2, 1, 0, _raildata_ai_13_data
};

static const AiDefaultBlockData _raildata_ai_14_data[] = {
	MKRAIL(0x88, -1, -3),
	MKRAIL(0x6, 0, -3),
	MKRAIL(0x2, -1, -2),
	MKRAIL(0x42, 0, -2),
	MKRAIL(0x2, -1, -1),
	MKRAIL(0x2, 0, -1),
	MKRAIL(0x2, -1, 0),
	MKRAIL(0x2, 0, 0),
	MKRAIL(0x82, -1, 1),
	MKRAIL(0x2, 0, 1),
	MKRAIL(0xA, -1, 2),
	MKRAIL(0x44, 0, 2),
	MKCLRRAIL(1, -1, 3),
	MKCLRRAIL(3, 0, -4),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_14 = {
	2, 3, 2, 1, 1, _raildata_ai_14_data
};

static const AiDefaultBlockData _raildata_ai_15_data[] = {
	MKRAIL(0x21, -3, 0),
	MKRAIL(0x50, -3, 1),
	MKRAIL(0x81, -2, 0),
	MKRAIL(0x1, -2, 1),
	MKRAIL(0x1, -1, 0),
	MKRAIL(0x1, -1, 1),
	MKRAIL(0x1, 0, 0),
	MKRAIL(0x1, 0, 1),
	MKRAIL(0x1, 1, 0),
	MKRAIL(0x41, 1, 1),
	MKRAIL(0xA0, 2, 0),
	MKRAIL(0x11, 2, 1),
	MKCLRRAIL(2, 3, 1),
	MKCLRRAIL(0, -4, 0),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_15 = {
	2, 3, 2, 1, 2, _raildata_ai_15_data
};

static const AiDefaultBlockData _raildata_ai_16_data[] = {
	MKSTATION(0x17, 0, -2),
	MKRAIL(0x1A, 0, 1),
	MKCLRRAIL(1, 0, 2),
	MKDEPOT(0, 1, 1),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_16 = {
	1, 3, 1, 0, 1, _raildata_ai_16_data
};

static const AiDefaultBlockData _raildata_ai_17_data[] = {
	MKCLRRAIL(3, 0, -2),
	MKRAIL(0x26, 0, -1),
	MKDEPOT(2, -1, -1),
	MKSTATION(0x17, 0, 0),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_17 = {
	1, 3, 1, 0, 3, _raildata_ai_17_data
};

static const AiDefaultBlockData _raildata_ai_18_data[] = {
	MKCLRRAIL(0, -2, 0),
	MKRAIL(0x29, -1, 0),
	MKDEPOT(3, -1, 1),
	MKSTATION(0x16, 0, 0),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_18 = {
	1, 3, 1, 0, 0, _raildata_ai_18_data
};

static const AiDefaultBlockData _raildata_ai_19_data[] = {
	MKSTATION(0x16, -2, 0),
	MKDEPOT(2, 0, -1),
	MKRAIL(0x20, 1, -1),
	MKRAIL(0x15, 1, 0),
	MKCLRRAIL(2, 2, 0),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_19 = {
	1, 3, 1, 0, 2, _raildata_ai_19_data
};

static const AiDefaultBlockData _raildata_ai_20_data[] = {
	MKSTATION(0x26, -2, 0),
	MKRAIL(0xC1, 1, 0),
	MKRAIL(0xC1, 1, 1),
	MKRAIL(0x26, 2, 0),
	MKRAIL(0x26, 2, 1),
	MKDEPOT(1, 2, -1),
	MKCLRRAIL(1, 2, 2),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_20 = {
	2, 3, 1, 0, 1, _raildata_ai_20_data
};

static const AiDefaultBlockData _raildata_ai_21_data[] = {
	MKDEPOT(2, -3, -1),
	MKRAIL(0x3F, -2, -1),
	MKRAIL(0x10, -2, 0),
	MKRAIL(0xC1, -1, -1),
	MKRAIL(0xC1, -1, 0),
	MKSTATION(0x26, 0, -1),
	MKCLRRAIL(3, -2, -2),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_21 = {
	2, 3, 1, 0, 3, _raildata_ai_21_data
};

static const AiDefaultBlockData _raildata_ai_22_data[] = {
	MKSTATION(0x27, 0, -2),
	MKRAIL(0xC2, 0, 1),
	MKRAIL(0xC2, 1, 1),
	MKRAIL(0x15, 0, 2),
	MKRAIL(0x4, 1, 2),
	MKRAIL(0x15, -1, 2),
	MKDEPOT(1, -1, 1),
	MKCLRRAIL(0, -2, 2),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_22 = {
	2, 3, 1, 0, 0, _raildata_ai_22_data
};

static const AiDefaultBlockData _raildata_ai_23_data[] = {
	MKDEPOT(1, 0, -3),
	MKRAIL(0x1A, 0, -2),
	MKRAIL(0x29, 1, -2),
	MKRAIL(0xC2, 0, -1),
	MKRAIL(0xC2, 1, -1),
	MKSTATION(0x27, 0, 0),
	MKCLRRAIL(2, 2, -2),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_23 = {
	2, 3, 1, 0, 2, _raildata_ai_23_data
};

static const AiDefaultRailBlock * const _default_rail_track_data[] = {
	&_raildata_ai_0,
	&_raildata_ai_1,
	&_raildata_ai_2,
	&_raildata_ai_3,
	&_raildata_ai_4,
	&_raildata_ai_5,
	&_raildata_ai_6,
	&_raildata_ai_7,
	&_raildata_ai_8,
	&_raildata_ai_9,
	&_raildata_ai_10,
	&_raildata_ai_11,
	&_raildata_ai_12,
	&_raildata_ai_13,
	&_raildata_ai_14,
	&_raildata_ai_15,
	&_raildata_ai_16,
	&_raildata_ai_17,
	&_raildata_ai_18,
	&_raildata_ai_19,
	&_raildata_ai_20,
	&_raildata_ai_21,
	&_raildata_ai_22,
	&_raildata_ai_23,
	NULL
};

#undef MKHDR

#define MKHDR(a) a,{

static const AiDefaultBlockData _roaddata_ai_0_data[] = {
	MKDEPOT(2, -1,1),
	MKSTATION(0x2, -1,0),
	MKRAIL(0xC, 0,0),
	MKRAIL(0x9, 0,1),
	MKCLRRAIL(0, 0,-1),
	MKCLRRAIL(0, 1,0),
	MKCLRRAIL(0, 1,1),
	MKCLRRAIL(0, 0,2),
	MKEND
};

static const AiDefaultRoadBlock _roaddata_ai_0 = {
	0, _roaddata_ai_0_data
};

static const AiDefaultBlockData _roaddata_ai_1_data[] = {
	MKDEPOT(1, 0,-1),
	MKSTATION(0x1, 1,-1),
	MKRAIL(0x3, 0,0),
	MKRAIL(0x9, 1,0),
	MKCLRRAIL(0, -1,0),
	MKCLRRAIL(0, 0,1),
	MKCLRRAIL(0, 1,1),
	MKCLRRAIL(0, 2,0),
	MKEND
};

static const AiDefaultRoadBlock _roaddata_ai_1 = {
	0, _roaddata_ai_1_data
};

static const AiDefaultBlockData _roaddata_ai_2_data[] = {
	MKDEPOT(3, 1,1),
	MKSTATION(0x3, 0,1),
	MKRAIL(0x6, 0,0),
	MKRAIL(0xC, 1,0),
	MKCLRRAIL(0, -1,0),
	MKCLRRAIL(0, 0,-1),
	MKCLRRAIL(0, 1,-1),
	MKCLRRAIL(0, 2,0),
	MKEND
};

static const AiDefaultRoadBlock _roaddata_ai_2 = {
	0, _roaddata_ai_2_data
};

static const AiDefaultBlockData _roaddata_ai_3_data[] = {
	MKDEPOT(0, 1,0),
	MKSTATION(0x0, 1,1),
	MKRAIL(0x6, 0,0),
	MKRAIL(0x3, 0,1),
	MKCLRRAIL(0, 0,-1),
	MKCLRRAIL(0, -1,0),
	MKCLRRAIL(0, -1,1),
	MKCLRRAIL(0, 0,2),
	MKEND
};

static const AiDefaultRoadBlock _roaddata_ai_3 = {
	0, _roaddata_ai_3_data
};

static const AiDefaultBlockData _roaddata_ai_4_data[] = {
	MKSTATION(0x2, -1,0),
	MKRAIL(0x8, 0,0),
	MKCLRRAIL(0, 0,-1),
	MKCLRRAIL(0, 1,0),
	MKCLRRAIL(0, 0,1),
	MKEND
};

static const AiDefaultRoadBlock _roaddata_ai_4 = {
	1, _roaddata_ai_4_data
};

static const AiDefaultBlockData _roaddata_ai_5_data[] = {
	MKSTATION(0x3, 0,1),
	MKRAIL(0x4, 0,0),
	MKCLRRAIL(0, -1,0),
	MKCLRRAIL(0, 0,-1),
	MKCLRRAIL(0, 1,0),
	MKEND
};

static const AiDefaultRoadBlock _roaddata_ai_5 = {
	1, _roaddata_ai_5_data
};

static const AiDefaultBlockData _roaddata_ai_6_data[] = {
	MKSTATION(0x0, 1,1),
	MKRAIL(0x2, 0,1),
	MKCLRRAIL(0, 0,0),
	MKCLRRAIL(0, -1,0),
	MKCLRRAIL(0, 0,2),
	MKEND
};

static const AiDefaultRoadBlock _roaddata_ai_6 = {
	1, _roaddata_ai_6_data
};

static const AiDefaultBlockData _roaddata_ai_7_data[] = {
	MKSTATION(0x1, 1,-1),
	MKRAIL(0x1, 1,0),
	MKCLRRAIL(0, 0,0),
	MKCLRRAIL(0, 1,1),
	MKCLRRAIL(0, 2,0),
	MKEND
};

static const AiDefaultRoadBlock _roaddata_ai_7 = {
	1, _roaddata_ai_7_data
};


static const AiDefaultRoadBlock * const _road_default_block_data[] = {
	&_roaddata_ai_0,
	&_roaddata_ai_1,
	&_roaddata_ai_2,
	&_roaddata_ai_3,
	&_roaddata_ai_4,
	&_roaddata_ai_5,
	&_roaddata_ai_6,
	&_roaddata_ai_7,
	NULL
};

#define MKAIR(a, b, c) {0, a, {b, c}}

static const AiDefaultBlockData _airportdata_ai_0[] = {
	MKAIR(1, 0, 0),
	{1, 0, {0, 0}},
};

static const AiDefaultBlockData _airportdata_ai_1[] = {
	MKAIR(0, 0, 0),
	{1, 0, {0, 0}}
};

static const AiDefaultBlockData * const _airport_default_block_data[] = {
	_airportdata_ai_0, // city airport
	_airportdata_ai_1, // country airport
	NULL
};
