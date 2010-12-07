/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file animcursors.h
 * This file defines all the the animated cursors.
 * Animated cursors consist of the number of sprites that are
 * displayed in a round-robin manner. Each sprite also has a time
 * associated that indicates how many ticks the corresponding sprite
 * is to be displayed.
 */

/**
 * Creates two array entries that define one
 *  status of the cursor.
 *  @param Sprite The Sprite to be displayed
 *  @param display_time The Number of ticks to display the sprite
 */
#define ANIM_CURSOR_LINE(Sprite, display_time) { Sprite, display_time },

/**
 * This indicates the termination of the cursor list
 */
#define ANIM_CURSOR_END() ANIM_CURSOR_LINE(AnimCursor::LAST, 0)

/**
 * Animated cursor elements for demolishion
 */
static const AnimCursor _demolish_animcursor[] = {
	ANIM_CURSOR_LINE(SPR_CURSOR_DEMOLISH_FIRST, 8)
	ANIM_CURSOR_LINE(SPR_CURSOR_DEMOLISH_1,     8)
	ANIM_CURSOR_LINE(SPR_CURSOR_DEMOLISH_2,     8)
	ANIM_CURSOR_LINE(SPR_CURSOR_DEMOLISH_LAST,  8)
	ANIM_CURSOR_END()
};

/**
 * Animated cursor elements for lower land
 */
static const AnimCursor _lower_land_animcursor[] = {
	ANIM_CURSOR_LINE(SPR_CURSOR_LOWERLAND_FIRST, 10)
	ANIM_CURSOR_LINE(SPR_CURSOR_LOWERLAND_1,     10)
	ANIM_CURSOR_LINE(SPR_CURSOR_LOWERLAND_LAST,  29)
	ANIM_CURSOR_END()
};

/**
 * Animated cursor elements for raise land
 */
static const AnimCursor _raise_land_animcursor[] = {
	ANIM_CURSOR_LINE(SPR_CURSOR_RAISELAND_FIRST, 10)
	ANIM_CURSOR_LINE(SPR_CURSOR_RAISELAND_1,     10)
	ANIM_CURSOR_LINE(SPR_CURSOR_RAISELAND_LAST,  29)
	ANIM_CURSOR_END()
};

/**
 * Animated cursor elements for the goto icon
 */
static const AnimCursor _order_goto_animcursor[] = {
	ANIM_CURSOR_LINE(SPR_CURSOR_PICKSTATION_FIRST, 10)
	ANIM_CURSOR_LINE(SPR_CURSOR_PICKSTATION_1,     10)
	ANIM_CURSOR_LINE(SPR_CURSOR_PICKSTATION_LAST,  29)
	ANIM_CURSOR_END()
};

/**
 * Animated cursor elements for the build signal icon
 */
static const AnimCursor _build_signals_animcursor[] = {
	ANIM_CURSOR_LINE(SPR_CURSOR_BUILDSIGNALS_FIRST, 20)
	ANIM_CURSOR_LINE(SPR_CURSOR_BUILDSIGNALS_LAST,  20)
	ANIM_CURSOR_END()
};

/**
 * This is an array of pointers to all the animated cursor
 *  definitions we have above. This is the only thing that is
 *  accessed directly from other files
 */
static const AnimCursor * const _animcursors[] = {
	_demolish_animcursor,
	_lower_land_animcursor,
	_raise_land_animcursor,
	_order_goto_animcursor,
	_build_signals_animcursor
};
