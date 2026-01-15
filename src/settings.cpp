/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file settings.cpp All actions handling saving and loading of the settings/configuration goes on in this file.
 *
 * The file consists of three parts:
 * <ol>
 * <li>Parsing the configuration file (openttd.cfg). This is achieved with the ini_ functions which
 *     handle various types, such as normal 'key = value' pairs, lists and value combinations of
 *     lists, strings, integers, 'bit'-masks and element selections.
 * <li>Handle reading and writing to the setting-structures from inside the game either from
 *     the console for example or through the gui with CMD_ functions.
 * <li>Handle saving/loading of the PATS chunk inside the savegame.
 * </ol>
 * @see SettingDesc
 * @see SaveLoad
 */

#include "stdafx.h"
#include <charconv>
#include "core/string_consumer.hpp"
#include "settings_table.h"
#include "debug.h"
#include "currency.h"
#include "network/network.h"
#include "network/network_func.h"
#include "network/core/config.h"
#include "command_func.h"
#include "console_func.h"
#include "genworld.h"
#include "string_func.h"
#include "strings_func.h"
#include "window_func.h"
#include "company_func.h"
#include "rev.h"
#include "error.h"
#include "gamelog.h"
#include "settings_func.h"
#include "ini_type.h"
#include "ai/ai_config.hpp"
#include "game/game_config.hpp"
#include "newgrf_config.h"
#include "picker_func.h"
#include "newgrf_badge_config.h"
#include "base_media_base.h"
#include "base_media_graphics.h"
#include "fios.h"
#include "fileio_func.h"
#include "settings_cmd.h"

#include "table/strings.h"

#include "safeguards.h"

ClientSettings _settings_client;
GameSettings _settings_game;     ///< Game settings of a running game or the scenario editor.
GameSettings _settings_newgame;  ///< Game settings for new games (updated from the intro screen).
VehicleDefaultSettings _old_vds; ///< Used for loading default vehicles settings from old savegames.
std::string _config_file; ///< Configuration file of OpenTTD.
std::string _private_file; ///< Private configuration file of OpenTTD.
std::string _secrets_file; ///< Secrets configuration file of OpenTTD.
std::string _favs_file; ///< Picker favourites configuration file of OpenTTD.

static ErrorList _settings_error_list; ///< Errors while loading minimal settings.

/**
 * List of all the generic setting tables.
 *
 * There are a few tables that are special and not processed like the rest:
 * - _currency_settings
 * - _misc_settings
 * - _company_settings
 * - _win32_settings
 * As such, they are not part of this list.
 */
static auto &GenericSettingTables()
{
	static const SettingTable _generic_setting_tables[] = {
		_difficulty_settings,
		_economy_settings,
		_game_settings,
		_gui_settings,
		_linkgraph_settings,
		_locale_settings,
		_multimedia_settings,
		_network_settings,
		_news_display_settings,
		_pathfinding_settings,
		_script_settings,
		_world_settings,
	};
	return _generic_setting_tables;
}

/**
 * List of all the private setting tables.
 */
static auto &PrivateSettingTables()
{
	static const SettingTable _private_setting_tables[] = {
		_network_private_settings,
	};
	return _private_setting_tables;
}

/**
 * List of all the secrets setting tables.
 */
static auto &SecretSettingTables()
{
	static const SettingTable _secrets_setting_tables[] = {
		_network_secrets_settings,
	};
	return _secrets_setting_tables;
}

using SettingDescProc = void(IniFile &ini, const SettingTable &desc, std::string_view grpname, void *object, bool only_startup);
using SettingDescProcList = void(IniFile &ini, std::string_view grpname, StringList &list);

static bool IsSignedVarMemType(VarType vt)
{
	switch (GetVarMemType(vt)) {
		case SLE_VAR_I8:
		case SLE_VAR_I16:
		case SLE_VAR_I32:
		case SLE_VAR_I64:
			return true;
	}
	return false;
}

/**
 * IniFile to store a configuration.
 */
class ConfigIniFile : public IniFile {
private:
	static inline const IniGroupNameList list_group_names = {
		"bans",
		"newgrf",
		"servers",
		"server_bind_addresses",
		"server_authorized_keys",
		"rcon_authorized_keys",
		"admin_authorized_keys"
	};

public:
	ConfigIniFile(const std::string &filename) : IniFile(list_group_names)
	{
		this->LoadFromDisk(filename, NO_DIRECTORY);
	}
};

/**
 * Ini-file versions.
 *
 * Sometimes we move settings between different ini-files, as we need to know
 * when we have to load/remove it from the old versus reading it from the new
 * location. These versions assist with situations like that.
 */
enum IniFileVersion : uint32_t {
	IFV_0,                                                 ///< 0  All versions prior to introduction.
	IFV_PRIVATE_SECRETS,                                   ///< 1  PR#9298  Moving of settings from openttd.cfg to private.cfg / secrets.cfg.
	IFV_GAME_TYPE,                                         ///< 2  PR#9515  Convert server_advertise to server_game_type.
	IFV_LINKGRAPH_SECONDS,                                 ///< 3  PR#10610 Store linkgraph update intervals in seconds instead of days.
	IFV_NETWORK_PRIVATE_SETTINGS,                          ///< 4  PR#10762 Move use_relay_service to private settings.

	IFV_AUTOSAVE_RENAME,                                   ///< 5  PR#11143 Renamed values of autosave to be in minutes.
	IFV_RIGHT_CLICK_CLOSE,                                 ///< 6  PR#10204 Add alternative right click to close windows setting.
	IFV_REMOVE_GENERATION_SEED,                            ///< 7  PR#11927 Remove "generation_seed" from configuration.

	IFV_MAX_VERSION,       ///< Highest possible ini-file version.
};

const uint16_t INIFILE_VERSION = (IniFileVersion)(IFV_MAX_VERSION - 1); ///< Current ini-file version of OpenTTD.

/**
 * Find the index value of a ONEofMANY type in a string
 * @param str the current value of the setting for which a value needs found
 * @param many full domain of values the ONEofMANY setting can have
 * @return the integer index of the full-list, or std::nullopt if not found
 */
std::optional<uint32_t> OneOfManySettingDesc::ParseSingleValue(std::string_view str, std::span<const std::string_view> many)
{
	StringConsumer consumer{str};
	auto digit = consumer.TryReadIntegerBase<uint32_t>(10);
	/* check if it's an integer */
	if (digit.has_value()) return digit;

	auto it = std::ranges::find(many, str);
	if (it == many.end()) return std::nullopt;
	return static_cast<uint32_t>(it - many.begin());
}

/**
 * Find whether a string was a boolean true or a boolean false.
 *
 * @param str the current value of the setting for which a value needs found.
 * @return Either true/false, or nullopt if no boolean value found.
 */
std::optional<bool> BoolSettingDesc::ParseSingleValue(std::string_view str)
{
	if (str == "true" || str == "on" || str == "1") return true;
	if (str == "false" || str == "off" || str == "0") return false;

	return std::nullopt;
}

/**
 * Find the set-integer value MANYofMANY type in a string
 * @param many full domain of values the MANYofMANY setting can have
 * @param str the current string value of the setting, each individual
 * of separated by a whitespace, tab or | character
 * @return the 'fully' set integer, or std::nullopt if a set is not found
 */
static std::optional<uint32_t> LookupManyOfMany(std::span<const std::string_view> many, std::string_view str)
{
	static const std::string_view separators{" \t|"};

	uint32_t res = 0;
	StringConsumer consumer{str};
	while (consumer.AnyBytesLeft()) {
		/* skip "whitespace" */
		consumer.SkipUntilCharNotIn(separators);

		std::string_view value = consumer.ReadUntilCharIn(separators);
		if (value.empty()) break;

		auto r = OneOfManySettingDesc::ParseSingleValue(value, many);
		if (!r.has_value()) return r;

		SetBit(res, *r); // value found, set it
	}
	return res;
}

/**
 * Parse a string into a vector of uint32s.
 * @param str the string to be parsed. Each element in the list is separated by a comma or a space character
 * @return std::optional with a vector of parsed integers. The optional is empty upon an error.
 */
static std::optional<std::vector<uint32_t>> ParseIntList(std::string_view str)
{
	bool comma = false; // do we accept comma?
	std::vector<uint32_t> result;

	StringConsumer consumer{str};
	for (;;) {
		consumer.SkipUntilCharNotIn(StringConsumer::WHITESPACE_NO_NEWLINE);
		if (!consumer.AnyBytesLeft()) break;
		if (comma && consumer.ReadIf(",")) {
			/* commas are optional, but we only accept one between values */
			comma = false;
			continue;
		}
		auto v = consumer.TryReadIntegerBase<uint32_t>(10);
		if (!v.has_value()) return std::nullopt;
		result.push_back(*v);
		comma = true;
	}

	/* If we have read comma but no number after it, fail.
	 * We have read comma when (n != 0) and comma is not allowed */
	if (!result.empty() && !comma) return std::nullopt;

	return result;
}

/**
 * Load parsed string-values into an integer-array (intlist)
 * @param str the string that contains the values (and will be parsed)
 * @param array pointer to the integer-arrays that will be filled
 * @param nelems the number of elements the array holds.
 * @param type the type of elements the array holds (eg INT8, UINT16, etc.)
 * @return return true on success and false on error
 */
