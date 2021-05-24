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

struct IniItem;
typedef bool OnChange(int32 var);           ///< callback prototype on data modification
typedef size_t OnConvert(const char *value); ///< callback prototype for conversion error

/** Properties of config file settings. */
struct SettingDesc {
	SettingDesc(SaveLoad save, const char *name, SettingGuiFlag flags, bool startup) :
		name(name), flags(flags), startup(startup), save(save) {}
	virtual ~SettingDesc() {}

	const char *name;       ///< name of the setting. Used in configuration file and for console
	SettingGuiFlag flags;   ///< handles how a setting would show up in the GUI (text/currency, etc.)
	bool startup;           ///< setting has to be loaded directly at startup?
	SaveLoad save;          ///< Internal structure (going to savegame, parts to config)

	bool IsEditable(bool do_command = false) const;
	SettingType GetType() const;

	/**
	 * Check whether this setting is an integer type setting.
	 * @return True when the underlying type is an integer.
	 */
	virtual bool IsIntSetting() const { return false; }

	/**
	 * Check whether this setting is an string type setting.
	 * @return True when the underlying type is a string.
	 */
	virtual bool IsStringSetting() const { return false; }

	const struct IntSettingDesc *AsIntSetting() const;
	const struct StringSettingDesc *AsStringSetting() const;

	/**
	 * Format the value of the setting associated with this object.
	 * @param buf The before of the buffer to format into.
	 * @param last The end of the buffer to format into.
	 * @param object The object the setting is in.
	 */
	virtual void FormatValue(char *buf, const char *last, const void *object) const = 0;

	/**
	 * Parse/read the value from the Ini item into the setting associated with this object.
	 * @param item The Ini item with the content of this setting.
	 * @param object The object the setting is in.
	 */
	virtual void ParseValue(const IniItem *item, void *object) const = 0;

	/**
	 * Check whether the value in the Ini item is the same as is saved in this setting in the object.
	 * It might be that determining whether the value is the same is way more expensive than just
	 * writing the value. In those cases this function may unconditionally return false even though
	 * the value might be the same as in the Ini item.
	 * @param item The Ini item with the content of this setting.
	 * @param object The object the setting is in.
	 * @return True if the value is definitely the same (might be false when the same).
	 */
	virtual bool IsSameValue(const IniItem *item, void *object) const = 0;
};

/** Base integer type, including boolean, settings. Only these are shown in the settings UI. */
struct IntSettingDesc : SettingDesc {
	IntSettingDesc(SaveLoad save, const char *name, SettingGuiFlag flags, bool startup, int32 def,
		int32 min, uint32 max, int32 interval, StringID str, StringID str_help, StringID str_val,
		SettingCategory cat, OnChange *proc) :
		SettingDesc(save, name, flags, startup), def(def), min(min), max(max), interval(interval),
		str(str), str_help(str_help), str_val(str_val), cat(cat), proc(proc) {}
	virtual ~IntSettingDesc() {}

	int32 def;              ///< default value given when none is present
	int32 min;              ///< minimum values
	uint32 max;             ///< maximum values
	int32 interval;         ///< the interval to use between settings in the 'settings' window. If interval is '0' the interval is dynamically determined
	StringID str;           ///< (translated) string with descriptive text; gui and console
	StringID str_help;      ///< (Translated) string with help text; gui only.
	StringID str_val;       ///< (Translated) first string describing the value.
	SettingCategory cat;    ///< assigned categories of the setting
	OnChange *proc;         ///< callback procedure for when the value is changed

	/**
	 * Check whether this setting is a boolean type setting.
	 * @return True when the underlying type is an integer.
	 */
	virtual bool IsBoolSetting() const { return false; }
	bool IsIntSetting() const override { return true; }

	void ChangeValue(const void *object, int32 newvalue) const;
	void Write_ValidateSetting(const void *object, int32 value) const;

	virtual size_t ParseValue(const char *str) const;
	void FormatValue(char *buf, const char *last, const void *object) const override;
	void ParseValue(const IniItem *item, void *object) const override;
	bool IsSameValue(const IniItem *item, void *object) const override;
	int32 Read(const void *object) const;
};

/** Boolean setting. */
struct BoolSettingDesc : IntSettingDesc {
	BoolSettingDesc(SaveLoad save, const char *name, SettingGuiFlag flags, bool startup, bool def,
		StringID str, StringID str_help, StringID str_val, SettingCategory cat, OnChange *proc) :
		IntSettingDesc(save, name, flags, startup, def, 0, 1, 0, str, str_help, str_val, cat, proc) {}
	virtual ~BoolSettingDesc() {}

