static const byte _shipdepot_display_seq_1[] = {
	ADD_WORD(0xFDD),

	0,15,0,16,1,0x14,
	ADD_WORD(0x8FE8),
	
	0x80 
};

static const byte _shipdepot_display_seq_2[] = {
	ADD_WORD(0xFDD),

	0,0,0,16,1,0x14, ADD_WORD(0xFEA),
	0,15,0,16,1,0x14, ADD_WORD(0x8FE6),

	0x80
};

static const byte _shipdepot_display_seq_3[] = {
	ADD_WORD(0xFDD),

	15,0,0,1,0x10,0x14,ADD_WORD(0x8FE9),

	0x80
};

static const byte _shipdepot_display_seq_4[] = {
	ADD_WORD(0xFDD),

	0,0,0,1,16,0x14, ADD_WORD(0xFEB),
	15,0,0,1,16,0x14, ADD_WORD(0x8FE7),

	0x80
};

static const byte * const _shipdepot_display_seq[] = {
	_shipdepot_display_seq_1,
	_shipdepot_display_seq_2,
	_shipdepot_display_seq_3,
	_shipdepot_display_seq_4,
};

static const byte _shiplift_display_seq_0[] = {
	ADD_WORD(SPR_CANALS_BASE + 6),
	0, 0, 0, 0x10, 1, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 0 + 1),
	0, 0xF, 0, 0x10, 1, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 4 + 1),
	0x80, 0
};

static const byte _shiplift_display_seq_1[] = {
	ADD_WORD(SPR_CANALS_BASE + 5),
	0, 0, 0, 1, 0x10, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 0),
	0xF, 0, 0, 1, 0x10, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 4),
	0x80, 0
};

static const byte _shiplift_display_seq_2[] = {
	ADD_WORD(SPR_CANALS_BASE + 7),
	0, 0, 0, 0x10, 1, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 0 + 2),
	0, 0xF, 0, 0x10, 1, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 4 + 2),
	0x80, 0
};

static const byte _shiplift_display_seq_3[] = {
	ADD_WORD(SPR_CANALS_BASE + 8),
	0, 0, 0, 1, 0x10, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 0 + 3),
	0xF, 0, 0, 1, 0x10, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 4 + 3),
	0x80, 0
};

static const byte _shiplift_display_seq_0b[] = {
	ADD_WORD(0xFDD),
	0, 0, 0, 0x10, 1, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 8 + 1),
	0, 0xF, 0, 0x10, 1, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 12 + 1),
	0x80, 0
};

static const byte _shiplift_display_seq_1b[] = {
	ADD_WORD(0xFDD),
	0, 0, 0, 0x1, 0x10, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 8),
	0xF, 0, 0, 0x1, 0x10, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 12),
	0x80, 0
};

static const byte _shiplift_display_seq_2b[] = {
	ADD_WORD(0xFDD),
	0, 0, 0, 0x10, 1, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 8 + 2),
	0, 0xF, 0, 0x10, 1, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 12 + 2),
	0x80, 0
};

static const byte _shiplift_display_seq_3b[] = {
	ADD_WORD(0xFDD),
	0, 0, 0, 1, 0x10, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 8 + 3),
	0xF, 0, 0, 1, 0x10, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 12 + 3),
	0x80, 0
};

static const byte _shiplift_display_seq_0t[] = {
	ADD_WORD(0xFDD),
	0, 0, 0, 0x10, 1, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 16 + 1),
	0, 0xF, 0, 0x10, 1, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 20 + 1),
	0x80, 8
};

static const byte _shiplift_display_seq_1t[] = {
	ADD_WORD(0xFDD),
	0, 0, 0, 0x1, 0x10, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 16),
	0xF, 0, 0, 0x1, 0x10, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 20),
	0x80, 8
};

static const byte _shiplift_display_seq_2t[] = {
	ADD_WORD(0xFDD),
	0, 0, 0, 0x10, 1, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 16 + 2),
	0, 0xF, 0, 0x10, 1, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 20 + 2),
	0x80, 8
};

static const byte _shiplift_display_seq_3t[] = {
	ADD_WORD(0xFDD),
	0, 0, 0, 1, 0x10, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 16 + 3),
	0xF, 0, 0, 1, 0x10, 0x14, ADD_WORD(SPR_CANALS_BASE + 9 + 20 + 3),
	0x80, 8	
};

static const byte * const _shiplift_display_seq[] = {
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


static const SpriteID _water_shore_sprites[15] = {
	0, 0xFDF, 0xFE0, 0xFE4, 0xFDE, 0, 0xFE2, 0, 0xFE1, 0xFE5, 0, 0, 0xFE3, 0, 0
};