static bool LoadIntList(std::optional<std::string_view> str, void *array, int nelems, VarType type)
{
	size_t elem_size = SlVarSize(type);
	std::byte *p = static_cast<std::byte *>(array);
	if (!str.has_value()) {
		std::fill_n(p, nelems * elem_size, static_cast<std::byte>(0));
		return true;
	}

	auto opt_items = ParseIntList(*str);
	if (!opt_items.has_value() || opt_items->size() != (size_t)nelems) return false;

	for (auto item : *opt_items) {
		WriteValue(p, type, item);
		p += elem_size;
	}
	return true;
}

/**
 * Convert an integer-array (intlist) to a string representation. Each value
 * is separated by a comma or a space character
 * @param buf output buffer where the string-representation will be stored
 * @param last last item to write to in the output buffer
 * @param array pointer to the integer-arrays that is read from
 * @param nelems the number of elements the array holds.
 * @param type the type of elements the array holds (eg INT8, UINT16, etc.)
 */
std::string ListSettingDesc::FormatValue(const void *object) const
{
	const uint8_t *p = static_cast<const uint8_t *>(GetVariableAddress(object, this->save));

	std::string result;
	for (size_t i = 0; i != this->save.length; i++) {
		int64_t v;
		switch (GetVarMemType(this->save.conv)) {
			case SLE_VAR_BL:
			case SLE_VAR_I8:  v = *(const   int8_t *)p; p += 1; break;
			case SLE_VAR_U8:  v = *(const  uint8_t *)p; p += 1; break;
			case SLE_VAR_I16: v = *(const  int16_t *)p; p += 2; break;
			case SLE_VAR_U16: v = *(const uint16_t *)p; p += 2; break;
			case SLE_VAR_I32: v = *(const  int32_t *)p; p += 4; break;
			case SLE_VAR_U32: v = *(const uint32_t *)p; p += 4; break;
			default: NOT_REACHED();
		}
		if (i != 0) result += ',';
		format_append(result, "{}", v);
	}
	return result;
}

std::string OneOfManySettingDesc::FormatSingleValue(uint id) const
{
	if (id >= this->many.size()) {
		return fmt::format("{}", id);
	}
	return std::string{this->many[id]};
}

std::string OneOfManySettingDesc::FormatValue(const void *object) const
{
	uint id = (uint)this->Read(object);
	return this->FormatSingleValue(id);
}

std::string ManyOfManySettingDesc::FormatValue(const void *object) const
{
	uint bitmask = (uint)this->Read(object);
	if (bitmask == 0) {
		return {};
	}

	std::string result;
	for (uint id : SetBitIterator(bitmask)) {
		if (!result.empty()) result += '|';
		result += this->FormatSingleValue(id);
	}
	return result;
}

/**
 * Convert a string representation (external) of an integer-like setting to an integer.
 * @param str Input string that will be parsed based on the type of desc.
 * @return The value from the parse string, or the default value of the setting.
 */
int32_t IntSettingDesc::ParseValue(std::string_view str) const
{
	StringConsumer consumer{str};
	/* The actual settings value might be int32 or uint32. Read as int64 and just cast away the high bits. */
	auto value = consumer.TryReadIntegerBase<int64_t>(10);
	if (!value.has_value()) {
		_settings_error_list.emplace_back(
			GetEncodedString(STR_CONFIG_ERROR),
			GetEncodedString(STR_CONFIG_ERROR_INVALID_VALUE, str, this->GetName()));
		return this->GetDefaultValue();
	}
	if (consumer.AnyBytesLeft()) {
		_settings_error_list.emplace_back(
			GetEncodedString(STR_CONFIG_ERROR),
			GetEncodedString(STR_CONFIG_ERROR_TRAILING_CHARACTERS, this->GetName()));
	}
	return static_cast<int32_t>(*value);
}

int32_t OneOfManySettingDesc::ParseValue(std::string_view str) const
{
	auto r = OneOfManySettingDesc::ParseSingleValue(str, this->many);
	/* if the first attempt of conversion from string to the appropriate value fails,
	 * look if we have defined a converter from old value to new value. */
	if (!r.has_value() && this->many_cnvt != nullptr) r = this->many_cnvt(str);
	if (r.has_value()) return *r; // and here goes converted value

	_settings_error_list.emplace_back(
		GetEncodedString(STR_CONFIG_ERROR),
		GetEncodedString(STR_CONFIG_ERROR_INVALID_VALUE, str, this->GetName()));
	return this->GetDefaultValue();
}

int32_t ManyOfManySettingDesc::ParseValue(std::string_view str) const
{
	auto r = LookupManyOfMany(this->many, str);
	if (r.has_value()) return *r;

	_settings_error_list.emplace_back(
		GetEncodedString(STR_CONFIG_ERROR),
		GetEncodedString(STR_CONFIG_ERROR_INVALID_VALUE, str, this->GetName()));
	return this->GetDefaultValue();
}

int32_t BoolSettingDesc::ParseValue(std::string_view str) const
{
	auto r = BoolSettingDesc::ParseSingleValue(str);
	if (r.has_value()) return *r;

	_settings_error_list.emplace_back(
		GetEncodedString(STR_CONFIG_ERROR),
		GetEncodedString(STR_CONFIG_ERROR_INVALID_VALUE, str, this->GetName()));
	return this->GetDefaultValue();
}

/**
 * Get the title of the setting.
 * The string should include a {STRING2} to show the current value.
 * @return The title string.
 */
StringID IntSettingDesc::GetTitle() const
{
	return this->get_title_cb != nullptr ? this->get_title_cb(*this) : this->str;
}

/**
 * Get the help text of the setting.
 * @return The requested help text.
 */
StringID IntSettingDesc::GetHelp() const
{
	return this->get_help_cb != nullptr ? this->get_help_cb(*this) : this->str_help;
}

/**
 * Get parameters for drawing the value of the setting.
 * @param value Setting value to set params for.
 */
std::pair<StringParameter, StringParameter> IntSettingDesc::GetValueParams(int32_t value) const
{
	if (this->get_value_params_cb != nullptr) {
		return this->get_value_params_cb(*this, value);
	}

	if (this->IsBoolSetting()) {
		return {value != 0 ? STR_CONFIG_SETTING_ON : STR_CONFIG_SETTING_OFF, {}};
	}

	if (this->flags.Test(SettingFlag::GuiDropdown)) {
		auto [min_val, _] = this->GetRange();
		return {this->str_val - min_val + value, value};
	}

	return {this->str_val + ((value == 0 && this->flags.Test(SettingFlag::GuiZeroIsSpecial)) ? 1 : 0), value};
}

/**
 * Get the default value of the setting.
 * @return The default value.
 */
int32_t IntSettingDesc::GetDefaultValue() const
{
	return this->get_def_cb != nullptr ? this->get_def_cb(*this) : this->def;
}

/**
 * Get the min/max range for the setting.
 * @return The min/max range.
 */
std::tuple<int32_t, uint32_t> IntSettingDesc::GetRange() const
{
	return this->get_range_cb != nullptr ? this->get_range_cb(*this) : std::tuple(this->min, this->max);
}

/**
 * Make the value valid and then write it to the setting.
 * See #MakeValidValid and #Write for more details.
 * @param object The object the setting is to be saved in.
 * @param val Signed version of the new value.
 */
void IntSettingDesc::MakeValueValidAndWrite(const void *object, int32_t val) const
{
	this->MakeValueValid(val);
	this->Write(object, val);
}

/**
 * Make the value valid given the limitations of this setting.
 *
 * In the case of int settings this is ensuring the value is between the minimum and
 * maximum value, with a special case for 0 if SettingFlag::GuiZeroIsSpecial is set.
 * This is generally done by clamping the value so it is within the allowed value range.
 * However, for SettingFlag::GuiDropdown the default is used when the value is not valid.
 * @param val The value to make valid.
 */
void IntSettingDesc::MakeValueValid(int32_t &val) const
{
	auto [min_val, max_val] = this->GetRange();
	/* We need to take special care of the uint32_t type as we receive from the function
	 * a signed integer. While here also bail out on 64-bit settings as those are not
	 * supported. Unsigned 8 and 16-bit variables are safe since they fit into a signed
	 * 32-bit variable
	 * TODO: Support 64-bit settings/variables; requires 64 bit over command protocol! */
	switch (GetVarMemType(this->save.conv)) {
		case SLE_VAR_NULL: return;
		case SLE_VAR_BL:
		case SLE_VAR_I8:
		case SLE_VAR_U8:
		case SLE_VAR_I16:
		case SLE_VAR_U16:
		case SLE_VAR_I32: {
			/* Override the minimum value. No value below this->min, except special value 0 */
			if (!this->flags.Test(SettingFlag::GuiZeroIsSpecial) || val != 0) {
				if (!this->flags.Test(SettingFlag::GuiDropdown)) {
					/* Clamp value-type setting to its valid range */
					val = Clamp(val, min_val, max_val);
				} else if (val < min_val || val > static_cast<int32_t>(max_val)) {
					/* Reset invalid discrete setting (where different values change gameplay) to its default value */
					val = this->GetDefaultValue();
				}
			}
			break;
		}
		case SLE_VAR_U32: {
			/* Override the minimum value. No value below this->min, except special value 0 */
			uint32_t uval = static_cast<uint32_t>(val);
			if (!this->flags.Test(SettingFlag::GuiZeroIsSpecial) || uval != 0) {
				if (!this->flags.Test(SettingFlag::GuiDropdown)) {
					/* Clamp value-type setting to its valid range */
					uval = ClampU(uval, min_val, max_val);
				} else if (uval < static_cast<uint32_t>(min_val) || uval > max_val) {
					/* Reset invalid discrete setting to its default value */
					uval = static_cast<uint32_t>(this->GetDefaultValue());
				}
			}
			val = static_cast<int32_t>(uval);
			return;
		}
		case SLE_VAR_I64:
		case SLE_VAR_U64:
		default: NOT_REACHED();
	}
}

