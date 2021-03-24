/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file settings_internal.h Functions and types used internally for the settings configurations. */

#ifndef SETTINGS_INTERNAL_H
#define SETTINGS_INTERNAL_H

#include "saveload/saveload.h"

/**
 * Convention/Type of settings. This is then further specified if necessary
 * with the SLE_ (SLE_VAR/SLE_FILE) enums in saveload.h
 * @see VarTypes
 * @see SettingDescBase
 */
enum SettingDescType : byte {
	/* 4 bytes allocated a maximum of 16 types for GenericType */
	SDT_BEGIN       = 0,
	SDT_NUMX        = 0, ///< any number-type
	SDT_BOOLX       = 1, ///< a boolean number
	SDT_ONEOFMANY   = 2, ///< bitmasked number where only ONE bit may be set
	SDT_MANYOFMANY  = 3, ///< bitmasked number where MULTIPLE bits may be set
	SDT_INTLIST     = 4, ///< list of integers separated by a comma ','
	SDT_STRING      = 5, ///< string with a pre-allocated buffer
	SDT_STDSTRING   = 6, ///< \c std::string
	SDT_END,
	/* 9 more possible primitives */
};


enum SettingGuiFlag : uint16 {
	/* 1 byte allocated for a maximum of 8 flags
	 * Flags directing saving/loading of a variable */
	SGF_NONE = 0,
	SGF_0ISDISABLED   = 1 << 0, ///< a value of zero means the feature is disabled
	SGF_DISPLAY_ABS   = 1 << 1, ///< display absolute value of the setting
	SGF_MULTISTRING   = 1 << 2, ///< the value represents a limited number of string-options (internally integer)
	SGF_NETWORK_ONLY  = 1 << 3, ///< this setting only applies to network games
	SGF_CURRENCY      = 1 << 4, ///< the number represents money, so when reading value multiply by exchange rate
	SGF_NO_NETWORK    = 1 << 5, ///< this setting does not apply to network games; it may not be changed during the game
	SGF_NEWGAME_ONLY  = 1 << 6, ///< this setting cannot be changed in a game
	SGF_SCENEDIT_TOO  = 1 << 7, ///< this setting can be changed in the scenario editor (only makes sense when SGF_NEWGAME_ONLY is set)
	SGF_PER_COMPANY   = 1 << 8, ///< this setting can be different for each company (saved in company struct)
	SGF_SCENEDIT_ONLY = 1 << 9, ///< this setting can only be changed in the scenario editor
};
DECLARE_ENUM_AS_BIT_SET(SettingGuiFlag)

/**
 * A SettingCategory defines a grouping of the settings.
 * The group #SC_BASIC is intended for settings which also a novice player would like to change and is able to understand.
 * The group #SC_ADVANCED is intended for settings which an experienced player would like to use. This is the case for most settings.
 * Finally #SC_EXPERT settings only few people want to see in rare cases.
 * The grouping is meant to be inclusive, i.e. all settings in #SC_BASIC also will be included
 * in the set of settings in #SC_ADVANCED. The group #SC_EXPERT contains all settings.
 */
enum SettingCategory {
	SC_NONE = 0,

	/* Filters for the list */
	SC_BASIC_LIST      = 1 << 0,    ///< Settings displayed in the list of basic settings.
	SC_ADVANCED_LIST   = 1 << 1,    ///< Settings displayed in the list of advanced settings.
	SC_EXPERT_LIST     = 1 << 2,    ///< Settings displayed in the list of expert settings.

	/* Setting classification */
	SC_BASIC           = SC_BASIC_LIST | SC_ADVANCED_LIST | SC_EXPERT_LIST,  ///< Basic settings are part of all lists.
	SC_ADVANCED        = SC_ADVANCED_LIST | SC_EXPERT_LIST,                  ///< Advanced settings are part of advanced and expert list.
	SC_EXPERT          = SC_EXPERT_LIST,                                     ///< Expert settings can only be seen in the expert list.

	SC_END,
};

/**
 * Type of settings for filtering.
 */
enum SettingType {
	ST_GAME,      ///< Game setting.
	ST_COMPANY,   ///< Company setting.
	ST_CLIENT,    ///< Client setting.

	ST_ALL,       ///< Used in setting filter to match all types.
};

typedef bool OnChange(int32 var);           ///< callback prototype on data modification
typedef size_t OnConvert(const char *value); ///< callback prototype for conversion error

/** Properties of config file settings. */
struct SettingDescBase {
	const char *name;       ///< name of the setting. Used in configuration file and for console
	const void *def;        ///< default value given when none is present
	SettingDescType cmd;    ///< various flags for the variable
	SettingGuiFlag flags;   ///< handles how a setting would show up in the GUI (text/currency, etc.)
	int32 min;              ///< minimum values
	uint32 max;             ///< maximum values
	int32 interval;         ///< the interval to use between settings in the 'settings' window. If interval is '0' the interval is dynamically determined
	const char *many;       ///< ONE/MANY_OF_MANY: string of possible values for this type
	StringID str;           ///< (translated) string with descriptive text; gui and console
	StringID str_help;      ///< (Translated) string with help text; gui only.
	StringID str_val;       ///< (Translated) first string describing the value.
	OnChange *proc;         ///< callback procedure for when the value is changed
	OnConvert *proc_cnvt;   ///< callback procedure when loading value mechanism fails
	SettingCategory cat;    ///< assigned categories of the setting
	bool startup;           ///< setting has to be loaded directly at startup?
};

struct SettingDesc {
	SettingDescBase desc;   ///< Settings structure (going to configuration file)
	SaveLoad save;          ///< Internal structure (going to savegame, parts to config)

	bool IsEditable(bool do_command = false) const;
	SettingType GetType() const;
};

/* NOTE: The only difference between SettingDesc and SettingDescGlob is
 * that one uses global variables as a source and the other offsets
 * in a struct which are bound to a certain variable during runtime.
 * The only way to differentiate between these two is to check if an object
 * has been passed to the function or not. If not, then it is a global variable
 * and save->variable has its address, otherwise save->variable only holds the
 * offset in a certain struct */
typedef SettingDesc SettingDescGlobVarList;

const SettingDesc *GetSettingFromName(const char *name, uint *i);
bool SetSettingValue(uint index, int32 value, bool force_newgame = false);
bool SetSettingValue(uint index, const char *value, bool force_newgame = false);
void SetCompanySetting(uint index, int32 value);

#endif /* SETTINGS_INTERNAL_H */
