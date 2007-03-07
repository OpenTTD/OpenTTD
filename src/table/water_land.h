/* $Id$ */

struct WaterDrawTileStruct {
	byte delta_x;
	byte delta_y;
	byte delta_z;
	byte width;
	byte height;
	byte unk;
	SpriteID image;
};

#define BEGIN(image) { 0, 0, 0, 0, 0, 0, image }
#define END(y) { 0x80, y, 0, 0, 0, 0, 0 }

static const WaterDrawTileStruct _shipdepot_display_seq_1[] = {
	BEGIN(0xFDD),
	{ 0, 15, 0, 16, 1, 0x14, 0xFE8 | (1 << PALETTE_MODIFIER_COLOR) },
	END(0)
};

static const WaterDrawTileStruct _shipdepot_display_seq_2[] = {
	BEGIN(0xFDD),
	{ 0,  0, 0, 16, 1, 0x14, 0xFEA },
	{ 0, 15, 0, 16, 1, 0x14, 0xFE6 | (1 << PALETTE_MODIFIER_COLOR) },
	END(0)
};

static const WaterDrawTileStruct _shipdepot_display_seq_3[] = {
	BEGIN(0xFDD),
	{ 15, 0, 0, 1, 0x10, 0x14, 0xFE9 | (1 << PALETTE_MODIFIER_COLOR) },
	END(0)
};

static const WaterDrawTileStruct _shipdepot_display_seq_4[] = {
	BEGIN(0xFDD),
	{  0, 0, 0, 1, 16, 0x14, 0xFEB },
	{ 15, 0, 0, 1, 16, 0x14, 0xFE7 | (1 << PALETTE_MODIFIER_COLOR) },
	END(0)
};

static const WaterDrawTileStruct* const _shipdepot_display_seq[] = {
	_shipdepot_display_seq_1,
	_shipdepot_display_seq_2,
	_shipdepot_display_seq_3,
	_shipdepot_display_seq_4,
};

static const WaterDrawTileStruct _shiplift_display_seq_0[] = {
	BEGIN(SPR_CANALS_BASE + 6),
	{ 0,   0, 0, 0x10, 1, 0x14, SPR_CANALS_BASE + 9 + 0 + 1 },
	{ 0, 0xF, 0, 0x10, 1, 0x14, SPR_CANALS_BASE + 9 + 4 + 1 },
	END(0)
};

static const WaterDrawTileStruct _shiplift_display_seq_1[] = {
	BEGIN(SPR_CANALS_BASE + 5),
	{   0, 0, 0, 1, 0x10, 0x14, SPR_CANALS_BASE + 9 + 0 },
	{ 0xF, 0, 0, 1, 0x10, 0x14, SPR_CANALS_BASE + 9 + 4 },
	END(0)
};

static const WaterDrawTileStruct _shiplift_display_seq_2[] = {
	BEGIN(SPR_CANALS_BASE + 7),
	{ 0,   0, 0, 0x10, 1, 0x14, SPR_CANALS_BASE + 9 + 0 + 2 },
	{ 0, 0xF, 0, 0x10, 1, 0x14, SPR_CANALS_BASE + 9 + 4 + 2 },
	END(0)
};

static const WaterDrawTileStruct _shiplift_display_seq_3[] = {
	BEGIN(SPR_CANALS_BASE + 8),
	{   0, 0, 0, 1, 0x10, 0x14, SPR_CANALS_BASE + 9 + 0 + 3 },
	{ 0xF, 0, 0, 1, 0x10, 0x14, SPR_CANALS_BASE + 9 + 4 + 3 },
	END(0)
};

static const WaterDrawTileStruct _shiplift_display_seq_0b[] = {
	BEGIN(0xFDD),
	{ 0,   0, 0, 0x10, 1, 0x14, SPR_CANALS_BASE + 9 + 8 + 1 },
	{ 0, 0xF, 0, 0x10, 1, 0x14, SPR_CANALS_BASE + 9 + 12 + 1 },
	END(0)
};

static const WaterDrawTileStruct _shiplift_display_seq_1b[] = {
	BEGIN(0xFDD),
	{   0, 0, 0, 0x1, 0x10, 0x14, SPR_CANALS_BASE + 9 + 8 },
	{ 0xF, 0, 0, 0x1, 0x10, 0x14, SPR_CANALS_BASE + 9 + 12 },
	END(0)
};

static const WaterDrawTileStruct _shiplift_display_seq_2b[] = {
	BEGIN(0xFDD),
	{ 0,   0, 0, 0x10, 1, 0x14, SPR_CANALS_BASE + 9 + 8 + 2 },
	{ 0, 0xF, 0, 0x10, 1, 0x14, SPR_CANALS_BASE + 9 + 12 + 2 },
	END(0)
};

static const WaterDrawTileStruct _shiplift_display_seq_3b[] = {
	BEGIN(0xFDD),
	{   0, 0, 0, 1, 0x10, 0x14, SPR_CANALS_BASE + 9 + 8 + 3 },
	{ 0xF, 0, 0, 1, 0x10, 0x14, SPR_CANALS_BASE + 9 + 12 + 3 },
	END(0)
};

static const WaterDrawTileStruct _shiplift_display_seq_0t[] = {
	BEGIN(0xFDD),
	{ 0,   0, 0, 0x10, 1, 0x14, SPR_CANALS_BASE + 9 + 16 + 1 },
	{ 0, 0xF, 0, 0x10, 1, 0x14, SPR_CANALS_BASE + 9 + 20 + 1 },
	END(8)
};

static const WaterDrawTileStruct _shiplift_display_seq_1t[] = {
	BEGIN(0xFDD),
	{   0, 0, 0, 0x1, 0x10, 0x14, SPR_CANALS_BASE + 9 + 16 },
	{ 0xF, 0, 0, 0x1, 0x10, 0x14, SPR_CANALS_BASE + 9 + 20 },
	END(8)
};

static const WaterDrawTileStruct _shiplift_display_seq_2t[] = {
	BEGIN(0xFDD),
	{ 0,   0, 0, 0x10, 1, 0x14, SPR_CANALS_BASE + 9 + 16 + 2 },
	{ 0, 0xF, 0, 0x10, 1, 0x14, SPR_CANALS_BASE + 9 + 20 + 2 },
	END(8)
};

static const WaterDrawTileStruct _shiplift_display_seq_3t[] = {
	BEGIN(0xFDD),
	{   0, 0, 0, 1, 0x10, 0x14, SPR_CANALS_BASE + 9 + 16 + 3 },
	{ 0xF, 0, 0, 1, 0x10, 0x14, SPR_CANALS_BASE + 9 + 20 + 3 },
	END(8)
};

static const WaterDrawTileStruct* const _shiplift_display_seq[] = {
	_shiplift_display_seq_0,
	_shiplift_display_seq_1,
	_shiplift_display_seq_2,
	_shiplift_display_seq_3,

	_shiplift_display_seq_0b,
	_shiplift_display_seq_1b,
	_shiplift_display_seq_2b,
	_shiplift_display_seq_3b,

	_shiplift_display_seq_0t,
	_shiplift_display_seq_1t,
	_shiplift_display_seq_2t,
	_shiplift_display_seq_3t,
};

#undef BEGIN
#undef END