/**
 * Set the value of a setting.
 * @param object The object the setting is to be saved in.
 * @param val Signed version of the new value.
 */
void IntSettingDesc::Write(const void *object, int32_t val) const
{
	void *ptr = GetVariableAddress(object, this->save);
	WriteValue(ptr, this->save.conv, (int64_t)val);
}

/**
 * Read the integer from the the actual setting.
 * @param object The object the setting is to be saved in.
 * @return The value of the saved integer.
 */
int32_t IntSettingDesc::Read(const void *object) const
{
	void *ptr = GetVariableAddress(object, this->save);
	return (int32_t)ReadValue(ptr, this->save.conv);
}

/**
 * Make the value valid given the limitations of this setting.
 *
 * In the case of string settings this is ensuring the string contains only accepted
 * Utf8 characters and is at most the maximum length defined in this setting.
 * @param str The string to make valid.
 */
void StringSettingDesc::MakeValueValid(std::string &str) const
{
	if (this->max_length == 0 || str.size() < this->max_length) return;

	/* In case a maximum length is imposed by the setting, the length
	 * includes the '\0' termination for network transfer purposes.
	 * Also ensure the string is valid after chopping of some bytes. */
	str.erase(this->max_length - 1, std::string::npos);
	StrMakeValidInPlace(str, {});
}

/**
 * Write a string to the actual setting.
 * @param object The object the setting is to be saved in.
 * @param str The string to save.
 */
void StringSettingDesc::Write(const void *object, std::string_view str) const
{
	reinterpret_cast<std::string *>(GetVariableAddress(object, this->save))->assign(str);
}

/**
 * Read the string from the the actual setting.
 * @param object The object the setting is to be saved in.
 * @return The value of the saved string.
 */
const std::string &StringSettingDesc::Read(const void *object) const
{
	return *reinterpret_cast<std::string *>(GetVariableAddress(object, this->save));
}

/**
 * Load values from a group of an IniFile structure into the internal representation
 * @param ini pointer to IniFile structure that holds administrative information
 * @param settings_table table with SettingDesc structures whose internally pointed variables will
 *        be given values
 * @param grpname the group of the IniFile to search in for the new values
 * @param object pointer to the object been loaded
 * @param only_startup load only the startup settings set
 */
static void IniLoadSettings(IniFile &ini, const SettingTable &settings_table, std::string_view grpname, void *object, bool only_startup)
{
	const IniGroup *group;
	const IniGroup *group_def = ini.GetGroup(grpname);

	for (auto &desc : settings_table) {
		const SettingDesc *sd = GetSettingDesc(desc);
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (sd->startup != only_startup) continue;

		/* For settings.xx.yy load the settings from [xx] yy = ? */
		std::string s{ sd->GetName() };
		auto sc = s.find('.');
		if (sc != std::string::npos) {
			group = ini.GetGroup(s.substr(0, sc));
			if (group == nullptr) group = group_def;
			s = s.substr(sc + 1);
		} else {
			group = group_def;
		}

		const IniItem *item = nullptr;
		if (group != nullptr) item = group->GetItem(s);
		if (item == nullptr && group != group_def && group_def != nullptr) {
			/* For settings.xx.yy load the settings from [settings] yy = ? in case the previous
			 * did not exist (e.g. loading old config files with a [settings] section */
			item = group_def->GetItem(s);
		}
		if (item == nullptr) {
			/* For settings.xx.zz.yy load the settings from [zz] yy = ? in case the previous
			 * did not exist (e.g. loading old config files with a [yapf] section */
			sc = s.find('.');
			if (sc != std::string::npos) {
				if (group = ini.GetGroup(s.substr(0, sc)); group != nullptr) item = group->GetItem(s.substr(sc + 1));
			}
		}

		sd->ParseValue(item, object);
	}
}

void IntSettingDesc::ParseValue(const IniItem *item, void *object) const
{
	int32_t val = (item != nullptr && item->value.has_value()) ? this->ParseValue(*item->value) : this->GetDefaultValue();
	this->MakeValueValidAndWrite(object, val);
}

void StringSettingDesc::ParseValue(const IniItem *item, void *object) const
{
	std::string str{(item == nullptr) ? this->def : item->value.value_or("")};
	this->MakeValueValid(str);
	this->Write(object, str);
}

void ListSettingDesc::ParseValue(const IniItem *item, void *object) const
{
	std::optional<std::string_view> str;
	if (item != nullptr) {
		str = item->value;
	} else if (!this->def.empty()) {
		str = this->def;
	}
	void *ptr = GetVariableAddress(object, this->save);
	if (!LoadIntList(str, ptr, this->save.length, GetVarMemType(this->save.conv))) {
		_settings_error_list.emplace_back(
			GetEncodedString(STR_CONFIG_ERROR),
			GetEncodedString(STR_CONFIG_ERROR_ARRAY, this->GetName()));

		/* Use default */
		LoadIntList(this->def, ptr, this->save.length, GetVarMemType(this->save.conv));
	}
}

/**
 * Save the values of settings to the inifile.
 * @param ini pointer to IniFile structure
 * @param sd read-only SettingDesc structure which contains the unmodified,
 *        loaded values of the configuration file and various information about it
 * @param grpname holds the name of the group (eg. [network]) where these will be saved
 * @param object pointer to the object been saved
 * The function works as follows: for each item in the SettingDesc structure we
 * have a look if the value has changed since we started the game (the original
 * values are reloaded when saving). If settings indeed have changed, we get
 * these and save them.
 */
static void IniSaveSettings(IniFile &ini, const SettingTable &settings_table, std::string_view grpname, void *object, bool)
{
	IniGroup *group_def = nullptr, *group;

	for (auto &desc : settings_table) {
		const SettingDesc *sd = GetSettingDesc(desc);
		/* If the setting is not saved to the configuration
		 * file, just continue with the next setting */
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (sd->flags.Test(SettingFlag::NotInConfig)) continue;

		/* XXX - wtf is this?? (group override?) */
		std::string s{ sd->GetName() };
		auto sc = s.find('.');
		if (sc != std::string::npos) {
			group = &ini.GetOrCreateGroup(s.substr(0, sc));
			s = s.substr(sc + 1);
		} else {
			if (group_def == nullptr) group_def = &ini.GetOrCreateGroup(grpname);
			group = group_def;
		}

		IniItem &item = group->GetOrCreateItem(s);

		if (!item.value.has_value() || !sd->IsSameValue(&item, object)) {
			/* The value is different, that means we have to write it to the ini */
			item.value.emplace(sd->FormatValue(object));
		}
	}
}

std::string IntSettingDesc::FormatValue(const void *object) const
{
	int64_t i;
	if (IsSignedVarMemType(this->save.conv)) {
		i = this->Read(object);
	} else {
		i = (uint32_t)this->Read(object);
	}
	return fmt::format("{}", i);
}

std::string BoolSettingDesc::FormatValue(const void *object) const
{
	bool val = this->Read(object) != 0;
	return val ? "true" : "false";
}

bool IntSettingDesc::IsSameValue(const IniItem *item, void *object) const
{
	int32_t item_value = static_cast<int32_t>(this->ParseValue(*item->value));
	int32_t object_value = this->Read(object);
	return item_value == object_value;
}

bool IntSettingDesc::IsDefaultValue(void *object) const
{
	int32_t object_value = this->Read(object);
	return this->GetDefaultValue() == object_value;
}

void IntSettingDesc::ResetToDefault(void *object) const
{
	this->Write(object, this->GetDefaultValue());
}

std::string StringSettingDesc::FormatValue(const void *object) const
{
	const std::string &str = this->Read(object);
	switch (GetVarMemType(this->save.conv)) {
		case SLE_VAR_STR: return str;

		case SLE_VAR_STRQ:
			if (str.empty()) {
				return str;
			}
			return fmt::format("\"{}\"", str);

		default: NOT_REACHED();
	}
}

bool StringSettingDesc::IsSameValue(const IniItem *item, void *object) const
{
	/* The ini parsing removes the quotes, which are needed to retain the spaces in STRQs,
	 * so those values are always different in the parsed ini item than they should be. */
	if (GetVarMemType(this->save.conv) == SLE_VAR_STRQ) return false;

	const std::string &str = this->Read(object);
	return item->value->compare(str) == 0;
}

bool StringSettingDesc::IsDefaultValue(void *object) const
{
	const std::string &str = this->Read(object);
	return this->def == str;
}

void StringSettingDesc::ResetToDefault(void *object) const
{
	this->Write(object, this->def);
}

bool ListSettingDesc::IsSameValue(const IniItem *, void *) const
{
	/* Checking for equality is way more expensive than just writing the value. */
	return false;
}

bool ListSettingDesc::IsDefaultValue(void *) const
{
	/* Defaults of lists are often complicated, and hard to compare. */
	return false;
}

