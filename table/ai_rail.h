typedef struct {
	byte mode;
	byte attr;
	TileIndexDiff tileoffs;
} AiDefaultBlockData;

typedef struct {
	byte p0;
	byte p1;
	byte p2;
	byte p3;
	byte dir;
	AiDefaultBlockData data[VARARRAY_SIZE];
} AiDefaultRailBlock;

typedef struct {
	byte dir;
	AiDefaultBlockData data[VARARRAY_SIZE];
} AiDefaultRoadBlock;


#define MKHDR(a,b,c,d,e) a,b,c,d,e,{
#define MKDEPOT(a,b) {0,a,b}
#define MKSTATION(a,b) {1,a,b}
#define MKRAIL(a,b) {2,a,b}
#define MKCLRRAIL(a,b) {3,a,b}
#define MKEND {4}}

static const AiDefaultRailBlock _raildata_ai_0 = {
	MKHDR(1,2,1,0,1)
	MKDEPOT(2,TILE_XY(-1,1)),
	MKSTATION(0x15,TILE_XY(0,-1)),
	MKRAIL(0x26,TILE_XY(0,1)),
	MKCLRRAIL(1,TILE_XY(0,2)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_1 = {
	MKHDR(1,2,1,0,3)
	MKDEPOT(2,TILE_XY(-1,-1)),
	MKRAIL(0x26,TILE_XY(0,-1)),
	MKSTATION(0x15,TILE_XY(0,0)),
	MKCLRRAIL(3,TILE_XY(0,-2)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_2 = {
	MKHDR(1,2,1,0,0)
	MKDEPOT(1,TILE_XY(-1,-1)),
	MKRAIL(0x15,TILE_XY(-1,0)),
	MKSTATION(0x14,TILE_XY(0,0)),
	MKCLRRAIL(0,TILE_XY(-2,0)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_3 = {
	MKHDR(1,2,1,0,2)
	MKDEPOT(1,TILE_XY(1,-1)),
	MKRAIL(0x15,TILE_XY(1,0)),
	MKSTATION(0x14,TILE_XY(-1,0)),
	MKCLRRAIL(2,TILE_XY(2,0)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_4 = {
	MKHDR(1,2,2,0,255)
	MKSTATION(0x15,TILE_XY(0,0)),
	MKCLRRAIL(3,TILE_XY(0,-1)),
	MKCLRRAIL(1,TILE_XY(0,2)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_5 = {
	MKHDR(1,2,2,0,255)
	MKSTATION(0x14,TILE_XY(0,0)),
	MKCLRRAIL(0,TILE_XY(-1,0)),
	MKCLRRAIL(2,TILE_XY(2,0)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_6 = {
	MKHDR(2,3,1,0,1)
	MKSTATION(0x27,TILE_XY(0,-2)),
	MKRAIL(0xC2,TILE_XY(0,1)),
	MKRAIL(0xC2,TILE_XY(1,1)),
	MKRAIL(0x1A,TILE_XY(0,2)),
	MKRAIL(0x26,TILE_XY(1,2)),
	MKDEPOT(3,TILE_XY(1,3)),
	MKCLRRAIL(1,TILE_XY(0,3)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_7 = {
	MKHDR(2,3,1,0,3)
	MKDEPOT(1,TILE_XY(0,-3)),
	MKRAIL(0x1A,TILE_XY(0,-2)),
	MKRAIL(0x26,TILE_XY(1,-2)),
	MKRAIL(0xC2,TILE_XY(0,-1)),
	MKRAIL(0xC2,TILE_XY(1,-1)),
	MKSTATION(0x27,TILE_XY(0,0)),
	MKCLRRAIL(3,TILE_XY(1,-3)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_8 = {
	MKHDR(2,3,1,0,2)
	MKSTATION(0x26,TILE_XY(-2,0)),
	MKRAIL(0xC1,TILE_XY(1,0)),
	MKRAIL(0xC1,TILE_XY(1,1)),
	MKRAIL(0x29,TILE_XY(2,0)),
	MKRAIL(0x15,TILE_XY(2,1)),
	MKDEPOT(0,TILE_XY(3,0)),
	MKCLRRAIL(2,TILE_XY(3,1)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_9 = {
	MKHDR(2,3,1,0,0)
	MKDEPOT(2,TILE_XY(-3,-1)),
	MKRAIL(0x29,TILE_XY(-2,-1)),
	MKRAIL(0x15,TILE_XY(-2,0)),
	MKRAIL(0xC1,TILE_XY(-1,-1)),
	MKRAIL(0xC1,TILE_XY(-1,0)),
	MKSTATION(0x26,TILE_XY(0,-1)),
	MKCLRRAIL(0,TILE_XY(-3,0)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_10 = {
	MKHDR(2,3,2,0,255)
	MKRAIL(0x1A,TILE_XY(0,-3)),
	MKRAIL(0x20,TILE_XY(1,-3)),
	MKRAIL(0xC2,TILE_XY(0,-2)),
	MKRAIL(0xC2,TILE_XY(1,-2)),
	MKSTATION(0x27,TILE_XY(0,-1)),
	MKRAIL(0xC2,TILE_XY(0,2)),
	MKRAIL(0xC2,TILE_XY(1,2)),
	MKRAIL(0x1A,TILE_XY(0,3)),
	MKRAIL(0x4,TILE_XY(1,3)),
	MKCLRRAIL(3,TILE_XY(0,-4)),
	MKCLRRAIL(1,TILE_XY(0,4)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_11 = {
	MKHDR(2,3,2,0,255)
	MKRAIL(0x29,TILE_XY(-3,0)),
	MKRAIL(0x10,TILE_XY(-3,1)),
	MKRAIL(0xC1,TILE_XY(-2,0)),
	MKRAIL(0xC1,TILE_XY(-2,1)),
	MKSTATION(0x26,TILE_XY(-1,0)),
	MKRAIL(0xC1,TILE_XY(2,0)),
	MKRAIL(0xC1,TILE_XY(2,1)),
	MKRAIL(0x29,TILE_XY(3,0)),
	MKRAIL(0x4,TILE_XY(3,1)),
	MKCLRRAIL(0,TILE_XY(-4,0)),
	MKCLRRAIL(2,TILE_XY(4,0)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_12 = {
	MKHDR(2,3,2,1,3)
	MKRAIL(0x88,TILE_XY(-1,-3)),
	MKRAIL(0x6,TILE_XY(0,-3)),
	MKRAIL(0x2,TILE_XY(-1,-2)),
	MKRAIL(0x42,TILE_XY(0,-2)),
	MKRAIL(0x2,TILE_XY(-1,-1)),
	MKRAIL(0x2,TILE_XY(0,-1)),
	MKRAIL(0x2,TILE_XY(-1,0)),
	MKRAIL(0x2,TILE_XY(0,0)),
	MKRAIL(0x82,TILE_XY(-1,1)),
	MKRAIL(0x2,TILE_XY(0,1)),
	MKRAIL(0xA,TILE_XY(-1,2)),
	MKRAIL(0x44,TILE_XY(0,2)),
	MKCLRRAIL(3,TILE_XY(0,-4)),
	MKCLRRAIL(1,TILE_XY(-1,3)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_13 = {
	MKHDR(2,3,2,1,0)
	MKRAIL(0x21,TILE_XY(-3,0)),
	MKRAIL(0x90,TILE_XY(-3,1)),
	MKRAIL(0x81,TILE_XY(-2,0)),
	MKRAIL(0x1,TILE_XY(-2,1)),
	MKRAIL(0x1,TILE_XY(-1,0)),
	MKRAIL(0x1,TILE_XY(-1,1)),
	MKRAIL(0x1,TILE_XY(0,0)),
	MKRAIL(0x1,TILE_XY(0,1)),
	MKRAIL(0x1,TILE_XY(1,0)),
	MKRAIL(0x41,TILE_XY(1,1)),
	MKRAIL(0x60,TILE_XY(2,0)),
	MKRAIL(0x11,TILE_XY(2,1)),
	MKCLRRAIL(0,TILE_XY(-4,0)),
	MKCLRRAIL(2,TILE_XY(3,1)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_14 = {
	MKHDR(2,3,2,1,1)
	MKRAIL(0x88,TILE_XY(-1,-3)),
	MKRAIL(0x6,TILE_XY(0,-3)),
	MKRAIL(0x2,TILE_XY(-1,-2)),
	MKRAIL(0x42,TILE_XY(0,-2)),
	MKRAIL(0x2,TILE_XY(-1,-1)),
	MKRAIL(0x2,TILE_XY(0,-1)),
	MKRAIL(0x2,TILE_XY(-1,0)),
	MKRAIL(0x2,TILE_XY(0,0)),
	MKRAIL(0x82,TILE_XY(-1,1)),
	MKRAIL(0x2,TILE_XY(0,1)),
	MKRAIL(0xA,TILE_XY(-1,2)),
	MKRAIL(0x44,TILE_XY(0,2)),
	MKCLRRAIL(1,TILE_XY(-1,3)),
	MKCLRRAIL(3,TILE_XY(0,-4)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_15 = {
	MKHDR(2,3,2,1,2)
	MKRAIL(0x21,TILE_XY(-3,0)),
	MKRAIL(0x90,TILE_XY(-3,1)),
	MKRAIL(0x81,TILE_XY(-2,0)),
	MKRAIL(0x1,TILE_XY(-2,1)),
	MKRAIL(0x1,TILE_XY(-1,0)),
	MKRAIL(0x1,TILE_XY(-1,1)),
	MKRAIL(0x1,TILE_XY(0,0)),
	MKRAIL(0x1,TILE_XY(0,1)),
	MKRAIL(0x1,TILE_XY(1,0)),
	MKRAIL(0x41,TILE_XY(1,1)),
	MKRAIL(0x60,TILE_XY(2,0)),
	MKRAIL(0x11,TILE_XY(2,1)),
	MKCLRRAIL(2,TILE_XY(3,1)),
	MKCLRRAIL(0,TILE_XY(-4,0)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_16 = {
	MKHDR(1,3,1,0,1)
	MKSTATION(0x17,TILE_XY(0,-2)),
	MKRAIL(0x1A,TILE_XY(0,1)),
	MKCLRRAIL(1,TILE_XY(0,2)),
	MKDEPOT(0,TILE_XY(1,1)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_17 = {
	MKHDR(1,3,1,0,3)
	MKCLRRAIL(3,TILE_XY(0,-2)),
	MKRAIL(0x26,TILE_XY(0,-1)),
	MKDEPOT(2,TILE_XY(-1,-1)),
	MKSTATION(0x17,TILE_XY(0,0)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_18 = {
	MKHDR(1,3,1,0,0)
	MKCLRRAIL(0,TILE_XY(-2,0)),
	MKRAIL(0x29,TILE_XY(-1,0)),
	MKDEPOT(3,TILE_XY(-1,1)),
	MKSTATION(0x16,TILE_XY(0,0)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_19 = {
	MKHDR(1,3,1,0,2)
	MKSTATION(0x16,TILE_XY(-2,0)),
	MKDEPOT(2,TILE_XY(0,-1)),
	MKRAIL(0x20,TILE_XY(1,-1)),
	MKRAIL(0x15,TILE_XY(1,0)),
	MKCLRRAIL(2,TILE_XY(2,0)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_20 = {
	MKHDR(2,3,1,0,1)
	MKSTATION(0x26,TILE_XY(-2,0)),
	MKRAIL(0xC1,TILE_XY(1,0)),
	MKRAIL(0xC1,TILE_XY(1,1)),
	MKRAIL(0x26,TILE_XY(2,0)),
	MKRAIL(0x26,TILE_XY(2,1)),
	MKDEPOT(1,TILE_XY(2,-1)),
	MKCLRRAIL(1,TILE_XY(2,2)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_21 = {
	MKHDR(2,3,1,0,3)
	MKDEPOT(2,TILE_XY(-3,-1)),
	MKRAIL(0x3F,TILE_XY(-2,-1)),
	MKRAIL(0x10,TILE_XY(-2,0)),
	MKRAIL(0xC1,TILE_XY(-1,-1)),
	MKRAIL(0xC1,TILE_XY(-1,0)),
	MKSTATION(0x26,TILE_XY(0,-1)),
	MKCLRRAIL(3,TILE_XY(-2,-2)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_22 = {
	MKHDR(2,3,1,0,0)
	MKSTATION(0x27,TILE_XY(0,-2)),
	MKRAIL(0xC2,TILE_XY(0,1)),
	MKRAIL(0xC2,TILE_XY(1,1)),
	MKRAIL(0x15,TILE_XY(0,2)),
	MKRAIL(0x4,TILE_XY(1,2)),
	MKRAIL(0x15,TILE_XY(-1,2)),
	MKDEPOT(1,TILE_XY(-1,1)),
	MKCLRRAIL(0,TILE_XY(-2,2)),
	MKEND
};

static const AiDefaultRailBlock _raildata_ai_23 = {
	MKHDR(2,3,1,0,2)
	MKDEPOT(1,TILE_XY(0,-3)),
	MKRAIL(0x1A,TILE_XY(0,-2)),
	MKRAIL(0x29,TILE_XY(1,-2)),
	MKRAIL(0xC2,TILE_XY(0,-1)),
	MKRAIL(0xC2,TILE_XY(1,-1)),
	MKSTATION(0x27,TILE_XY(0,0)),
	MKCLRRAIL(2,TILE_XY(2,-2)),
	MKEND
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

static const AiDefaultRoadBlock _roaddata_ai_0 = {
	MKHDR(0)
	MKDEPOT(2,TILE_XY(-1,1)),
	MKSTATION(0x2,TILE_XY(-1,0)),
	MKRAIL(0xC,TILE_XY(0,0)),
	MKRAIL(0x9,TILE_XY(0,1)),
	MKCLRRAIL(0,TILE_XY(0,-1)),
	MKCLRRAIL(0,TILE_XY(1,0)),
	MKCLRRAIL(0,TILE_XY(1,1)),
	MKCLRRAIL(0,TILE_XY(0,2)),
	MKEND
};

static const AiDefaultRoadBlock _roaddata_ai_1 = {
	MKHDR(0)
	MKDEPOT(1,TILE_XY(0,-1)),
	MKSTATION(0x1,TILE_XY(1,-1)),
	MKRAIL(0x3,TILE_XY(0,0)),
	MKRAIL(0x9,TILE_XY(1,0)),
	MKCLRRAIL(0,TILE_XY(-1,0)),
	MKCLRRAIL(0,TILE_XY(0,1)),
	MKCLRRAIL(0,TILE_XY(1,1)),
	MKCLRRAIL(0,TILE_XY(2,0)),
	MKEND
};

static const AiDefaultRoadBlock _roaddata_ai_2 = {
	MKHDR(0)
	MKDEPOT(3,TILE_XY(1,1)),
	MKSTATION(0x3,TILE_XY(0,1)),
	MKRAIL(0x6,TILE_XY(0,0)),
	MKRAIL(0xC,TILE_XY(1,0)),
	MKCLRRAIL(0,TILE_XY(-1,0)),
	MKCLRRAIL(0,TILE_XY(0,-1)),
	MKCLRRAIL(0,TILE_XY(1,-1)),
	MKCLRRAIL(0,TILE_XY(2,0)),
	MKEND
};

static const AiDefaultRoadBlock _roaddata_ai_3 = {
	MKHDR(0)
	MKDEPOT(0,TILE_XY(1,0)),
	MKSTATION(0x0,TILE_XY(1,1)),
	MKRAIL(0x6,TILE_XY(0,0)),
	MKRAIL(0x3,TILE_XY(0,1)),
	MKCLRRAIL(0,TILE_XY(0,-1)),
	MKCLRRAIL(0,TILE_XY(-1,0)),
	MKCLRRAIL(0,TILE_XY(-1,1)),
	MKCLRRAIL(0,TILE_XY(0,2)),
	MKEND
};

static const AiDefaultRoadBlock _roaddata_ai_4 = {
	MKHDR(1)
	MKSTATION(0x2,TILE_XY(-1,0)),
	MKRAIL(0x8,TILE_XY(0,0)),
	MKCLRRAIL(0,TILE_XY(0,-1)),
	MKCLRRAIL(0,TILE_XY(1,0)),
	MKCLRRAIL(0,TILE_XY(0,1)),
	MKEND
};

static const AiDefaultRoadBlock _roaddata_ai_5 = {
	MKHDR(1)
	MKSTATION(0x3,TILE_XY(0,1)),
	MKRAIL(0x4,TILE_XY(0,0)),
	MKCLRRAIL(0,TILE_XY(-1,0)),
	MKCLRRAIL(0,TILE_XY(0,-1)),
	MKCLRRAIL(0,TILE_XY(1,0)),
	MKEND
};

static const AiDefaultRoadBlock _roaddata_ai_6 = {
	MKHDR(1)
	MKSTATION(0x0,TILE_XY(1,1)),
	MKRAIL(0x2,TILE_XY(0,1)),
	MKCLRRAIL(0,TILE_XY(0,0)),
	MKCLRRAIL(0,TILE_XY(-1,0)),
	MKCLRRAIL(0,TILE_XY(0,2)),
	MKEND
};

static const AiDefaultRoadBlock _roaddata_ai_7 = {
	MKHDR(1)
	MKSTATION(0x1,TILE_XY(1,-1)),
	MKRAIL(0x1,TILE_XY(1,0)),
	MKCLRRAIL(0,TILE_XY(0,0)),
	MKCLRRAIL(0,TILE_XY(1,1)),
	MKCLRRAIL(0,TILE_XY(2,0)),
	MKEND
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

#define MKAIR(a,b) {0,a,b}

static const AiDefaultBlockData _airportdata_ai_0[] = {
	MKAIR(1, 0),
	{1},
};

static const AiDefaultBlockData _airportdata_ai_1[] = {
	MKAIR(0, 0),
	{1}
};

static const AiDefaultBlockData * const _airport_default_block_data[] = {
	_airportdata_ai_0, // city airport
	_airportdata_ai_1, // country airport
	NULL
};


