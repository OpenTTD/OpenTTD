/* $Id$ */

/** @file transparency.h */

#ifndef TRANSPARENCY_H
#define TRANSPARENCY_H

#include "gfx_func.h"

/**
 * Transparency option bits: which position in _transparency_opt stands for which transparency.
 * If you change the order, change the order of the ShowTransparencyToolbar() stuff in transparency_gui.cpp too.
 * If you add or remove an option don't forget to change the transparency 'hot keys' in main_gui.cpp.
 * If you add an option and have more then 8, change the typedef TransparencyOptionBits and
 * the save stuff (e.g. SLE_UINT8 to SLE_UINT16) in settings.cpp .
 */
enum TransparencyOption {
	TO_SIGNS = 0,  ///< signs
	TO_TREES,      ///< trees
	TO_HOUSES,     ///< town buildings
	TO_INDUSTRIES, ///< industries
	TO_BUILDINGS,  ///< player buildings - depots, stations, HQ, ...
	TO_BRIDGES,    ///< bridges
	TO_STRUCTURES, ///< unmovable structures
	TO_CATENARY,   ///< catenary
	TO_LOADING,    ///< loading indicators
	TO_END,
};

typedef uint TransparencyOptionBits; ///< transparency option bits
extern TransparencyOptionBits _transparency_opt;
extern TransparencyOptionBits _transparency_lock;

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
 * Toggle the transparency option bit
 *
 * @param to the transparency option to be toggled
 */
static inline void ToggleTransparency(TransparencyOption to)
{
	ToggleBit(_transparency_opt, to);
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
		_transparency_opt |= ~_transparency_lock;
	} else {
		/* clear all non-locked options */
		_transparency_opt &= _transparency_lock;
	}

	MarkWholeScreenDirty();
}

#endif /* TRANSPARENCY_H */