void ListSettingDesc::ResetToDefault(void *) const
{
	/* Resetting a list to default is not supported. */
	NOT_REACHED();
}

/**
 * Loads all items from a 'grpname' section into a list
 * The list parameter can be a nullptr pointer, in this case nothing will be
 * saved and a callback function should be defined that will take over the
 * list-handling and store the data itself somewhere.
 * @param ini IniFile handle to the ini file with the source data
 * @param grpname character string identifying the section-header of the ini file that will be parsed
 * @param list new list with entries of the given section
 */
static void IniLoadSettingList(IniFile &ini, std::string_view grpname, StringList &list)
{
	const IniGroup *group = ini.GetGroup(grpname);

	if (group == nullptr) return;

	list.clear();

	for (const IniItem &item : group->items) {
		if (!item.name.empty()) list.push_back(item.name);
	}
}

/**
 * Saves all items from a list into the 'grpname' section
 * The list parameter can be a nullptr pointer, in this case a callback function
 * should be defined that will provide the source data to be saved.
 * @param ini IniFile handle to the ini file where the destination data is saved
 * @param grpname character string identifying the section-header of the ini file
 * @param list pointer to an string(pointer) array that will be used as the
 *             source to be saved into the relevant ini section
 */
static void IniSaveSettingList(IniFile &ini, std::string_view grpname, StringList &list)
{
	IniGroup &group = ini.GetOrCreateGroup(grpname);
	group.Clear();

	for (const auto &iter : list) {
		group.GetOrCreateItem(iter).SetValue("");
	}
}

/**
 * Load a WindowDesc from config.
 * @param ini IniFile handle to the ini file with the source data
 * @param grpname character string identifying the section-header of the ini file that will be parsed
 * @param desc Destination WindowDesc
 */
void IniLoadWindowSettings(IniFile &ini, std::string_view grpname, WindowDesc *desc)
{
	IniLoadSettings(ini, _window_settings, grpname, desc, false);
}

/**
 * Save a WindowDesc to config.
 * @param ini IniFile handle to the ini file where the destination data is saved
 * @param grpname character string identifying the section-header of the ini file
 * @param desc Source WindowDesc
 */
void IniSaveWindowSettings(IniFile &ini, std::string_view grpname, WindowDesc *desc)
{
	IniSaveSettings(ini, _window_settings, grpname, desc, false);
}

/**
 * Check whether the setting is editable in the current gamemode.
 * @param do_command true if this is about checking a command from the server.
 * @return true if editable.
 */
bool SettingDesc::IsEditable(bool do_command) const
{
	if (!do_command && !this->flags.Test(SettingFlag::NoNetworkSync) && _networking && !_network_server && !this->flags.Test(SettingFlag::PerCompany)) return false;
	if (do_command && this->flags.Test(SettingFlag::NoNetworkSync)) return false;
	if (this->flags.Test(SettingFlag::NetworkOnly) && !_networking && _game_mode != GM_MENU) return false;
	if (this->flags.Test(SettingFlag::NoNetwork) && _networking) return false;
	if (this->flags.Test(SettingFlag::NewgameOnly) &&
			(_game_mode == GM_NORMAL ||
			(_game_mode == GM_EDITOR && !this->flags.Test(SettingFlag::SceneditToo)))) return false;
	if (this->flags.Test(SettingFlag::SceneditOnly) && _game_mode != GM_EDITOR) return false;
	return true;
}

/**
 * Return the type of the setting.
 * @return type of setting
 */
SettingType SettingDesc::GetType() const
{
	if (this->flags.Test(SettingFlag::PerCompany)) return ST_COMPANY;
	return this->flags.Test(SettingFlag::NotInSave) ? ST_CLIENT : ST_GAME;
}

/**
 * Get the setting description of this setting as an integer setting.
 * @return The integer setting description.
 */
const IntSettingDesc *SettingDesc::AsIntSetting() const
{
	assert(this->IsIntSetting());
	return static_cast<const IntSettingDesc *>(this);
}

/**
 * Get the setting description of this setting as a string setting.
 * @return The string setting description.
 */
const StringSettingDesc *SettingDesc::AsStringSetting() const
{
	assert(this->IsStringSetting());
	return static_cast<const StringSettingDesc *>(this);
}

void PrepareOldDiffCustom();
void HandleOldDiffCustom(bool savegame);


/** Checks if any settings are set to incorrect values, and sets them to correct values in that case. */
static void ValidateSettings()
{
	/* Do not allow a custom sea level with the original land generator. */
	if (_settings_newgame.game_creation.land_generator == LG_ORIGINAL &&
			_settings_newgame.difficulty.quantity_sea_lakes == CUSTOM_SEA_LEVEL_NUMBER_DIFFICULTY) {
		_settings_newgame.difficulty.quantity_sea_lakes = CUSTOM_SEA_LEVEL_MIN_PERCENTAGE;
	}
}

static void AILoadConfig(const IniFile &ini, std::string_view grpname)
{
	const IniGroup *group = ini.GetGroup(grpname);

	/* Clean any configured AI */
	for (CompanyID c = CompanyID::Begin(); c < MAX_COMPANIES; ++c) {
		AIConfig::GetConfig(c, AIConfig::SSS_FORCE_NEWGAME)->Change(std::nullopt);
	}

	/* If no group exists, return */
	if (group == nullptr) return;

	CompanyID c = CompanyID::Begin();
	for (const IniItem &item : group->items) {
		AIConfig *config = AIConfig::GetConfig(c, AIConfig::SSS_FORCE_NEWGAME);

		config->Change(item.name);
		if (!config->HasScript()) {
			if (item.name != "none") {
				Debug(script, 0, "The AI by the name '{}' was no longer found, and removed from the list.", item.name);
				continue;
			}
		}
		if (item.value.has_value()) config->StringToSettings(*item.value);
		++c;
		if (c >= MAX_COMPANIES) break;
	}
}

static void GameLoadConfig(const IniFile &ini, std::string_view grpname)
{
	const IniGroup *group = ini.GetGroup(grpname);

	/* Clean any configured GameScript */
	GameConfig::GetConfig(GameConfig::SSS_FORCE_NEWGAME)->Change(std::nullopt);

	/* If no group exists, return */
	if (group == nullptr || group->items.empty()) return;

	const IniItem &item = group->items.front();

	GameConfig *config = GameConfig::GetConfig(AIConfig::SSS_FORCE_NEWGAME);

	config->Change(item.name);
	if (!config->HasScript()) {
		if (item.name != "none") {
			Debug(script, 0, "The GameScript by the name '{}' was no longer found, and removed from the list.", item.name);
			return;
		}
	}
	if (item.value.has_value()) config->StringToSettings(*item.value);
}

/**
 * Load BaseGraphics set selection and configuration.
 */
static void GraphicsSetLoadConfig(IniFile &ini)
{
	if (const IniGroup *group = ini.GetGroup("misc"); group != nullptr) {
		/* Load old setting first. */
		if (const IniItem *item = group->GetItem("graphicsset"); item != nullptr && item->value) BaseGraphics::ini_data.name = *item->value;
	}

	if (const IniGroup *group = ini.GetGroup("graphicsset"); group != nullptr) {
		/* Load new settings. */
		if (const IniItem *item = group->GetItem("name"); item != nullptr && item->value) BaseGraphics::ini_data.name = *item->value;

		if (const IniItem *item = group->GetItem("shortname"); item != nullptr && item->value && item->value->size() == 8) {
			auto val = ParseInteger<uint32_t>(*item->value, 16);
			if (val.has_value()) {
				BaseGraphics::ini_data.shortname = std::byteswap<uint32_t>(*val);
			} else {
				ShowErrorMessage(GetEncodedString(STR_CONFIG_ERROR),
					GetEncodedString(STR_CONFIG_ERROR_INVALID_VALUE, *item->value, BaseGraphics::ini_data.name),
					WL_CRITICAL);
			}
		}

		if (const IniItem *item = group->GetItem("extra_version"); item != nullptr && item->value) {
			auto val = ParseInteger<uint32_t>(*item->value);
			if (val.has_value()) {
				BaseGraphics::ini_data.extra_version = *val;
			} else {
				ShowErrorMessage(GetEncodedString(STR_CONFIG_ERROR),
					GetEncodedString(STR_CONFIG_ERROR_INVALID_VALUE, *item->value, BaseGraphics::ini_data.name),
					WL_CRITICAL);
			}
		}

		if (const IniItem *item = group->GetItem("extra_params"); item != nullptr && item->value) {
			auto params = ParseIntList(*item->value);
			if (params.has_value()) {
				BaseGraphics::ini_data.extra_params = params.value();
			} else {
				ShowErrorMessage(GetEncodedString(STR_CONFIG_ERROR),
					GetEncodedString(STR_CONFIG_ERROR_ARRAY, BaseGraphics::ini_data.name),
					WL_CRITICAL);
			}
		}
	}
}

/**
 * Load a GRF configuration
 * @param ini       The configuration to read from.
 * @param grpname   Group name containing the configuration of the GRF.
 * @param is_static GRF is static.
 */
