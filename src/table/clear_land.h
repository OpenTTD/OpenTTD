/* $Id$ */

/** @file clear_land.h Tables with sprites for clear land and fences. */

static const SpriteID _landscape_clear_sprites[8] = {
	0xFA0,
	0xFB3,
	0xFB4,
	0xFB5,
	0xFB6,
	0xFA0,
	0xFB3,
	0xFB4,
};

static const byte _fence_mod_by_tileh[32] = {
	0, 2, 4, 0, 0, 2, 4, 0,
	0, 2, 4, 0, 0, 2, 4, 0,
	0, 2, 4, 0, 0, 2, 4, 4,
	0, 2, 4, 2, 0, 2, 4, 0,
};

static const byte _fence_mod_by_tileh_2[32] = {
	1, 1, 5, 5, 3, 3, 1, 1,
	1, 1, 5, 5, 3, 3, 1, 1,
	1, 1, 5, 5, 3, 3, 1, 5,
	1, 1, 5, 5, 3, 3, 3, 1,
};


static const SpriteID _clear_land_fence_sprites_1[7] = {
	0xFFA,
	0x1000,
	0x1006,
	0x100C,
	0x1012,
	0x1018,
};

static const SpriteID _clear_land_sprites_1[16] = {
	0x101E,
	0x1031,
	0x1044,
	0x1057,
	0x106A,
	0x107D,
	0x1090,
	0x10A3,
	0x10B6,
};

static const SpriteID _clear_land_sprites_2[8] = {
	0x118D,
	0x11A0,
	0x11B3,
	0x11C6,
};

static const SpriteID _clear_land_sprites_3[8] = {
	0x118D,
	0x11A0,
	0x11B3,
	0x11C6,
};
