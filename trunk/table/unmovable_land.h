#define TILE_SEQ_BEGIN(x) ADD_WORD(x),
#define TILE_SEQ_LINE(a,b,c,d,e,f,g) a,b,c,d,e,f,ADD_WORD(g),
#define TILE_SEQ_END() 0x80

static const DrawTileUnmovableStruct _draw_tile_unmovable_data[] = {
	{0xA29, 7,7, 2,2, 70},
	{0xA2A, 4,4, 7,7, 61},
};

static const byte _unmovable_display_datas_0[] = {
	TILE_SEQ_BEGIN(0x8A2B)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_1[] = {
	TILE_SEQ_BEGIN(0x8A2C)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_2[] = {
	TILE_SEQ_BEGIN(0x8A2D)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_3[] = {
	TILE_SEQ_BEGIN(0x8A2E)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_4[] = {
	TILE_SEQ_BEGIN(0x8A2F)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_5[] = {
	TILE_SEQ_BEGIN(0x8A30)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_6[] = {
	TILE_SEQ_BEGIN(0x8A31)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_7[] = {
	TILE_SEQ_BEGIN(0x8A32)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_8[] = {
	TILE_SEQ_BEGIN(0x8A33)
	TILE_SEQ_LINE(  0,  0,  0, 16, 16, 20, 0x8A34)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_9[] = {
	TILE_SEQ_BEGIN(0x8A35)
	TILE_SEQ_LINE(  0,  0,  0, 16, 16, 20, 0x8A36)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_10[] = {
	TILE_SEQ_BEGIN(0x8A37)
	TILE_SEQ_LINE(  0,  0,  0, 16, 16, 20, 0x8A38)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_11[] = {
	TILE_SEQ_BEGIN(0x8A39)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_12[] = {
	TILE_SEQ_BEGIN(0x8A3A)
	TILE_SEQ_LINE(  0,  0,  0, 16, 16, 50, 0x8A3B)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_13[] = {
	TILE_SEQ_BEGIN(0x8A3C)
	TILE_SEQ_LINE(  0,  0,  0, 16, 16, 50, 0x8A3D)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_14[] = {
	TILE_SEQ_BEGIN(0x8A3E)
	TILE_SEQ_LINE(  0,  0,  0, 16, 16, 50, 0x8A3F)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_15[] = {
	TILE_SEQ_BEGIN(0x8A40)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_16[] = {
	TILE_SEQ_BEGIN(0x8A41)
	TILE_SEQ_LINE(  0,  0,  0, 16, 16, 60, 0x8A42)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_17[] = {
	TILE_SEQ_BEGIN(0x8A43)
	TILE_SEQ_LINE(  0,  0,  0, 16, 16, 60, 0x8A44)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_18[] = {
	TILE_SEQ_BEGIN(0x8A45)
	TILE_SEQ_LINE(  0,  0,  0, 16, 16, 60, 0x8A46)
	TILE_SEQ_END()
};

static const byte _unmovable_display_datas_19[] = {
	TILE_SEQ_BEGIN(0x8A47)
	TILE_SEQ_END()
};

static const byte * const _unmovable_display_datas[] = {
	_unmovable_display_datas_0,
	_unmovable_display_datas_1,
	_unmovable_display_datas_2,
	_unmovable_display_datas_3,
	_unmovable_display_datas_4,
	_unmovable_display_datas_5,
	_unmovable_display_datas_6,
	_unmovable_display_datas_7,
	_unmovable_display_datas_8,
	_unmovable_display_datas_9,
	_unmovable_display_datas_10,
	_unmovable_display_datas_11,
	_unmovable_display_datas_12,
	_unmovable_display_datas_13,
	_unmovable_display_datas_14,
	_unmovable_display_datas_15,
	_unmovable_display_datas_16,
	_unmovable_display_datas_17,
	_unmovable_display_datas_18,
	_unmovable_display_datas_19,
};