static GRFConfigList GRFLoadConfig(const IniFile &ini, std::string_view grpname, bool is_static)
{
	const IniGroup *group = ini.GetGroup(grpname);
	GRFConfigList list;

	if (group == nullptr) return list;

	uint num_grfs = 0;
	for (const IniItem &item : group->items) {
		std::unique_ptr<GRFConfig> c{};

		std::array<uint8_t, 4> grfid_buf;
		MD5Hash md5sum;
		std::string_view item_name = item.name;
		bool has_md5sum = false;

		/* Try reading "<grfid>|" and on success, "<md5sum>|". */
		auto grfid_pos = item_name.find("|");
		if (grfid_pos != std::string_view::npos) {
			std::string_view grfid_str = item_name.substr(0, grfid_pos);

			if (ConvertHexToBytes(grfid_str, grfid_buf)) {
				item_name = item_name.substr(grfid_pos + 1);

				auto md5sum_pos = item_name.find("|");
				if (md5sum_pos != std::string_view::npos) {
					std::string_view md5sum_str = item_name.substr(0, md5sum_pos);

					has_md5sum = ConvertHexToBytes(md5sum_str, md5sum);
					if (has_md5sum) item_name = item_name.substr(md5sum_pos + 1);
				}

				uint32_t grfid = grfid_buf[0] | (grfid_buf[1] << 8) | (grfid_buf[2] << 16) | (grfid_buf[3] << 24);
				if (has_md5sum) {
					const GRFConfig *s = FindGRFConfig(grfid, FGCM_EXACT, &md5sum);
					if (s != nullptr) c = std::make_unique<GRFConfig>(*s);
				}
				if (c == nullptr && !FioCheckFileExists(std::string(item_name), NEWGRF_DIR)) {
					const GRFConfig *s = FindGRFConfig(grfid, FGCM_NEWEST_VALID);
					if (s != nullptr) c = std::make_unique<GRFConfig>(*s);
				}
			}
		}
		std::string filename = std::string(item_name);

		if (c == nullptr) c = std::make_unique<GRFConfig>(filename);

		/* Parse parameters */
		if (item.value.has_value() && !item.value->empty()) {
			auto params = ParseIntList(*item.value);
			if (params.has_value()) {
				c->SetParams(params.value());
			} else {
				ShowErrorMessage(GetEncodedString(STR_CONFIG_ERROR),
					GetEncodedString(STR_CONFIG_ERROR_ARRAY, filename),
					WL_CRITICAL);
			}
		}

		/* Check if item is valid */
		if (!FillGRFDetails(*c, is_static) || c->flags.Test(GRFConfigFlag::Invalid)) {
			StringID reason;
			if (c->status == GCS_NOT_FOUND) {
				reason = STR_CONFIG_ERROR_INVALID_GRF_NOT_FOUND;
			} else if (c->flags.Test(GRFConfigFlag::Unsafe)) {
				reason = STR_CONFIG_ERROR_INVALID_GRF_UNSAFE;
			} else if (c->flags.Test(GRFConfigFlag::System)) {
				reason = STR_CONFIG_ERROR_INVALID_GRF_SYSTEM;
			} else if (c->flags.Test(GRFConfigFlag::Invalid)) {
				reason = STR_CONFIG_ERROR_INVALID_GRF_INCOMPATIBLE;
			} else {
				reason = STR_CONFIG_ERROR_INVALID_GRF_UNKNOWN;
			}

			ShowErrorMessage(GetEncodedString(STR_CONFIG_ERROR),
				GetEncodedString(STR_CONFIG_ERROR_INVALID_GRF, filename.empty() ? item.name : filename, reason),
				WL_CRITICAL);
			continue;
		}

		/* Check for duplicate GRFID (will also check for duplicate filenames) */
		auto found = std::ranges::find_if(list, [&c](const auto &gc) { return gc->ident.grfid == c->ident.grfid; });
		if (found != std::end(list)) {
			ShowErrorMessage(GetEncodedString(STR_CONFIG_ERROR),
				GetEncodedString(STR_CONFIG_ERROR_DUPLICATE_GRFID, c->filename, (*found)->filename),
				WL_CRITICAL);
			continue;
		}

		if (is_static) {
			/* Mark file as static to avoid saving in savegame. */
			c->flags.Set(GRFConfigFlag::Static);
		} else if (++num_grfs > NETWORK_MAX_GRF_COUNT) {
			/* Check we will not load more non-static NewGRFs than allowed. This could trigger issues for game servers. */
			ShowErrorMessage(GetEncodedString(STR_CONFIG_ERROR),
				GetEncodedString(STR_NEWGRF_ERROR_TOO_MANY_NEWGRFS_LOADED), WL_CRITICAL);
			break;
		}

		/* Add item to list */
		list.push_back(std::move(c));
	}

	return list;
}

static IniFileVersion LoadVersionFromConfig(const IniFile &ini)
{
	const IniGroup *group = ini.GetGroup("version");
	if (group == nullptr) return IFV_0;

	auto version_number = group->GetItem("ini_version");
	/* Older ini-file versions don't have this key yet. */
	if (version_number == nullptr || !version_number->value.has_value()) return IFV_0;

	uint32_t version = 0;
	std::from_chars(version_number->value->data(), version_number->value->data() + version_number->value->size(), version);

	return static_cast<IniFileVersion>(version);
}

static void AISaveConfig(IniFile &ini, std::string_view grpname)
{
	IniGroup &group = ini.GetOrCreateGroup(grpname);
	group.Clear();

	for (CompanyID c = CompanyID::Begin(); c < MAX_COMPANIES; ++c) {
		AIConfig *config = AIConfig::GetConfig(c, AIConfig::SSS_FORCE_NEWGAME);
		std::string name;
		std::string value = config->SettingsToString();

		if (config->HasScript()) {
			name = config->GetName();
		} else {
			name = "none";
		}

		group.CreateItem(name).SetValue(value);
	}
}

static void GameSaveConfig(IniFile &ini, std::string_view grpname)
{
	IniGroup &group = ini.GetOrCreateGroup(grpname);
	group.Clear();

	GameConfig *config = GameConfig::GetConfig(AIConfig::SSS_FORCE_NEWGAME);
	std::string name;
	std::string value = config->SettingsToString();

	if (config->HasScript()) {
		name = config->GetName();
	} else {
		name = "none";
	}

	group.CreateItem(name).SetValue(value);
}

/**
 * Save the version of OpenTTD to the ini file.
 * @param ini the ini to write to
 */
static void SaveVersionInConfig(IniFile &ini)
{
	IniGroup &group = ini.GetOrCreateGroup("version");
	group.GetOrCreateItem("version_string").SetValue(_openttd_revision);
	group.GetOrCreateItem("version_number").SetValue(fmt::format("{:08X}", _openttd_newgrf_version));
	group.GetOrCreateItem("ini_version").SetValue(fmt::format("{}", INIFILE_VERSION));
}

/**
 * Save BaseGraphics set selection and configuration.
 */
static void GraphicsSetSaveConfig(IniFile &ini)
{
	const GraphicsSet *used_set = BaseGraphics::GetUsedSet();
	if (used_set == nullptr) return;

	IniGroup &group = ini.GetOrCreateGroup("graphicsset");
	group.Clear();

	group.GetOrCreateItem("name").SetValue(used_set->name);
	group.GetOrCreateItem("shortname").SetValue(fmt::format("{:08X}", std::byteswap(used_set->shortname)));

	const GRFConfig *extra_cfg = used_set->GetExtraConfig();
	if (extra_cfg != nullptr && !extra_cfg->param.empty()) {
		group.GetOrCreateItem("extra_version").SetValue(fmt::format("{}", extra_cfg->version));
		group.GetOrCreateItem("extra_params").SetValue(GRFBuildParamList(*extra_cfg));
	}
}

/* Save a GRF configuration to the given group name */
static void GRFSaveConfig(IniFile &ini, std::string_view grpname, const GRFConfigList &list)
{
	IniGroup &group = ini.GetOrCreateGroup(grpname);
	group.Clear();

	for (const auto &c : list) {
		std::string key = fmt::format("{:08X}|{}|{}", std::byteswap(c->ident.grfid),
				FormatArrayAsHex(c->ident.md5sum), c->filename);
		group.GetOrCreateItem(key).SetValue(GRFBuildParamList(*c));
	}
}

/* Common handler for saving/loading variables to the configuration file */
static void HandleSettingDescs(IniFile &generic_ini, IniFile &private_ini, IniFile &secrets_ini, SettingDescProc *proc, SettingDescProcList *proc_list, bool only_startup = false)
{
	proc(generic_ini, _misc_settings, "misc", nullptr, only_startup);
#if defined(_WIN32) && !defined(DEDICATED)
	proc(generic_ini, _win32_settings, "win32", nullptr, only_startup);
#endif /* _WIN32 */

	/* The name "patches" is a fallback, as every setting should sets its own group. */

	for (auto &table : GenericSettingTables()) {
		proc(generic_ini, table, "patches", &_settings_newgame, only_startup);
	}
	for (auto &table : PrivateSettingTables()) {
		proc(private_ini, table, "patches", &_settings_newgame, only_startup);
	}
	for (auto &table : SecretSettingTables()) {
		proc(secrets_ini, table, "patches", &_settings_newgame, only_startup);
	}

	proc(generic_ini, _currency_settings, "currency", &GetCustomCurrency(), only_startup);
	proc(generic_ini, _company_settings, "company", &_settings_client.company, only_startup);

	if (!only_startup) {
		proc_list(private_ini, "server_bind_addresses", _network_bind_list);
		proc_list(private_ini, "servers", _network_host_list);
		proc_list(private_ini, "bans", _network_ban_list);
		proc_list(private_ini, "server_authorized_keys", _settings_client.network.server_authorized_keys);
		proc_list(private_ini, "rcon_authorized_keys", _settings_client.network.rcon_authorized_keys);
		proc_list(private_ini, "admin_authorized_keys", _settings_client.network.admin_authorized_keys);
	}
}

