/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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
 * Animated cursor elements for demolition
 */
static constexpr AnimCursor _demolish_animcursor[] = {
	{SPR_CURSOR_DEMOLISH_FIRST, 8},
	{SPR_CURSOR_DEMOLISH_1,     8},
	{SPR_CURSOR_DEMOLISH_2,     8},
	{SPR_CURSOR_DEMOLISH_LAST,  8},
};

/**
 * Animated cursor elements for lower land
 */
static constexpr AnimCursor _lower_land_animcursor[] = {
	{SPR_CURSOR_LOWERLAND_FIRST, 10},
	{SPR_CURSOR_LOWERLAND_1,     10},
	{SPR_CURSOR_LOWERLAND_LAST,  29},
};

/**
 * Animated cursor elements for raise land
 */
static constexpr AnimCursor _raise_land_animcursor[] = {
	{SPR_CURSOR_RAISELAND_FIRST, 10},
	{SPR_CURSOR_RAISELAND_1,     10},
	{SPR_CURSOR_RAISELAND_LAST,  29},
};

/**
 * Animated cursor elements for the goto icon
 */
static constexpr AnimCursor _order_goto_animcursor[] = {
	{SPR_CURSOR_PICKSTATION_FIRST, 10},
	{SPR_CURSOR_PICKSTATION_1,     10},
	{SPR_CURSOR_PICKSTATION_LAST,  29},
};

/**
 * Animated cursor elements for the build signal icon
 */
static constexpr AnimCursor _build_signals_animcursor[] = {
	{SPR_CURSOR_BUILDSIGNALS_FIRST, 20},
	{SPR_CURSOR_BUILDSIGNALS_LAST,  20},
};

/**
 * This is an array of pointers to all the animated cursor
 *  definitions we have above. This is the only thing that is
 *  accessed directly from other files
 */
static constexpr std::span<const AnimCursor> _animcursors[] = {
	_demolish_animcursor,
	_lower_land_animcursor,
	_raise_land_animcursor,
	_order_goto_animcursor,
	_build_signals_animcursor
};