	bool IsBoolSetting() const override { return true; }
	size_t ParseValue(const char *str) const override;
	void FormatValue(char *buf, const char *last, const void *object) const override;
};

/** One of many setting. */
struct OneOfManySettingDesc : IntSettingDesc {
	OneOfManySettingDesc(SaveLoad save, const char *name, SettingGuiFlag flags, bool startup,
		int32 def, int32 max, StringID str, StringID str_help, StringID str_val, SettingCategory cat, OnChange *proc,
		std::initializer_list<const char *> many, OnConvert *many_cnvt) :
		IntSettingDesc(save, name, flags, startup, def, 0, max, 0, str, str_help, str_val, cat, proc), many_cnvt(many_cnvt)
	{
		for (auto one : many) this->many.push_back(one);
	}

	virtual ~OneOfManySettingDesc() {}

	std::vector<std::string> many; ///< possible values for this type
	OnConvert *many_cnvt;          ///< callback procedure when loading value mechanism fails

	static size_t ParseSingleValue(const char *str, size_t len, const std::vector<std::string> &many);
	char *FormatSingleValue(char *buf, const char *last, uint id) const;

	size_t ParseValue(const char *str) const override;
	void FormatValue(char *buf, const char *last, const void *object) const override;
};

/** Many of many setting. */
struct ManyOfManySettingDesc : OneOfManySettingDesc {
	ManyOfManySettingDesc(SaveLoad save, const char *name, SettingGuiFlag flags, bool startup,
		int32 def, StringID str, StringID str_help, StringID str_val, SettingCategory cat, OnChange *proc,
		std::initializer_list<const char *> many, OnConvert *many_cnvt) :
		OneOfManySettingDesc(save, name, flags, startup, def, (1 << many.size()) - 1, str, str_help,
			str_val, cat, proc, many, many_cnvt) {}
	virtual ~ManyOfManySettingDesc() {}

	size_t ParseValue(const char *str) const override;
	void FormatValue(char *buf, const char *last, const void *object) const override;
};

/** String settings. */
struct StringSettingDesc : SettingDesc {
	StringSettingDesc(SaveLoad save, const char *name, SettingGuiFlag flags, bool startup, const char *def,
			uint32 max_length, OnChange proc) :
		SettingDesc(save, name, flags, startup), def(def == nullptr ? "" : def), max_length(max_length),
			proc(proc) {}
	virtual ~StringSettingDesc() {}

	std::string def;        ///< default value given when none is present
	uint32 max_length;      ///< maximum length of the string, 0 means no maximum length
	OnChange *proc;         ///< callback procedure for when the value is changed

	bool IsStringSetting() const override { return true; }
	void ChangeValue(const void *object, std::string &newval) const;

	void FormatValue(char *buf, const char *last, const void *object) const override;
	void ParseValue(const IniItem *item, void *object) const override;
	bool IsSameValue(const IniItem *item, void *object) const override;
	const std::string &Read(const void *object) const;

private:
	void MakeValueValid(std::string &str) const;
	void Write(const void *object, const std::string &str) const;
};

/** List/array settings. */
struct ListSettingDesc : SettingDesc {
	ListSettingDesc(SaveLoad save, const char *name, SettingGuiFlag flags, bool startup, const char *def) :
		SettingDesc(save, name, flags, startup), def(def) {}
	virtual ~ListSettingDesc() {}

	const char *def;        ///< default value given when none is present

	void FormatValue(char *buf, const char *last, const void *object) const override;
	void ParseValue(const IniItem *item, void *object) const override;
	bool IsSameValue(const IniItem *item, void *object) const override;
};

/** Placeholder for settings that have been removed, but might still linger in the savegame. */
struct NullSettingDesc : SettingDesc {
	NullSettingDesc(SaveLoad save) :
		SettingDesc(save, "", SGF_NONE, false) {}
	virtual ~NullSettingDesc() {}

	void FormatValue(char *buf, const char *last, const void *object) const override { NOT_REACHED(); }
	void ParseValue(const IniItem *item, void *object) const override { NOT_REACHED(); }
	bool IsSameValue(const IniItem *item, void *object) const override { NOT_REACHED(); }
};

typedef std::initializer_list<std::unique_ptr<const SettingDesc>> SettingTable;

const SettingDesc *GetSettingFromName(const char *name);
bool SetSettingValue(const IntSettingDesc *sd, int32 value, bool force_newgame = false);
bool SetSettingValue(const StringSettingDesc *sd, const std::string value, bool force_newgame = false);
uint GetSettingIndex(const SettingDesc *sd);

#endif /* SETTINGS_INTERNAL_H */
