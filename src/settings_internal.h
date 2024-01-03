/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file settings_internal.h Functions and types used internally for the settings configurations. */

#ifndef SETTINGS_INTERNAL_H
#define SETTINGS_INTERNAL_H

#include <variant>
#include "saveload/saveload.h"

enum SettingFlag : uint16_t {
	SF_NONE = 0,
	SF_GUI_0_IS_SPECIAL        = 1 <<  0, ///< A value of zero is possible and has a custom string (the one after "strval").
	SF_GUI_NEGATIVE_IS_SPECIAL = 1 <<  1, ///< A negative value has another string (the one after "strval").
	SF_GUI_DROPDOWN            = 1 <<  2, ///< The value represents a limited number of string-options (internally integer) presented as dropdown.
	SF_GUI_CURRENCY            = 1 <<  3, ///< The number represents money, so when reading value multiply by exchange rate.
	SF_NETWORK_ONLY            = 1 <<  4, ///< This setting only applies to network games.
	SF_NO_NETWORK              = 1 <<  5, ///< This setting does not apply to network games; it may not be changed during the game.
	SF_NEWGAME_ONLY            = 1 <<  6, ///< This setting cannot be changed in a game.
	SF_SCENEDIT_TOO            = 1 <<  7, ///< This setting can be changed in the scenario editor (only makes sense when SF_NEWGAME_ONLY is set).
	SF_SCENEDIT_ONLY           = 1 <<  8, ///< This setting can only be changed in the scenario editor.
	SF_PER_COMPANY             = 1 <<  9, ///< This setting can be different for each company (saved in company struct).
	SF_NOT_IN_SAVE             = 1 << 10, ///< Do not save with savegame, basically client-based.
	SF_NOT_IN_CONFIG           = 1 << 11, ///< Do not save to config file.
	SF_NO_NETWORK_SYNC         = 1 << 12, ///< Do not synchronize over network (but it is saved if SF_NOT_IN_SAVE is not set).
};
DECLARE_ENUM_AS_BIT_SET(SettingFlag)

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

/** Properties of config file settings. */
struct SettingDesc {
	SettingDesc(const SaveLoad &save, SettingFlag flags, bool startup) :
		flags(flags), startup(startup), save(save) {}
	virtual ~SettingDesc() = default;

	SettingFlag flags;  ///< Handles how a setting would show up in the GUI (text/currency, etc.).
	bool startup;       ///< Setting has to be loaded directly at startup?.
	SaveLoad save;      ///< Internal structure (going to savegame, parts to config).

	bool IsEditable(bool do_command = false) const;
	SettingType GetType() const;

	/**
	 * Get the name of this setting.
	 * @return The name of the setting.
	 */
	constexpr const std::string &GetName() const
	{
		return this->save.name;
	}

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
	virtual std::string FormatValue(const void *object) const = 0;

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

	/**
	 * Check whether the value is the same as the default value.
	 *
	 * @param object The object the setting is in.
	 * @return true iff the value is the default value.
	 */
	virtual bool IsDefaultValue(void *object) const = 0;
};

/** Base integer type, including boolean, settings. Only these are shown in the settings UI. */
struct IntSettingDesc : SettingDesc {
	/**
	 * A check to be performed before the setting gets changed. The passed integer may be
	 * changed by the check if that is important, for example to remove some unwanted bit.
	 * The return value denotes whether the value, potentially after the changes,
	 * is allowed to be used/set in the configuration.
	 * @param value The prospective new value for the setting.
	 * @return True when the setting is accepted.
	 */
	typedef bool PreChangeCheck(int32_t &value);
	/**
	 * A callback to denote that a setting has been changed.
	 * @param The new value for the setting.
	 */
	typedef void PostChangeCallback(int32_t value);