/**
 * Remove all entries from a settings table from an ini-file.
 *
 * This is only useful if those entries are moved to another file, and you
 * want to clean up what is left behind.
 *
 * @param ini The ini file to remove the entries from.
 * @param table The table to look for entries to remove.
 */
static void RemoveEntriesFromIni(IniFile &ini, const SettingTable &table)
{
	for (auto &desc : table) {
		const SettingDesc *sd = GetSettingDesc(desc);

		/* For settings.xx.yy load the settings from [xx] yy = ? */
		std::string s{ sd->GetName() };
		auto sc = s.find('.');
		if (sc == std::string::npos) continue;

		IniGroup *group = ini.GetGroup(s.substr(0, sc));
		if (group == nullptr) continue;
		s = s.substr(sc + 1);

		group->RemoveItem(s);
	}
}

/**
 * Check whether a conversion should be done, and based on what old setting information.
 *
 * To prevent errors when switching back and forth between older and newer
 * version of OpenTTD, the type of a setting is never changed. Instead, the
 * setting is renamed, and this function is used to check whether a conversion
 * between the old and new setting is required.
 *
 * This checks if the new setting doesn't exist, and if the old does.
 *
 * Doing it this way means that if you switch to an older client, the old
 * setting is used, and only on the first time starting a new client, the
 * old setting is converted to the new. After that, they are independent
 * of each other. And you can safely, without errors on either, switch
 * between old and new client.
 *
 * @param ini The ini-file to use.
 * @param group The group the setting is in.
 * @param old_var The old name of the setting.
 * @param new_var The new name of the setting.
 * @param[out] old_item The old item to base upgrading on.
 * @return Whether upgrading should happen; if false, old_item is a nullptr.
 */
bool IsConversionNeeded(const ConfigIniFile &ini, const std::string &group, const std::string &old_var, const std::string &new_var, const IniItem **old_item)
{
	*old_item = nullptr;

	const IniGroup *igroup = ini.GetGroup(group);
	/* If the group doesn't exist, there is nothing to convert. */
	if (igroup == nullptr) return false;

	const IniItem *tmp_old_item = igroup->GetItem(old_var);
	const IniItem *new_item = igroup->GetItem(new_var);

	/* If the old item doesn't exist, there is nothing to convert. */
	if (tmp_old_item == nullptr) return false;

	/* If the new item exists, it means conversion was already done. We only
	 * do the conversion the first time, and after that these settings are
	 * independent. This allows users to freely change between older and
	 * newer clients without breaking anything. */
	if (new_item != nullptr) return false;

	*old_item = tmp_old_item;
	return true;
}

/**
 * Load the values from the configuration files
 * @param startup Load the minimal amount of the configuration to "bootstrap" the blitter and such.
 */
void LoadFromConfig(bool startup)
{
	ConfigIniFile generic_ini(_config_file);
	ConfigIniFile private_ini(_private_file);
	ConfigIniFile secrets_ini(_secrets_file);
	ConfigIniFile favs_ini(_favs_file);

	if (!startup) ResetCurrencies(false); // Initialize the array of currencies, without preserving the custom one

	IniFileVersion generic_version = LoadVersionFromConfig(generic_ini);

	if (startup) {
		GraphicsSetLoadConfig(generic_ini);
	}

	/* Before the split of private/secrets, we have to look in the generic for these settings. */
	if (generic_version < IFV_PRIVATE_SECRETS) {
		HandleSettingDescs(generic_ini, generic_ini, generic_ini, IniLoadSettings, IniLoadSettingList, startup);
	} else {
		HandleSettingDescs(generic_ini, private_ini, secrets_ini, IniLoadSettings, IniLoadSettingList, startup);
	}

	/* Load basic settings only during bootstrap, load other settings not during bootstrap */
	if (!startup) {
		if (generic_version < IFV_LINKGRAPH_SECONDS) {
			_settings_newgame.linkgraph.recalc_interval *= CalendarTime::SECONDS_PER_DAY;
			_settings_newgame.linkgraph.recalc_time     *= CalendarTime::SECONDS_PER_DAY;
		}

		/* Move use_relay_service from generic_ini to private_ini. */
		if (generic_version < IFV_NETWORK_PRIVATE_SETTINGS) {
			const IniGroup *network = generic_ini.GetGroup("network");
			if (network != nullptr) {
				const IniItem *use_relay_service = network->GetItem("use_relay_service");
				if (use_relay_service != nullptr) {
					if (use_relay_service->value == "never") {
						_settings_client.network.use_relay_service = UseRelayService::Never;
					} else if (use_relay_service->value == "ask") {
						_settings_client.network.use_relay_service = UseRelayService::Ask;
					} else if (use_relay_service->value == "allow") {
						_settings_client.network.use_relay_service = UseRelayService::Allow;
					}
				}
			}
		}

		const IniItem *old_item;

		if (generic_version < IFV_GAME_TYPE && IsConversionNeeded(generic_ini, "network", "server_advertise", "server_game_type", &old_item)) {
			auto old_value = BoolSettingDesc::ParseSingleValue(*old_item->value);
			_settings_client.network.server_game_type = old_value.value_or(false) ? SERVER_GAME_TYPE_PUBLIC : SERVER_GAME_TYPE_LOCAL;
		}

		if (generic_version < IFV_AUTOSAVE_RENAME && IsConversionNeeded(generic_ini, "gui", "autosave", "autosave_interval", &old_item)) {
			static constexpr std::initializer_list<std::string_view> _old_autosave_interval{"off"sv, "monthly"sv, "quarterly"sv, "half year"sv, "yearly"sv};
			auto old_value = OneOfManySettingDesc::ParseSingleValue(*old_item->value, _old_autosave_interval).value_or(-1);

			switch (old_value) {
				case 0: _settings_client.gui.autosave_interval = 0; break;
				case 1: _settings_client.gui.autosave_interval = 10; break;
				case 2: _settings_client.gui.autosave_interval = 30; break;
				case 3: _settings_client.gui.autosave_interval = 60; break;
				case 4: _settings_client.gui.autosave_interval = 120; break;
				default: break;
			}
		}

		/* Persist the right click close option from older versions. */
		if (generic_version < IFV_RIGHT_CLICK_CLOSE && IsConversionNeeded(generic_ini, "gui", "right_mouse_wnd_close", "right_click_wnd_close", &old_item)) {
			auto old_value = BoolSettingDesc::ParseSingleValue(*old_item->value);
			_settings_client.gui.right_click_wnd_close = old_value.value_or(false) ? RightClickClose::Yes : RightClickClose::No;
		}

		_grfconfig_newgame = GRFLoadConfig(generic_ini, "newgrf", false);
		_grfconfig_static  = GRFLoadConfig(generic_ini, "newgrf-static", true);
		AILoadConfig(generic_ini, "ai_players");
		GameLoadConfig(generic_ini, "game_scripts");
		PickerLoadConfig(favs_ini);
		BadgeClassLoadConfig(favs_ini);

		PrepareOldDiffCustom();
		IniLoadSettings(generic_ini, _old_gameopt_settings, "gameopt", &_settings_newgame, false);
		HandleOldDiffCustom(false);

		ValidateSettings();
		DebugReconsiderSendRemoteMessages();

		/* Display scheduled errors */
		ScheduleErrorMessage(_settings_error_list);
		if (FindWindowById(WC_ERRMSG, 0) == nullptr) ShowFirstError();
	}
}

