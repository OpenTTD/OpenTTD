/* $Id$ */

#define ANIM_CURSOR_LINE(a,b) a,b,
#define ANIM_CURSOR_END() 0xFFFF

static const CursorID _demolish_animcursor[] = {
	ANIM_CURSOR_LINE(0x2C0, 29)
	ANIM_CURSOR_LINE(0x2C1, 29)
	ANIM_CURSOR_LINE(0x2C2, 29)
	ANIM_CURSOR_LINE(0x2C3, 29)
	ANIM_CURSOR_END()
};

static const CursorID _lower_land_animcursor[] = {
	ANIM_CURSOR_LINE(0x2BB, 29)
	ANIM_CURSOR_LINE(0x2BC, 29)
	ANIM_CURSOR_LINE(0x2BD, 98)
	ANIM_CURSOR_END()
};

static const CursorID _raise_land_animcursor[] = {
	ANIM_CURSOR_LINE(0x2B8, 29)
	ANIM_CURSOR_LINE(0x2B9, 29)
	ANIM_CURSOR_LINE(0x2BA, 98)
	ANIM_CURSOR_END()
};

static const CursorID _pick_station_animcursor[] = {
	ANIM_CURSOR_LINE(0x2CC, 29)
	ANIM_CURSOR_LINE(0x2CD, 29)
	ANIM_CURSOR_LINE(0x2CE, 98)
	ANIM_CURSOR_END()
};

static const CursorID _build_signals_animcursor[] = {
	ANIM_CURSOR_LINE(0x50C, 148)
	ANIM_CURSOR_LINE(0x50D, 148)
	ANIM_CURSOR_END()
};

static const CursorID * const _animcursors[] = {
	_demolish_animcursor,
	_lower_land_animcursor,
	_raise_land_animcursor,
	_pick_station_animcursor,
	_build_signals_animcursor
};