	template <
		typename Tdef,
		typename Tmin,
		typename Tmax,
		typename Tinterval,
		std::enable_if_t<std::disjunction_v<std::is_convertible<Tdef, int32_t>, std::is_base_of<StrongTypedefBase, Tdef>>, int> = 0,
		std::enable_if_t<std::disjunction_v<std::is_convertible<Tmin, int32_t>, std::is_base_of<StrongTypedefBase, Tmin>>, int> = 0,
		std::enable_if_t<std::disjunction_v<std::is_convertible<Tmax, uint32_t>, std::is_base_of<StrongTypedefBase, Tmax>>, int> = 0,
		std::enable_if_t<std::disjunction_v<std::is_convertible<Tinterval, int32_t>, std::is_base_of<StrongTypedefBase, Tinterval>>, int> = 0
	>
	IntSettingDesc(const SaveLoad &save, SettingFlag flags, bool startup, Tdef def,
			Tmin min, Tmax max, Tinterval interval, StringID str, StringID str_help, StringID str_val,
			SettingCategory cat, PreChangeCheck pre_check, PostChangeCallback post_callback) :
		SettingDesc(save, flags, startup),
			str(str), str_help(str_help), str_val(str_val), cat(cat), pre_check(pre_check),
			post_callback(post_callback) {
		if constexpr (std::is_base_of_v<StrongTypedefBase, Tdef>) {
			this->def = def.base();
		} else {
			this->def = def;
		}

		if constexpr (std::is_base_of_v<StrongTypedefBase, Tmin>) {
			this->min = min.base();
		} else {
			this->min = min;
		}

		if constexpr (std::is_base_of_v<StrongTypedefBase, Tmax>) {
			this->max = max.base();
		} else {
			this->max = max;
		}

		if constexpr (std::is_base_of_v<StrongTypedefBase, Tinterval>) {
			this->interval = interval.base();
		} else {
			this->interval = interval;
		}
	}

	int32_t def;              ///< default value given when none is present
	int32_t min;              ///< minimum values
	uint32_t max;             ///< maximum values
	int32_t interval;         ///< the interval to use between settings in the 'settings' window. If interval is '0' the interval is dynamically determined
	StringID str;           ///< (translated) string with descriptive text; gui and console
	StringID str_help;      ///< (Translated) string with help text; gui only.
	StringID str_val;       ///< (Translated) first string describing the value.
	SettingCategory cat;    ///< assigned categories of the setting
	PreChangeCheck *pre_check;         ///< Callback to check for the validity of the setting.
	PostChangeCallback *post_callback; ///< Callback when the setting has been changed.

	/**
	 * Check whether this setting is a boolean type setting.
	 * @return True when the underlying type is an integer.
	 */
	virtual bool IsBoolSetting() const { return false; }
	bool IsIntSetting() const override { return true; }

	void ChangeValue(const void *object, int32_t newvalue) const;
	void MakeValueValidAndWrite(const void *object, int32_t value) const;

	virtual size_t ParseValue(const char *str) const;
	std::string FormatValue(const void *object) const override;
	void ParseValue(const IniItem *item, void *object) const override;
	bool IsSameValue(const IniItem *item, void *object) const override;
	bool IsDefaultValue(void *object) const override;
	int32_t Read(const void *object) const;

private:
	void MakeValueValid(int32_t &value) const;
	void Write(const void *object, int32_t value) const;
};

/** Boolean setting. */
struct BoolSettingDesc : IntSettingDesc {
	BoolSettingDesc(const SaveLoad &save, SettingFlag flags, bool startup, bool def,
			StringID str, StringID str_help, StringID str_val, SettingCategory cat,
			PreChangeCheck pre_check, PostChangeCallback post_callback) :
		IntSettingDesc(save, flags, startup, def ? 1 : 0, 0, 1, 0, str, str_help, str_val, cat,
			pre_check, post_callback) {}

	static std::optional<bool> ParseSingleValue(const char *str);

	bool IsBoolSetting() const override { return true; }
	size_t ParseValue(const char *str) const override;
	std::string FormatValue(const void *object) const override;
};

/** One of many setting. */
struct OneOfManySettingDesc : IntSettingDesc {
	typedef size_t OnConvert(const char *value); ///< callback prototype for conversion error

	OneOfManySettingDesc(const SaveLoad &save, SettingFlag flags, bool startup, int32_t def,
			int32_t max, StringID str, StringID str_help, StringID str_val, SettingCategory cat,
			PreChangeCheck pre_check, PostChangeCallback post_callback,
			std::initializer_list<const char *> many, OnConvert *many_cnvt) :
		IntSettingDesc(save, flags, startup, def, 0, max, 0, str, str_help, str_val, cat,
			pre_check, post_callback), many_cnvt(many_cnvt)
	{
		for (auto one : many) this->many.push_back(one);
	}

	std::vector<std::string> many; ///< possible values for this type
	OnConvert *many_cnvt;          ///< callback procedure when loading value mechanism fails

	static size_t ParseSingleValue(const char *str, size_t len, const std::vector<std::string> &many);
	std::string FormatSingleValue(uint id) const;

	size_t ParseValue(const char *str) const override;
	std::string FormatValue(const void *object) const override;
};

