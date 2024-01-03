/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file transparency.h Functions related to transparency. */

#ifndef TRANSPARENCY_H
#define TRANSPARENCY_H

#include "gfx_func.h"
#include "openttd.h"
#include "core/bitmath_func.hpp"

/**
 * Transparency option bits: which position in _transparency_opt stands for which transparency.
 * If you change the order, change the order of the ShowTransparencyToolbar() stuff in transparency_gui.cpp too.
 * If you add or remove an option don't forget to change the transparency 'hot keys' in main_gui.cpp.
 */
enum TransparencyOption {
	TO_SIGNS = 0,  ///< signs
	TO_TREES,      ///< trees
	TO_HOUSES,     ///< town buildings
	TO_INDUSTRIES, ///< industries
	TO_BUILDINGS,  ///< company buildings - depots, stations, HQ, ...
	TO_BRIDGES,    ///< bridges
	TO_STRUCTURES, ///< other objects such as transmitters and lighthouses
	TO_CATENARY,   ///< catenary
	TO_TEXT,       ///< loading and cost/income text
	TO_END,
	TO_INVALID,    ///< Invalid transparency option
};

typedef uint TransparencyOptionBits; ///< transparency option bits
extern TransparencyOptionBits _transparency_opt;
extern TransparencyOptionBits _transparency_lock;
extern TransparencyOptionBits _invisibility_opt;
extern byte _display_opt;

/**
 * Check if the transparency option bit is set
 * and if we aren't in the game menu (there's never transparency)
 *
 * @param to the structure which transparency option is ask for
 */
static inline bool IsTransparencySet(TransparencyOption to)
{
	return (HasBit(_transparency_opt, to) && _game_mode != GM_MENU);
}

/**
 * Check if the invisibility option bit is set
 * and if we aren't in the game menu (there's never transparency)
 *
 * @param to the structure which invisibility option is ask for
 */
static inline bool IsInvisibilitySet(TransparencyOption to)
{
	return (HasBit(_transparency_opt & _invisibility_opt, to) && _game_mode != GM_MENU);
}

/**
 * Toggle the transparency option bit
 *
 * @param to the transparency option to be toggled
 */
static inline void ToggleTransparency(TransparencyOption to)
{
	ToggleBit(_transparency_opt, to);
}

/**
 * Toggle the invisibility option bit
 *
 * @param to the structure which invisibility option is toggle
 */
static inline void ToggleInvisibility(TransparencyOption to)
{
	ToggleBit(_invisibility_opt, to);
}

/**
 * Toggles between invisible and solid state.
 * If object is transparent, then it is made invisible.
 * Used by the keyboard shortcuts.
 *
 * @param to the object type which invisibility option to toggle
 */
static inline void ToggleInvisibilityWithTransparency(TransparencyOption to)
{
	if (IsInvisibilitySet(to)) {
		ClrBit(_invisibility_opt, to);
		ClrBit(_transparency_opt, to);
	} else {
		SetBit(_invisibility_opt, to);
		SetBit(_transparency_opt, to);
	}
}

/**
 * Toggle the transparency lock bit
 *
 * @param to the transparency option to be locked or unlocked
 */
static inline void ToggleTransparencyLock(TransparencyOption to)
{
	ToggleBit(_transparency_lock, to);
}

/** Set or clear all non-locked transparency options */
static inline void ResetRestoreAllTransparency()
{
	/* if none of the non-locked options are set */
	if ((_transparency_opt & ~_transparency_lock) == 0) {
		/* set all non-locked options */
		_transparency_opt |= GB(~_transparency_lock, 0, TO_END);
	} else {
		/* clear all non-locked options */
		_transparency_opt &= _transparency_lock;
	}

	MarkWholeScreenDirty();
}

#endif /* TRANSPARENCY_H */
