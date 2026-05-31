/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file transparency.h Functions related to transparency. */

#ifndef TRANSPARENCY_H
#define TRANSPARENCY_H

#include "gfx_func.h"
#include "openttd.h"
#include "core/bitmath_func.hpp"
#include "station_type.h"

/**
 * Transparency option bits: which position in _transparency_opt stands for which transparency.
 * If you change the order, change the order of the ShowTransparencyToolbar() stuff in transparency_gui.cpp too.
 * If you add or remove an option don't forget to change the transparency 'hot keys' in main_gui.cpp.
 */
enum class TransparencyOption : uint8_t {
	Signs = 0, ///< signs
	Trees = 1, ///< trees
	Houses = 2, ///< town buildings
	Industries = 3, ///< industries
	Buildings = 4, ///< company buildings - depots, stations, HQ, ...
	Bridges = 5, ///< bridges
	Structures = 6, ///< other objects such as transmitters and lighthouses
	Catenary = 7, ///< catenary
	Text = 8, ///< loading and cost/income text
	Invalid, ///< Invalid transparency option
};

/** Bitset of \c TransparencyOption elements. */
using TransparencyOptions = EnumBitSet<TransparencyOption, uint32_t>;

extern TransparencyOptions _transparency_opt;
extern TransparencyOptions _transparency_lock;
extern TransparencyOptions _invisibility_opt;
extern DisplayOptions _display_opt;
extern StationFacilities _facility_display_opt;

/**
 * Check if the transparency option bit is set
 * and if we aren't in the game menu (there's never transparency)
 *
 * @param to the structure which transparency option is ask for
 * @return \c true if transparency is set for the option, and not in the main menu.
 */
inline bool IsTransparencySet(TransparencyOption to)
{
	return _transparency_opt.Test(to) && _game_mode != GameMode::Menu;
}

/**
 * Check if the invisibility option bit is set
 * and if we aren't in the game menu (there's never transparency)
 *
 * @param to the structure which invisibility option is ask for
 * @return \c true if invisibility is set for the option, and not in the main menu.
 */
inline bool IsInvisibilitySet(TransparencyOption to)
{
	return IsTransparencySet(to) && _invisibility_opt.Test(to) && _game_mode != GameMode::Menu;
}

/**
 * Toggle the transparency option bit
 *
 * @param to the transparency option to be toggled
 */
inline void ToggleTransparency(TransparencyOption to)
{
	_transparency_opt.Flip(to);
}

/**
 * Toggle the invisibility option bit
 *
 * @param to the structure which invisibility option is toggle
 */
inline void ToggleInvisibility(TransparencyOption to)
{
	_invisibility_opt.Flip(to);
}

/**
 * Toggles between invisible and solid state.
 * If object is transparent, then it is made invisible.
 * Used by the keyboard shortcuts.
 *
 * @param to the object type which invisibility option to toggle
 */
inline void ToggleInvisibilityWithTransparency(TransparencyOption to)
{
	if (IsInvisibilitySet(to)) {
		_invisibility_opt.Reset(to);
		_transparency_opt.Reset(to);
	} else {
		_invisibility_opt.Set(to);
		_transparency_opt.Set(to);
	}
}

/**
 * Toggle the transparency lock bit
 *
 * @param to the transparency option to be locked or unlocked
 */
inline void ToggleTransparencyLock(TransparencyOption to)
{
	_transparency_lock.Flip(to);
}

/** Set or clear all non-locked transparency options */
inline void ResetRestoreAllTransparency()
{
	TransparencyOptions unlocked = _transparency_lock;
	unlocked.Flip();

	/* if none of the non-locked options are set */
	if (!_transparency_opt.Any(unlocked)) {
		/* set all non-locked options */
		_transparency_opt.Set(unlocked);
	} else {
		/* clear all non-locked options */
		_transparency_opt.Reset(unlocked);
	}

	MarkWholeScreenDirty();
}

#endif /* TRANSPARENCY_H */