/** Save the values to the configuration file */
void SaveToConfig()
{
	ConfigIniFile generic_ini(_config_file);
	ConfigIniFile private_ini(_private_file);
	ConfigIniFile secrets_ini(_secrets_file);
	ConfigIniFile favs_ini(_favs_file);

	IniFileVersion generic_version = LoadVersionFromConfig(generic_ini);

	/* If we newly create the private/secrets file, add a dummy group on top
	 * just so we can add a comment before it (that is how IniFile works).
	 * This to explain what the file is about. After doing it once, never touch
	 * it again, as otherwise we might be reverting user changes. */
	if (IniGroup *group = private_ini.GetGroup("private"); group != nullptr) group->comment = "; This file possibly contains private information which can identify you as person.\n";
	if (IniGroup *group = secrets_ini.GetGroup("secrets"); group != nullptr) group->comment = "; Do not share this file with others, not even if they claim to be technical support.\n; This file contains saved passwords and other secrets that should remain private to you!\n";

	if (generic_version == IFV_0) {
		/* Remove some obsolete groups. These have all been loaded into other groups. */
		generic_ini.RemoveGroup("patches");
		generic_ini.RemoveGroup("yapf");
		generic_ini.RemoveGroup("gameopt");

		/* Remove all settings from the generic ini that are now in the private ini. */
		generic_ini.RemoveGroup("server_bind_addresses");
		generic_ini.RemoveGroup("servers");
		generic_ini.RemoveGroup("bans");
		for (auto &table : PrivateSettingTables()) {
			RemoveEntriesFromIni(generic_ini, table);
		}

		/* Remove all settings from the generic ini that are now in the secrets ini. */
		for (auto &table : SecretSettingTables()) {
			RemoveEntriesFromIni(generic_ini, table);
		}
	}

	if (generic_version < IFV_REMOVE_GENERATION_SEED) {
		IniGroup *game_creation = generic_ini.GetGroup("game_creation");
		if (game_creation != nullptr) {
			game_creation->RemoveItem("generation_seed");
		}
	}

	/* These variables are migrated from generic ini to private ini now. */
	if (generic_version < IFV_NETWORK_PRIVATE_SETTINGS) {
		IniGroup *network = generic_ini.GetGroup("network");
		if (network != nullptr) {
			network->RemoveItem("use_relay_service");
		}
	}

	HandleSettingDescs(generic_ini, private_ini, secrets_ini, IniSaveSettings, IniSaveSettingList);
	GraphicsSetSaveConfig(generic_ini);
	GRFSaveConfig(generic_ini, "newgrf", _grfconfig_newgame);
	GRFSaveConfig(generic_ini, "newgrf-static", _grfconfig_static);
	AISaveConfig(generic_ini, "ai_players");
	GameSaveConfig(generic_ini, "game_scripts");
	PickerSaveConfig(favs_ini);
	BadgeClassSaveConfig(favs_ini);

	SaveVersionInConfig(generic_ini);
	SaveVersionInConfig(private_ini);
	SaveVersionInConfig(secrets_ini);
	SaveVersionInConfig(favs_ini);

	generic_ini.SaveToDisk(_config_file);
	private_ini.SaveToDisk(_private_file);
	secrets_ini.SaveToDisk(_secrets_file);
	favs_ini.SaveToDisk(_favs_file);
}

/**
 * Get the list of known NewGrf presets.
 * @returns List of preset names.
 */
StringList GetGRFPresetList()
{
	StringList list;

	ConfigIniFile ini(_config_file);
	for (const IniGroup &group : ini.groups) {
		if (group.name.starts_with("preset-")) {
			list.push_back(group.name.substr(7));
		}
	}

	return list;
}

/**
 * Load a NewGRF configuration by preset-name.
 * @param config_name Name of the preset.
 * @return NewGRF configuration.
 * @see GetGRFPresetList
 */
GRFConfigList LoadGRFPresetFromConfig(std::string_view config_name)
{
	std::string section("preset-");
	section += config_name;

	ConfigIniFile ini(_config_file);
	GRFConfigList config = GRFLoadConfig(ini, section, false);

	return config;
}

/**
 * Save a NewGRF configuration with a preset name.
 * @param config_name Name of the preset.
 * @param config      NewGRF configuration to save.
 * @see GetGRFPresetList
 */
void SaveGRFPresetToConfig(std::string_view config_name, GRFConfigList &config)
{
	std::string section("preset-");
	section += config_name;

	ConfigIniFile ini(_config_file);
	GRFSaveConfig(ini, section, config);
	ini.SaveToDisk(_config_file);
}

/**
 * Delete a NewGRF configuration by preset name.
 * @param config_name Name of the preset.
 */
void DeleteGRFPresetFromConfig(std::string_view config_name)
{
	std::string section("preset-");
	section += config_name;

	ConfigIniFile ini(_config_file);
	ini.RemoveGroup(section);
	ini.SaveToDisk(_config_file);
}

/**
 * Handle changing a value. This performs validation of the input value and
 * calls the appropriate callbacks, and saves it when the value is changed.
 * @param object The object the setting is in.
 * @param newval The new value for the setting.
 */
void IntSettingDesc::ChangeValue(const void *object, int32_t newval) const
{
	int32_t oldval = this->Read(object);
	this->MakeValueValid(newval);
	if (this->pre_check != nullptr && !this->pre_check(newval)) return;
	if (oldval == newval) return;

	this->Write(object, newval);
	if (this->post_callback != nullptr) this->post_callback(newval);

	if (this->flags.Test(SettingFlag::NoNetwork) || this->flags.Test(SettingFlag::Sandbox)) {
		_gamelog.StartAction(GLAT_SETTING);
		_gamelog.Setting(this->GetName(), oldval, newval);
		_gamelog.StopAction();
	}

	SetWindowClassesDirty(WC_GAME_OPTIONS);
	if (this->flags.Test(SettingFlag::Sandbox)) SetWindowClassesDirty(WC_CHEATS);

	if (_save_config) SaveToConfig();
}

/**
 * Given a name of setting, return a setting description from the table.
 * @param name Name of the setting to return a setting description of.
 * @param settings Table to look in for the setting.
 * @return Pointer to the setting description of setting \a name if it can be found,
 *         \c nullptr indicates failure to obtain the description.
 */
static const SettingDesc *GetSettingFromName(std::string_view name, const SettingTable &settings)
{
	/* First check all full names */
	for (auto &desc : settings) {
		const SettingDesc *sd = GetSettingDesc(desc);
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (sd->GetName() == name) return sd;
	}

	/* Then check the shortcut variant of the name. */
	std::string short_name_suffix = std::string{ "." }.append(name);
	for (auto &desc : settings) {
		const SettingDesc *sd = GetSettingDesc(desc);
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (sd->GetName().ends_with(short_name_suffix)) return sd;
	}

	return nullptr;
}

/**
 * Get the SaveLoad for all settings in the settings table.
 * @param settings The settings table to get the SaveLoad objects from.
 * @param saveloads A vector to store the result in.
 */
void GetSaveLoadFromSettingTable(SettingTable settings, std::vector<SaveLoad> &saveloads)
{
	for (auto &desc : settings) {
		const SettingDesc *sd = GetSettingDesc(desc);
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		saveloads.push_back(sd->save);
	}
}

/**
 * Create a single table with all settings that should be stored/loaded
 * in the savegame.
 */
SettingTable GetSaveLoadSettingTable()
{
	static const SettingTable saveload_settings_tables[] = {
		_difficulty_settings,
		_economy_settings,
		_game_settings,
		_linkgraph_settings,
		_locale_settings,
		_pathfinding_settings,
		_script_settings,
		_world_settings,
	};
	static std::vector<SettingVariant> settings_table;

	if (settings_table.empty()) {
		for (auto &saveload_settings_table : saveload_settings_tables) {
			for (auto &saveload_setting : saveload_settings_table) {
				settings_table.push_back(saveload_setting);
			}
		}
	}

	return settings_table;
}

/**
 * Given a name of setting, return a company setting description of it.
 * @param name  Name of the company setting to return a setting description of.
 * @return Pointer to the setting description of setting \a name if it can be found,
 *         \c nullptr indicates failure to obtain the description.
 */
static const SettingDesc *GetCompanySettingFromName(std::string_view name)
{
	static const std::string_view company_prefix = "company.";
	if (name.starts_with(company_prefix)) name.remove_prefix(company_prefix.size());
	return GetSettingFromName(name, _company_settings);
}

/**
 * Given a name of any setting, return any setting description of it.
 * @param name  Name of the setting to return a setting description of.
 * @return Pointer to the setting description of setting \a name if it can be found,
 *         \c nullptr indicates failure to obtain the description.
 */
const SettingDesc *GetSettingFromName(std::string_view name)
{
	for (auto &table : GenericSettingTables()) {
		auto sd = GetSettingFromName(name, table);
		if (sd != nullptr) return sd;
	}
	for (auto &table : PrivateSettingTables()) {
		auto sd = GetSettingFromName(name, table);
		if (sd != nullptr) return sd;
	}
	for (auto &table : SecretSettingTables()) {
		auto sd = GetSettingFromName(name, table);
		if (sd != nullptr) return sd;
	}

	return GetCompanySettingFromName(name);
}

/**
 * Get a collection of settings matching a custom filter.
 * @param func Function to filter each setting.
 * @returns Vector containing the list of collections.
 */
std::vector<const SettingDesc *> GetFilteredSettingCollection(std::function<bool(const SettingDesc &desc)> func)
{
	std::vector<const SettingDesc *> collection;

	for (const auto &table : GenericSettingTables()) {
		for (const auto &desc : table) {
			const auto sd = GetSettingDesc(desc);
			if (!func(*sd)) continue;

			collection.push_back(sd);
		}
	}

	return collection;
}

/**
 * Network-safe changing of settings (server-only).
 * @param flags operation to perform
 * @param name the name of the setting to change
 * @param value the new value for the setting
 * The new value is properly clamped to its minimum/maximum when setting
 * @return the cost of this operation or an error
 * @see _settings
 */
CommandCost CmdChangeSetting(DoCommandFlags flags, const std::string &name, int32_t value)
{
	if (name.empty()) return CMD_ERROR;
	const SettingDesc *sd = GetSettingFromName(name);

	if (sd == nullptr) return CMD_ERROR;
	if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) return CMD_ERROR;
	if (!sd->IsIntSetting()) return CMD_ERROR;

	if (!sd->IsEditable(true)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		sd->AsIntSetting()->ChangeValue(&GetGameSettings(), value);
	}

	return CommandCost();
}

/**
 * Change one of the per-company settings.
 * @param flags operation to perform
 * @param name the name of the company setting to change
 * @param value the new value for the setting
 * The new value is properly clamped to its minimum/maximum when setting
 * @return the cost of this operation or an error
 */