/** Many of many setting. */
struct ManyOfManySettingDesc : OneOfManySettingDesc {
	ManyOfManySettingDesc(const SaveLoad &save, SettingFlag flags, bool startup,
		int32_t def, StringID str, StringID str_help, StringID str_val, SettingCategory cat,
		PreChangeCheck pre_check, PostChangeCallback post_callback,
		std::initializer_list<const char *> many, OnConvert *many_cnvt) :
		OneOfManySettingDesc(save, flags, startup, def, (1 << many.size()) - 1, str, str_help,
			str_val, cat, pre_check, post_callback, many, many_cnvt) {}

	size_t ParseValue(const char *str) const override;
	std::string FormatValue(const void *object) const override;
};

/** String settings. */
struct StringSettingDesc : SettingDesc {
	/**
	 * A check to be performed before the setting gets changed. The passed string may be
	 * changed by the check if that is important, for example to remove unwanted white
	 * space. The return value denotes whether the value, potentially after the changes,
	 * is allowed to be used/set in the configuration.
	 * @param value The prospective new value for the setting.
	 * @return True when the setting is accepted.
	 */
	typedef bool PreChangeCheck(std::string &value);
	/**
	 * A callback to denote that a setting has been changed.
	 * @param The new value for the setting.
	 */
	typedef void PostChangeCallback(const std::string &value);

	StringSettingDesc(const SaveLoad &save, SettingFlag flags, bool startup, const char *def,
			uint32_t max_length, PreChangeCheck pre_check, PostChangeCallback post_callback) :
		SettingDesc(save, flags, startup), def(def == nullptr ? "" : def), max_length(max_length),
			pre_check(pre_check), post_callback(post_callback) {}

	std::string def;                   ///< Default value given when none is present
	uint32_t max_length;                 ///< Maximum length of the string, 0 means no maximum length
	PreChangeCheck *pre_check;         ///< Callback to check for the validity of the setting.
	PostChangeCallback *post_callback; ///< Callback when the setting has been changed.

	bool IsStringSetting() const override { return true; }
	void ChangeValue(const void *object, std::string &newval) const;

	std::string FormatValue(const void *object) const override;
	void ParseValue(const IniItem *item, void *object) const override;
	bool IsSameValue(const IniItem *item, void *object) const override;
	bool IsDefaultValue(void *object) const override;
	const std::string &Read(const void *object) const;

private:
	void MakeValueValid(std::string &str) const;
	void Write(const void *object, const std::string &str) const;
};

/** List/array settings. */
struct ListSettingDesc : SettingDesc {
	ListSettingDesc(const SaveLoad &save, SettingFlag flags, bool startup, const char *def) :
		SettingDesc(save, flags, startup), def(def) {}

	const char *def;        ///< default value given when none is present

	std::string FormatValue(const void *object) const override;
	void ParseValue(const IniItem *item, void *object) const override;
	bool IsSameValue(const IniItem *item, void *object) const override;
	bool IsDefaultValue(void *object) const override;
};

/** Placeholder for settings that have been removed, but might still linger in the savegame. */
struct NullSettingDesc : SettingDesc {
	NullSettingDesc(const SaveLoad &save) :
		SettingDesc(save, SF_NOT_IN_CONFIG, false) {}

	std::string FormatValue(const void *) const override { NOT_REACHED(); }
	void ParseValue(const IniItem *, void *) const override { NOT_REACHED(); }
	bool IsSameValue(const IniItem *, void *) const override { NOT_REACHED(); }
	bool IsDefaultValue(void *) const override { NOT_REACHED(); }
};

typedef std::variant<IntSettingDesc, BoolSettingDesc, OneOfManySettingDesc, ManyOfManySettingDesc, StringSettingDesc, ListSettingDesc, NullSettingDesc> SettingVariant;

/**
 * Helper to convert the type of the iterated settings description to a pointer to it.
 * @param desc The type of the iterator of the value in SettingTable.
 * @return The actual pointer to SettingDesc.
 */
static constexpr const SettingDesc *GetSettingDesc(const SettingVariant &desc)
{
	return std::visit([](auto&& arg) -> const SettingDesc * { return &arg; }, desc);
}

typedef span<const SettingVariant> SettingTable;

const SettingDesc *GetSettingFromName(const std::string_view name);
void GetSaveLoadFromSettingTable(SettingTable settings, std::vector<SaveLoad> &saveloads);
bool SetSettingValue(const IntSettingDesc *sd, int32_t value, bool force_newgame = false);
bool SetSettingValue(const StringSettingDesc *sd, const std::string value, bool force_newgame = false);

#endif /* SETTINGS_INTERNAL_H */
