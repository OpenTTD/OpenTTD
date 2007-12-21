/* $Id$ */

/** @file strings_type.h Types related to strings. */

#ifndef STRINGS_TYPE_H
#define STRINGS_TYPE_H

typedef uint16 StringID;
static const StringID INVALID_STRING_ID = 0xFFFF;

enum {
	MAX_LANG = 64,
};

/** Information about a language */
struct Language {
	char *name; ///< The internal name of the language
	char *file; ///< The name of the language as it appears on disk
};

/** Used for dynamic language support */
struct DynamicLanguages {
	int num;                         ///< Number of languages
	int curr;                        ///< Currently selected language index
	char curr_file[MAX_PATH];        ///< Currently selected language file name without path (needed for saving the filename of the loaded language).
	StringID dropdown[MAX_LANG + 1]; ///< List of languages in the settings gui
	Language ent[MAX_LANG];          ///< Information about the languages
};

// special string constants
enum SpecialStrings {

	// special strings for town names. the town name is generated dynamically on request.
	SPECSTR_TOWNNAME_START     = 0x20C0,
	SPECSTR_TOWNNAME_ENGLISH   = SPECSTR_TOWNNAME_START,
	SPECSTR_TOWNNAME_FRENCH,
	SPECSTR_TOWNNAME_GERMAN,
	SPECSTR_TOWNNAME_AMERICAN,
	SPECSTR_TOWNNAME_LATIN,
	SPECSTR_TOWNNAME_SILLY,
	SPECSTR_TOWNNAME_SWEDISH,
	SPECSTR_TOWNNAME_DUTCH,
	SPECSTR_TOWNNAME_FINNISH,
	SPECSTR_TOWNNAME_POLISH,
	SPECSTR_TOWNNAME_SLOVAKISH,
	SPECSTR_TOWNNAME_NORWEGIAN,
	SPECSTR_TOWNNAME_HUNGARIAN,
	SPECSTR_TOWNNAME_AUSTRIAN,
	SPECSTR_TOWNNAME_ROMANIAN,
	SPECSTR_TOWNNAME_CZECH,
	SPECSTR_TOWNNAME_SWISS,
	SPECSTR_TOWNNAME_DANISH,
	SPECSTR_TOWNNAME_TURKISH,
	SPECSTR_TOWNNAME_ITALIAN,
	SPECSTR_TOWNNAME_CATALAN,
	SPECSTR_TOWNNAME_LAST      = SPECSTR_TOWNNAME_CATALAN,

	// special strings for player names on the form "TownName transport".
	SPECSTR_PLAYERNAME_START   = 0x70EA,
	SPECSTR_PLAYERNAME_ENGLISH = SPECSTR_PLAYERNAME_START,
	SPECSTR_PLAYERNAME_FRENCH,
	SPECSTR_PLAYERNAME_GERMAN,
	SPECSTR_PLAYERNAME_AMERICAN,
	SPECSTR_PLAYERNAME_LATIN,
	SPECSTR_PLAYERNAME_SILLY,
	SPECSTR_PLAYERNAME_LAST    = SPECSTR_PLAYERNAME_SILLY,

	SPECSTR_ANDCO_NAME         = 0x70E6,
	SPECSTR_PRESIDENT_NAME     = 0x70E7,
	SPECSTR_SONGNAME           = 0x70E8,

	// reserve MAX_LANG strings for the *.lng files
	SPECSTR_LANGUAGE_START     = 0x7100,
	SPECSTR_LANGUAGE_END       = SPECSTR_LANGUAGE_START + MAX_LANG - 1,

	// reserve 32 strings for various screen resolutions
	SPECSTR_RESOLUTION_START   = SPECSTR_LANGUAGE_END + 1,
	SPECSTR_RESOLUTION_END     = SPECSTR_RESOLUTION_START + 0x1F,

	// reserve 32 strings for screenshot formats
	SPECSTR_SCREENSHOT_START   = SPECSTR_RESOLUTION_END + 1,
	SPECSTR_SCREENSHOT_END     = SPECSTR_SCREENSHOT_START + 0x1F,

	// Used to implement SetDParamStr
	STR_SPEC_DYNSTRING         = 0xF800,
	STR_SPEC_USERSTRING        = 0xF808,
};

#endif /* STRINGS_TYPE_H */
