/* $Id$ */

/** @file animcursors.h
 * This file defines all the the animated cursors.
 * Animated cursors consist of the number of sprites that are
 * displayed in a round-robin manner. Each sprite also has a time
 * associated that indicates how many ticks the corresponding sprite
 * is to be displayed. Currently all this information is recorded in a single
 * array. @todo This should be converted into an array of structs on the long run
 * All these arrays end up in an array of pointers called _animcursors.
 */

/** Creates two array entries that define one
 *  status of the cursor.
 *  @param Sprite The Sprite to be displayed
 *  @param display_time The Number of ticks to display the sprite
 */
#define ANIM_CURSOR_LINE(Sprite, display_time) Sprite, display_time,

/** This indicates the termination of the cursor list
 */
#define ANIM_CURSOR_END() 0xFFFF

/** Animated cursor elements for demolishion
 */
static const CursorID _demolish_animcursor[] = {
	ANIM_CURSOR_LINE(0x2C0, 29)
	ANIM_CURSOR_LINE(0x2C1, 29)
	ANIM_CURSOR_LINE(0x2C2, 29)
	ANIM_CURSOR_LINE(0x2C3, 29)
	ANIM_CURSOR_END()
};

/** Animated cursor elements for lower land
 */
static const CursorID _lower_land_animcursor[] = {
	ANIM_CURSOR_LINE(0x2BB, 29)
	ANIM_CURSOR_LINE(0x2BC, 29)
	ANIM_CURSOR_LINE(0x2BD, 98)
	ANIM_CURSOR_END()
};

/** Animated cursor elements for raise land
 */
static const CursorID _raise_land_animcursor[] = {
	ANIM_CURSOR_LINE(0x2B8, 29)
	ANIM_CURSOR_LINE(0x2B9, 29)
	ANIM_CURSOR_LINE(0x2BA, 98)
	ANIM_CURSOR_END()
};

/** Animated cursor elements for the goto icon
 */
static const CursorID _pick_station_animcursor[] = {
	ANIM_CURSOR_LINE(0x2CC, 29)
	ANIM_CURSOR_LINE(0x2CD, 29)
	ANIM_CURSOR_LINE(0x2CE, 98)
	ANIM_CURSOR_END()
};

/** Animated cursor elements for the build signal icon
 */
static const CursorID _build_signals_animcursor[] = {
	ANIM_CURSOR_LINE(0x50C, 29)
	ANIM_CURSOR_LINE(0x50D, 29)
	ANIM_CURSOR_END()
};

/** This is an array of pointers to all the animated cursor
 *  definitions we have above. This is the only thing that is
 *  accessed directly from other files
 */
static const CursorID * const _animcursors[] = {
	_demolish_animcursor,
	_lower_land_animcursor,
	_raise_land_animcursor,
	_pick_station_animcursor,
	_build_signals_animcursor
};
