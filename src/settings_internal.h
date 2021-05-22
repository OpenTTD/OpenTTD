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
 * @see SettingDesc
 */
enum SettingDescType : byte {
	SDT_NUMX        = 0, ///< any number-type
	SDT_BOOLX       = 1, ///< a boolean number
	SDT_ONEOFMANY   = 2, ///< bitmasked number where only ONE bit may be set
	SDT_MANYOFMANY  = 3, ///< bitmasked number where MULTIPLE bits may be set
	SDT_INTLIST     = 4, ///< list of integers separated by a comma ','
	SDT_STDSTRING   = 6, ///< \c std::string
	SDT_NULL        = 7, ///< an old setting that has been removed but could still be in savegames
};

enum SettingGuiFlag : uint16 {
	/* 2 bytes allocated for a maximum of 16 flags. */
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
struct SettingDesc {
	SettingDesc(SaveLoad save, const char *name, const void *def, SettingDescType cmd, SettingGuiFlag flags,
			int32 min, uint32 max, int32 interval, const char *many, StringID str, StringID str_help,
			StringID str_val, OnChange *proc, OnConvert *proc_cnvt, SettingCategory cat, bool startup) :
		name(name), def(def), cmd(cmd), flags(flags), min(min), max(max), interval(interval), many(many), str(str),
		str_help(str_help), str_val(str_val), proc(proc), proc_cnvt(proc_cnvt), cat(cat), startup(startup), save(save) {}
	virtual ~SettingDesc() {}

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
	SaveLoad save;          ///< Internal structure (going to savegame, parts to config)

	bool IsEditable(bool do_command = false) const;
	SettingType GetType() const;
	bool IsIntSetting() const;
	const struct IntSettingDesc *AsIntSetting() const;

	/**
	 * Format the value of the setting associated with this object.
	 * @param buf The before of the buffer to format into.
	 * @param last The end of the buffer to format into.
	 * @param object The object the setting is in.
	 */
	virtual void FormatValue(char *buf, const char *last, const void *object) const = 0;
};

/** Integer type, including boolean, settings. Only these are shown in the settings UI. */
struct IntSettingDesc : SettingDesc {
	IntSettingDesc(SaveLoad save, const char *name, SettingGuiFlag flags, SettingDescType cmd, bool startup, int32 def,
			int32 min, uint32 max, int32 interval, StringID str, StringID str_help, StringID str_val,
			SettingCategory cat, OnChange *proc, const char *many = nullptr, OnConvert *many_cnvt = nullptr) :
		SettingDesc(save, name, (void*)(size_t)def, cmd, flags, min, max, interval, many, str, str_help, str_val,
			proc, many_cnvt, cat, startup) {}
	virtual ~IntSettingDesc() {}

	void ChangeValue(const void *object, int32 newvalue) const;
	void Write_ValidateSetting(const void *object, int32 value) const;

	void FormatValue(char *buf, const char *last, const void *object) const override;
};

/** String settings. */
struct StringSettingDesc : SettingDesc {
	StringSettingDesc(SaveLoad save, const char *name, SettingGuiFlag flags, SettingDescType cmd, bool startup, const char *def,
			uint32 max_length, OnChange proc) :
		SettingDesc(save, name, def, cmd, flags, 0, max_length, 0, nullptr, 0, 0, 0, proc, nullptr, SC_NONE, startup) {}
	virtual ~StringSettingDesc() {}

	void FormatValue(char *buf, const char *last, const void *object) const override;
	const std::string &Read(const void *object) const;
};

/** List/array settings. */
struct ListSettingDesc : SettingDesc {
	ListSettingDesc(SaveLoad save, const char *name, SettingGuiFlag flags, SettingDescType cmd, bool startup, const char *def) :
		SettingDesc(save, name, def, cmd, flags, 0, 0, 0, nullptr, 0, 0, 0, proc, nullptr, SC_NONE, startup) {}
	virtual ~ListSettingDesc() {}

	void FormatValue(char *buf, const char *last, const void *object) const override;
};

/** Placeholder for settings that have been removed, but might still linger in the savegame. */
struct NullSettingDesc : SettingDesc {
	NullSettingDesc(SaveLoad save) :
		SettingDesc(save, "", nullptr, SDT_NULL, SGF_NONE, 0, 0, 0, nullptr, 0, 0, 0, nullptr, nullptr, SC_NONE, false) {}
	virtual ~NullSettingDesc() {}

	void FormatValue(char *buf, const char *last, const void *object) const override { NOT_REACHED(); }
};

typedef std::initializer_list<std::unique_ptr<const SettingDesc>> SettingTable;

const SettingDesc *GetSettingFromName(const char *name);
bool SetSettingValue(const SettingDesc *sd, int32 value, bool force_newgame = false);
bool SetSettingValue(const SettingDesc *sd, const char *value, bool force_newgame = false);
uint GetSettingIndex(const SettingDesc *sd);

#endif /* SETTINGS_INTERNAL_H */
