/* $Id$ */

/** @file transparency.h */

#ifndef TRANSPARENCY_H
#define TRANSPARENCY_H

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
	TO_LOADING,    ///< loading indicators
	TO_END,
};

typedef byte TransparencyOptionBits; ///< transparency option bits
extern TransparencyOptionBits _transparency_opt;

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
 * @param to the structure which transparency option is toggle
 */
static inline void ToggleTransparency(TransparencyOption to)
{
	TOGGLEBIT(_transparency_opt, to);
}

/** Toggle all transparency options (except signs) or restore the stored transparencies */
static inline void ResetRestoreAllTransparency()
{
	/* backup of the original transparencies or if all transparencies false toggle them to true */
	static TransparencyOptionBits trans_opt = ~0;

	if (_transparency_opt == 0) {
		/* no structure is transparent, so restore the old transparency if present otherwise set all true */
		_transparency_opt = trans_opt;
	} else {
		/* any structure is transparent, so store current transparency settings and reset it */
		trans_opt = _transparency_opt;
		_transparency_opt = 0;
	}

	MarkWholeScreenDirty();
}

#endif /* TRANSPARENCY_H */