CommandCost CmdChangeCompanySetting(DoCommandFlags flags, const std::string &name, int32_t value)
{
	if (name.empty()) return CMD_ERROR;
	const SettingDesc *sd = GetCompanySettingFromName(name);

	if (sd == nullptr) return CMD_ERROR;
	if (!sd->IsIntSetting()) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		sd->AsIntSetting()->ChangeValue(&Company::Get(_current_company)->settings, value);
	}

	return CommandCost();
}

/**
 * Top function to save the new value of an element of the Settings struct
 * @param index offset in the SettingDesc array of the Settings struct which
 * identifies the setting member we want to change
 * @param value new value of the setting
 * @param force_newgame force the newgame settings
 */
bool SetSettingValue(const IntSettingDesc *sd, int32_t value, bool force_newgame)
{
	const IntSettingDesc *setting = sd->AsIntSetting();
	if (setting->flags.Test(SettingFlag::PerCompany)) {
		if (Company::IsValidID(_local_company) && _game_mode != GM_MENU) {
			return Command<Commands::ChangeCompanySetting>::Post(setting->GetName(), value);
		}

		setting->ChangeValue(&_settings_client.company, value);
		return true;
	}

	/* If an item is company-based, we do not send it over the network
	 * (if any) to change. Also *hack*hack* we update the _newgame version
	 * of settings because changing a company-based setting in a game also
	 * changes its defaults. At least that is the convention we have chosen */
	if (setting->flags.Test(SettingFlag::NoNetworkSync)) {
		if (_game_mode != GM_MENU) {
			setting->ChangeValue(&_settings_newgame, value);
		}
		setting->ChangeValue(&GetGameSettings(), value);
		return true;
	}

	if (force_newgame) {
		setting->ChangeValue(&_settings_newgame, value);
		return true;
	}

	/* send non-company-based settings over the network */
	if (!_networking || (_networking && _network_server)) {
		return Command<Commands::ChangeSetting>::Post(setting->GetName(), value);
	}
	return false;
}

/**
 * Set the company settings for a new company to their default values.
 */
void SetDefaultCompanySettings(CompanyID cid)
{
	Company *c = Company::Get(cid);
	AutoRestoreBackup backup(_current_company, cid);
	for (auto &desc : _company_settings) {
		const IntSettingDesc *int_setting = GetSettingDesc(desc)->AsIntSetting();
		int_setting->MakeValueValidAndWrite(&c->settings, int_setting->GetDefaultValue());
	}
}

/**
 * Sync all company settings in a multiplayer game.
 */
void SyncCompanySettings()
{
	const void *old_object = &Company::Get(_current_company)->settings;
	const void *new_object = &_settings_client.company;
	for (auto &desc : _company_settings) {
		const SettingDesc *sd = GetSettingDesc(desc);
		uint32_t old_value = (uint32_t)sd->AsIntSetting()->Read(old_object);
		uint32_t new_value = (uint32_t)sd->AsIntSetting()->Read(new_object);
		/*
		 * This is called from a command, and since it contains local configuration information
		 * that the rest of the clients do not know about, we need to circumvent the normal ::Post
		 * local command validation and immediately send the command to the server.
		 */
		if (old_value != new_value) Command<Commands::ChangeCompanySetting>::SendNet(STR_NULL, _local_company, sd->GetName(), new_value);
	}
}

/**
 * Set a setting value with a string.
 * @param sd the setting to change.
 * @param value the value to write
 * @param force_newgame force the newgame settings
 * @note Strings WILL NOT be synced over the network
 */
bool SetSettingValue(const StringSettingDesc *sd, std::string_view value, bool force_newgame)
{
	assert(sd->flags.Test(SettingFlag::NoNetworkSync));

	if (GetVarMemType(sd->save.conv) == SLE_VAR_STRQ && value == "(null)") {
		value = {};
	}

	const void *object = (_game_mode == GM_MENU || force_newgame) ? &_settings_newgame : &_settings_game;
	sd->AsStringSetting()->ChangeValue(object, std::string{value});
	return true;
}

/**
 * Handle changing a string value. This performs validation of the input value
 * and calls the appropriate callbacks, and saves it when the value is changed.
 * @param object The object the setting is in.
 * @param newval The new value for the setting.
 */
void StringSettingDesc::ChangeValue(const void *object, std::string &&newval) const
{
	this->MakeValueValid(newval);
	if (this->pre_check != nullptr && !this->pre_check(newval)) return;

	this->Write(object, newval);
	if (this->post_callback != nullptr) this->post_callback(newval);

	if (_save_config) SaveToConfig();
}

/* Those 2 functions need to be here, else we have to make some stuff non-static
 * and besides, it is also better to keep stuff like this at the same place */
void IConsoleSetSetting(std::string_view name, std::string_view value, bool force_newgame)
{
	const SettingDesc *sd = GetSettingFromName(name);
	/* Company settings are not in "list_settings", so don't try to modify them. */
	if (sd == nullptr || sd->flags.Test(SettingFlag::PerCompany)) {
		IConsolePrint(CC_ERROR, "'{}' is an unknown setting.", name);
		return;
	}

	bool success = true;
	if (sd->IsStringSetting()) {
		success = SetSettingValue(sd->AsStringSetting(), value, force_newgame);
	} else if (sd->IsIntSetting()) {
		const IntSettingDesc *isd = sd->AsIntSetting();
		size_t val = isd->ParseValue(value);
		if (!_settings_error_list.empty()) {
			IConsolePrint(CC_ERROR, "'{}' is not a valid value for this setting.", value);
			_settings_error_list.clear();
			return;
		}
		success = SetSettingValue(isd, (int32_t)val, force_newgame);
	}

	if (!success) {
		if (_network_server) {
			IConsolePrint(CC_ERROR, "This command/variable is not available during network games.");
		} else {
			IConsolePrint(CC_ERROR, "This command/variable is only available to a network server.");
		}
	}
}

void IConsoleSetSetting(std::string_view name, int value)
{
	const SettingDesc *sd = GetSettingFromName(name);
	assert(sd != nullptr);
	SetSettingValue(sd->AsIntSetting(), value);
}

/**
 * Output value of a specific setting to the console
 * @param name  Name of the setting to output its value
 * @param force_newgame force the newgame settings
 */
void IConsoleGetSetting(std::string_view name, bool force_newgame)
{
	const SettingDesc *sd = GetSettingFromName(name);
	/* Company settings are not in "list_settings", so don't try to read them. */
	if (sd == nullptr || sd->flags.Test(SettingFlag::PerCompany)) {
		IConsolePrint(CC_ERROR, "'{}' is an unknown setting.", name);
		return;
	}

	const void *object = (_game_mode == GM_MENU || force_newgame) ? &_settings_newgame : &_settings_game;

	if (sd->IsStringSetting()) {
		IConsolePrint(CC_INFO, "Current value for '{}' is '{}'.", sd->GetName(), sd->AsStringSetting()->Read(object));
	} else if (sd->IsIntSetting()) {
		std::string value = sd->FormatValue(object);
		const IntSettingDesc *int_setting = sd->AsIntSetting();
		auto [min_val, max_val] = int_setting->GetRange();
		auto def_val = int_setting->GetDefaultValue();
		IConsolePrint(CC_INFO, "Current value for '{}' is '{}' (min: {}{}, max: {}, def: {}).",
			sd->GetName(), value, sd->flags.Test(SettingFlag::GuiZeroIsSpecial) ? "(0) " : "", min_val, max_val, def_val);
	}
}

static void IConsoleListSettingsTable(const SettingTable &table, std::string_view prefilter)
{
	for (auto &desc : table) {
		const SettingDesc *sd = GetSettingDesc(desc);
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (!prefilter.empty() && sd->GetName().find(prefilter) == std::string::npos) continue;
		IConsolePrint(CC_DEFAULT, "{} = {}", sd->GetName(), sd->FormatValue(&GetGameSettings()));
	}
}

/**
 * List all settings and their value to the console
 *
 * @param prefilter  If not \c nullptr, only list settings with names that begin with \a prefilter prefix
 */
void IConsoleListSettings(std::string_view prefilter)
{
	IConsolePrint(CC_HELP, "All settings with their current value:");

	for (auto &table : GenericSettingTables()) {
		IConsoleListSettingsTable(table, prefilter);
	}
	for (auto &table : PrivateSettingTables()) {
		IConsoleListSettingsTable(table, prefilter);
	}
	for (auto &table : SecretSettingTables()) {
		IConsoleListSettingsTable(table, prefilter);
	}

	IConsolePrint(CC_HELP, "Use 'setting' command to change a value.");
}

ScriptConfigSettings::ScriptConfigSettings()
{
	/* Instantiate here, because unique_ptr needs a complete type. */
}

ScriptConfigSettings::~ScriptConfigSettings()
{
	/* Instantiate here, because unique_ptr needs a complete type. */
}

ScriptConfigSettings::ScriptConfigSettings(const ScriptConfigSettings &other)
{
	*this = other;
}

ScriptConfigSettings &ScriptConfigSettings::operator=(const ScriptConfigSettings &other)
{
	for (CompanyID c = CompanyID::Begin(); c < MAX_COMPANIES; ++c) {
		if (other.ai[c] != nullptr) {
			this->ai[c] = std::make_unique<AIConfig>(*other.ai[c]);
		}
	}
	if (other.game != nullptr) {
		this->game = std::make_unique<GameConfig>(*other.game);
	}
	return *this;
}
