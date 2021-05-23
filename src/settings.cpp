/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file settings.cpp
 * All actions handling saving and loading of the settings/configuration goes on in this file.
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
#include <limits>
#include "currency.h"
#include "screenshot.h"
#include "network/network.h"
#include "network/network_func.h"
#include "settings_internal.h"
#include "command_func.h"
#include "console_func.h"
#include "pathfinder/pathfinder_type.h"
#include "genworld.h"
#include "train.h"
#include "news_func.h"
#include "window_func.h"
#include "sound_func.h"
#include "company_func.h"
#include "rev.h"
#if defined(WITH_FREETYPE) || defined(_WIN32) || defined(WITH_COCOA)
#include "fontcache.h"
#endif
#include "textbuf_gui.h"
#include "rail_gui.h"
#include "elrail_func.h"
#include "error.h"
#include "town.h"
#include "video/video_driver.hpp"
#include "sound/sound_driver.hpp"
#include "music/music_driver.hpp"
#include "blitter/factory.hpp"
#include "base_media_base.h"
#include "gamelog.h"
#include "settings_func.h"
#include "ini_type.h"
#include "ai/ai_config.hpp"
#include "ai/ai.hpp"
#include "game/game_config.hpp"
#include "game/game.hpp"
#include "ship.h"
#include "smallmap_gui.h"
#include "roadveh.h"
#include "fios.h"
#include "strings_func.h"
#include "vehicle_func.h"

#include "void_map.h"
#include "station_base.h"

#if defined(WITH_FREETYPE) || defined(_WIN32) || defined(WITH_COCOA)
#define HAS_TRUETYPE_FONT
#endif

#include "table/strings.h"
#include "table/settings.h"

#include "safeguards.h"

ClientSettings _settings_client;
GameSettings _settings_game;     ///< Game settings of a running game or the scenario editor.
GameSettings _settings_newgame;  ///< Game settings for new games (updated from the intro screen).
VehicleDefaultSettings _old_vds; ///< Used for loading default vehicles settings from old savegames
std::string _config_file; ///< Configuration file of OpenTTD

typedef std::list<ErrorMessageData> ErrorList;
static ErrorList _settings_error_list; ///< Errors while loading minimal settings.


typedef void SettingDescProc(IniFile *ini, const SettingTable &desc, const char *grpname, void *object, bool only_startup);
typedef void SettingDescProcList(IniFile *ini, const char *grpname, StringList &list);

static bool IsSignedVarMemType(VarType vt);

/**
 * Get the setting at the given index into the settings table.
 * @param index The index to look for.
 * @return The setting at the given index, or nullptr when the index is invalid.
 */
const SettingDesc *GetSettingDescription(uint index)
{
	if (index >= _settings.size()) return nullptr;
	return _settings.begin()[index].get();
}

/**
 * Get the setting at the given index into the company settings table.
 * @param index The index to look for.
 * @return The setting at the given index, or nullptr when the index is invalid.
 */
static const SettingDesc *GetCompanySettingDescription(uint index)
{
	if (index >= _company_settings.size()) return nullptr;
	return _company_settings.begin()[index].get();
}

/**
 * Groups in openttd.cfg that are actually lists.
 */
static const char * const _list_group_names[] = {
	"bans",
	"newgrf",
	"servers",
	"server_bind_addresses",
	nullptr
};

/**
 * Find the index value of a ONEofMANY type in a string separated by |
 * @param str the current value of the setting for which a value needs found
 * @param len length of the string
 * @param many full domain of values the ONEofMANY setting can have
 * @return the integer index of the full-list, or -1 if not found
 */
size_t OneOfManySettingDesc::ParseSingleValue(const char *str, size_t len, const std::vector<std::string> &many)
{
	/* check if it's an integer */
	if (isdigit(*str)) return strtoul(str, nullptr, 0);

	size_t idx = 0;
	for (auto one : many) {
		if (one.size() == len && strncmp(one.c_str(), str, len) == 0) return idx;
		idx++;
	}

	return (size_t)-1;
}

/**
 * Find the set-integer value MANYofMANY type in a string
 * @param many full domain of values the MANYofMANY setting can have
 * @param str the current string value of the setting, each individual
 * of separated by a whitespace,tab or | character
 * @return the 'fully' set integer, or -1 if a set is not found
 */
static size_t LookupManyOfMany(const std::vector<std::string> &many, const char *str)
{
	const char *s;
	size_t r;
	size_t res = 0;

	for (;;) {
		/* skip "whitespace" */
		while (*str == ' ' || *str == '\t' || *str == '|') str++;
		if (*str == 0) break;

		s = str;
		while (*s != 0 && *s != ' ' && *s != '\t' && *s != '|') s++;

		r = OneOfManySettingDesc::ParseSingleValue(str, s - str, many);
		if (r == (size_t)-1) return r;

		SetBit(res, (uint8)r); // value found, set it
		if (*s == 0) break;
		str = s + 1;
	}
	return res;
}

/**
 * Parse an integerlist string and set each found value
 * @param p the string to be parsed. Each element in the list is separated by a
 * comma or a space character
 * @param items pointer to the integerlist-array that will be filled with values
 * @param maxitems the maximum number of elements the integerlist-array has
 * @return returns the number of items found, or -1 on an error
 */
template<typename T>
static int ParseIntList(const char *p, T *items, int maxitems)
{
	int n = 0; // number of items read so far
	bool comma = false; // do we accept comma?

	while (*p != '\0') {
		switch (*p) {
			case ',':
				/* Do not accept multiple commas between numbers */
				if (!comma) return -1;
				comma = false;
				FALLTHROUGH;

			case ' ':
				p++;
				break;

			default: {
				if (n == maxitems) return -1; // we don't accept that many numbers
				char *end;
				unsigned long v = strtoul(p, &end, 0);
				if (p == end) return -1; // invalid character (not a number)
				if (sizeof(T) < sizeof(v)) v = Clamp<unsigned long>(v, std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
				items[n++] = v;
				p = end; // first non-number
				comma = true; // we accept comma now
				break;
			}
		}
	}

	/* If we have read comma but no number after it, fail.
	 * We have read comma when (n != 0) and comma is not allowed */
	if (n != 0 && !comma) return -1;

	return n;
}

/**
 * Load parsed string-values into an integer-array (intlist)
 * @param str the string that contains the values (and will be parsed)
 * @param array pointer to the integer-arrays that will be filled
 * @param nelems the number of elements the array holds. Maximum is 64 elements
 * @param type the type of elements the array holds (eg INT8, UINT16, etc.)
 * @return return true on success and false on error
 */
static bool LoadIntList(const char *str, void *array, int nelems, VarType type)
{
	unsigned long items[64];
	int i, nitems;

	if (str == nullptr) {
		memset(items, 0, sizeof(items));
		nitems = nelems;
	} else {
		nitems = ParseIntList(str, items, lengthof(items));
		if (nitems != nelems) return false;
	}

	switch (type) {
		case SLE_VAR_BL:
		case SLE_VAR_I8:
		case SLE_VAR_U8:
			for (i = 0; i != nitems; i++) ((byte*)array)[i] = items[i];
			break;

		case SLE_VAR_I16:
		case SLE_VAR_U16:
			for (i = 0; i != nitems; i++) ((uint16*)array)[i] = items[i];
			break;

		case SLE_VAR_I32:
		case SLE_VAR_U32:
			for (i = 0; i != nitems; i++) ((uint32*)array)[i] = items[i];
			break;

		default: NOT_REACHED();
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
void ListSettingDesc::FormatValue(char *buf, const char *last, const void *object) const
{
	const byte *p = static_cast<const byte *>(GetVariableAddress(object, &this->save));
	int i, v = 0;

	for (i = 0; i != this->save.length; i++) {
		switch (GetVarMemType(this->save.conv)) {
			case SLE_VAR_BL:
			case SLE_VAR_I8:  v = *(const   int8 *)p; p += 1; break;
			case SLE_VAR_U8:  v = *(const  uint8 *)p; p += 1; break;
			case SLE_VAR_I16: v = *(const  int16 *)p; p += 2; break;
			case SLE_VAR_U16: v = *(const uint16 *)p; p += 2; break;
			case SLE_VAR_I32: v = *(const  int32 *)p; p += 4; break;
			case SLE_VAR_U32: v = *(const uint32 *)p; p += 4; break;
			default: NOT_REACHED();
		}
		if (IsSignedVarMemType(this->save.conv)) {
			buf += seprintf(buf, last, (i == 0) ? "%d" : ",%d", v);
		} else if (this->save.conv & SLF_HEX) {
			buf += seprintf(buf, last, (i == 0) ? "0x%X" : ",0x%X", v);
		} else {
			buf += seprintf(buf, last, (i == 0) ? "%u" : ",%u", v);
		}
	}
}

char *OneOfManySettingDesc::FormatSingleValue(char *buf, const char *last, uint id) const
{
	if (id >= this->many.size()) {
		return buf + seprintf(buf, last, "%d", id);
	}
	return strecpy(buf, this->many[id].c_str(), last);
}

void OneOfManySettingDesc::FormatValue(char *buf, const char *last, const void *object) const
{
	uint id = (uint)ReadValue(GetVariableAddress(object, &this->save), this->save.conv);
	this->FormatSingleValue(buf, last, id);
}

void ManyOfManySettingDesc::FormatValue(char *buf, const char *last, const void *object) const
{
	uint bitmask = (uint)ReadValue(GetVariableAddress(object, &this->save), this->save.conv);
	uint id = 0;
	bool first = true;
	FOR_EACH_SET_BIT(id, bitmask) {
		if (!first) buf = strecpy(buf, "|", last);
		buf = this->FormatSingleValue(buf, last, id);
		first = false;
	}
}

/**
 * Convert a string representation (external) of an integer-like setting to an integer.
 * @param str Input string that will be parsed based on the type of desc.
 * @return The value from the parse string, or the default value of the setting.
 */
size_t IntSettingDesc::ParseValue(const char *str) const
{
	char *end;
	size_t val = strtoul(str, &end, 0);
	if (end == str) {
		ErrorMessageData msg(STR_CONFIG_ERROR, STR_CONFIG_ERROR_INVALID_VALUE);
		msg.SetDParamStr(0, str);
		msg.SetDParamStr(1, this->name);
		_settings_error_list.push_back(msg);
		return this->def;
	}
	if (*end != '\0') {
		ErrorMessageData msg(STR_CONFIG_ERROR, STR_CONFIG_ERROR_TRAILING_CHARACTERS);
		msg.SetDParamStr(0, this->name);
		_settings_error_list.push_back(msg);
	}
	return val;
}

size_t OneOfManySettingDesc::ParseValue(const char *str) const
{
	size_t r = OneOfManySettingDesc::ParseSingleValue(str, strlen(str), this->many);
	/* if the first attempt of conversion from string to the appropriate value fails,
		* look if we have defined a converter from old value to new value. */
	if (r == (size_t)-1 && this->many_cnvt != nullptr) r = this->many_cnvt(str);
	if (r != (size_t)-1) return r; // and here goes converted value

	ErrorMessageData msg(STR_CONFIG_ERROR, STR_CONFIG_ERROR_INVALID_VALUE);
	msg.SetDParamStr(0, str);
	msg.SetDParamStr(1, this->name);
	_settings_error_list.push_back(msg);
	return this->def;
}

size_t ManyOfManySettingDesc::ParseValue(const char *str) const
{
	size_t r = LookupManyOfMany(this->many, str);
	if (r != (size_t)-1) return r;
	ErrorMessageData msg(STR_CONFIG_ERROR, STR_CONFIG_ERROR_INVALID_VALUE);
	msg.SetDParamStr(0, str);
	msg.SetDParamStr(1, this->name);
	_settings_error_list.push_back(msg);
	return this->def;
}

size_t BoolSettingDesc::ParseValue(const char *str) const
{
	if (strcmp(str, "true") == 0 || strcmp(str, "on") == 0 || strcmp(str, "1") == 0) return true;
	if (strcmp(str, "false") == 0 || strcmp(str, "off") == 0 || strcmp(str, "0") == 0) return false;

	ErrorMessageData msg(STR_CONFIG_ERROR, STR_CONFIG_ERROR_INVALID_VALUE);
	msg.SetDParamStr(0, str);
	msg.SetDParamStr(1, this->name);
	_settings_error_list.push_back(msg);
	return this->def;
}

/**
 * Set the value of a setting and if needed clamp the value to the preset minimum and maximum.
 * @param object The object the setting is to be saved in.
 * @param val Signed version of the new value.
 */
void IntSettingDesc::Write_ValidateSetting(const void *object, int32 val) const
{
	void *ptr = GetVariableAddress(object, &this->save);

	/* We cannot know the maximum value of a bitset variable, so just have faith */
	if (this->cmd != SDT_MANYOFMANY) {
		/* We need to take special care of the uint32 type as we receive from the function
		 * a signed integer. While here also bail out on 64-bit settings as those are not
		 * supported. Unsigned 8 and 16-bit variables are safe since they fit into a signed
		 * 32-bit variable
		 * TODO: Support 64-bit settings/variables */
		switch (GetVarMemType(this->save.conv)) {
			case SLE_VAR_NULL: return;
			case SLE_VAR_BL:
			case SLE_VAR_I8:
			case SLE_VAR_U8:
			case SLE_VAR_I16:
			case SLE_VAR_U16:
			case SLE_VAR_I32: {
				/* Override the minimum value. No value below sdb->min, except special value 0 */
				if (!(this->flags & SGF_0ISDISABLED) || val != 0) {
					if (!(this->flags & SGF_MULTISTRING)) {
						/* Clamp value-type setting to its valid range */
						val = Clamp(val, this->min, this->max);
					} else if (val < this->min || val > (int32)this->max) {
						/* Reset invalid discrete setting (where different values change gameplay) to its default value */
						val = this->def;
					}
				}
				break;
			}
			case SLE_VAR_U32: {
				/* Override the minimum value. No value below sdb->min, except special value 0 */
				uint32 uval = (uint32)val;
				if (!(this->flags & SGF_0ISDISABLED) || uval != 0) {
					if (!(this->flags & SGF_MULTISTRING)) {
						/* Clamp value-type setting to its valid range */
						uval = ClampU(uval, this->min, this->max);
					} else if (uval < (uint)this->min || uval > this->max) {
						/* Reset invalid discrete setting to its default value */
						uval = (uint32)this->def;
					}
				}
				WriteValue(ptr, SLE_VAR_U32, (int64)uval);
				return;
			}
			case SLE_VAR_I64:
			case SLE_VAR_U64:
			default: NOT_REACHED();
		}
	}

	WriteValue(ptr, this->save.conv, (int64)val);
}

/**
 * Set the string value of a setting.
 * @param object The object the setting is to be saved in.
 * @param str The string to save.
 */
void StringSettingDesc::Write_ValidateSetting(const void *object, const char *str) const
{
	std::string *dst = reinterpret_cast<std::string *>(GetVariableAddress(object, &this->save));

	switch (GetVarMemType(this->save.conv)) {
		case SLE_VAR_STR:
		case SLE_VAR_STRQ:
			if (str != nullptr) {
				if (this->max_length != 0 && strlen(str) >= this->max_length) {
					/* In case a maximum length is imposed by the setting, the length
					 * includes the '\0' termination for network transfer purposes.
					 * Also ensure the string is valid after chopping of some bytes. */
					std::string stdstr(str, this->max_length - 1);
					dst->assign(str_validate(stdstr, SVS_NONE));
				} else {
					dst->assign(str);
				}
			} else {
				dst->clear();
			}
			break;

		default: NOT_REACHED();
	}
}

/**
 * Read the string from the the actual setting.
 * @param object The object the setting is to be saved in.
 * @return The value of the saved string.
 */
const std::string &StringSettingDesc::Read(const void *object) const
{
	return *reinterpret_cast<std::string *>(GetVariableAddress(object, &this->save));
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
static void IniLoadSettings(IniFile *ini, const SettingTable &settings_table, const char *grpname, void *object, bool only_startup)
{
	IniGroup *group;
	IniGroup *group_def = ini->GetGroup(grpname);

	for (auto &sd : settings_table) {
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (sd->startup != only_startup) continue;

		/* For settings.xx.yy load the settings from [xx] yy = ? */
		std::string s{ sd->name };
		auto sc = s.find('.');
		if (sc != std::string::npos) {
			group = ini->GetGroup(s.substr(0, sc));
			s = s.substr(sc + 1);
		} else {
			group = group_def;
		}

		IniItem *item = group->GetItem(s, false);
		if (item == nullptr && group != group_def) {
			/* For settings.xx.yy load the settings from [settings] yy = ? in case the previous
			 * did not exist (e.g. loading old config files with a [settings] section */
			item = group_def->GetItem(s, false);
		}
		if (item == nullptr) {
			/* For settings.xx.zz.yy load the settings from [zz] yy = ? in case the previous
			 * did not exist (e.g. loading old config files with a [yapf] section */
			sc = s.find('.');
			if (sc != std::string::npos) item = ini->GetGroup(s.substr(0, sc))->GetItem(s.substr(sc + 1), false);
		}

		sd->ParseValue(item, object);
	}
}

void IntSettingDesc::ParseValue(const IniItem *item, void *object) const
{
	size_t val = (item == nullptr) ? this->def : this->ParseValue(item->value.has_value() ? item->value->c_str() : "");
	this->Write_ValidateSetting(object, (int32)val);
}

void StringSettingDesc::ParseValue(const IniItem *item, void *object) const
{
	const char *str = (item == nullptr) ? this->def : item->value.has_value() ? item->value->c_str() : nullptr;
	this->Write_ValidateSetting(object, str);
}

void ListSettingDesc::ParseValue(const IniItem *item, void *object) const
{
	const char *str = (item == nullptr) ? this->def : item->value.has_value() ? item->value->c_str() : nullptr;
	void *ptr = GetVariableAddress(object, &this->save);
	if (!LoadIntList(str, ptr, this->save.length, GetVarMemType(this->save.conv))) {
		ErrorMessageData msg(STR_CONFIG_ERROR, STR_CONFIG_ERROR_ARRAY);
		msg.SetDParamStr(0, this->name);
		_settings_error_list.push_back(msg);

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
static void IniSaveSettings(IniFile *ini, const SettingTable &settings_table, const char *grpname, void *object, bool)
{
	IniGroup *group_def = nullptr, *group;
	IniItem *item;
	char buf[512];

	for (auto &sd : settings_table) {
		/* If the setting is not saved to the configuration
		 * file, just continue with the next setting */
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (sd->save.conv & SLF_NOT_IN_CONFIG) continue;

		/* XXX - wtf is this?? (group override?) */
		std::string s{ sd->name };
		auto sc = s.find('.');
		if (sc != std::string::npos) {
			group = ini->GetGroup(s.substr(0, sc));
			s = s.substr(sc + 1);
		} else {
			if (group_def == nullptr) group_def = ini->GetGroup(grpname);
			group = group_def;
		}

		item = group->GetItem(s, true);

		if (!item->value.has_value() || !sd->IsSameValue(item, object)) {
			/* Value has changed, get the new value and put it into a buffer */
			sd->FormatValue(buf, lastof(buf), object);

			/* The value is different, that means we have to write it to the ini */
			item->value.emplace(buf);
		}
	}
}

void IntSettingDesc::FormatValue(char *buf, const char *last, const void *object) const
{
	uint32 i = (uint32)ReadValue(GetVariableAddress(object, &this->save), this->save.conv);
	seprintf(buf, last, IsSignedVarMemType(this->save.conv) ? "%d" : (this->save.conv & SLF_HEX) ? "%X" : "%u", i);
}

void BoolSettingDesc::FormatValue(char *buf, const char *last, const void *object) const
{
	bool val = ReadValue(GetVariableAddress(object, &this->save), this->save.conv) != 0;
	strecpy(buf, val ? "true" : "false", last);
}

bool IntSettingDesc::IsSameValue(const IniItem *item, void *object) const
{
	int64 item_value = this->ParseValue(item->value->c_str());
	int64 object_value = ReadValue(GetVariableAddress(object, &this->save), this->save.conv);
	return item_value == object_value;
}

void StringSettingDesc::FormatValue(char *buf, const char *last, const void *object) const
{
	const std::string &str = this->Read(object);
	switch (GetVarMemType(this->save.conv)) {
		case SLE_VAR_STR: strecpy(buf, str.c_str(), last); break;

		case SLE_VAR_STRQ:
			if (str.empty()) {
				buf[0] = '\0';
			} else {
				seprintf(buf, last, "\"%s\"", str.c_str());
			}
			break;

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

bool ListSettingDesc::IsSameValue(const IniItem *item, void *object) const
{
	/* Checking for equality is way more expensive than just writing the value. */
	return false;
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
static void IniLoadSettingList(IniFile *ini, const char *grpname, StringList &list)
{
	IniGroup *group = ini->GetGroup(grpname);

	if (group == nullptr) return;

	list.clear();

	for (const IniItem *item = group->item; item != nullptr; item = item->next) {
		if (!item->name.empty()) list.push_back(item->name);
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
static void IniSaveSettingList(IniFile *ini, const char *grpname, StringList &list)
{
	IniGroup *group = ini->GetGroup(grpname);

	if (group == nullptr) return;
	group->Clear();

	for (const auto &iter : list) {
		group->GetItem(iter.c_str(), true)->SetValue("");
	}
}

/**
 * Load a WindowDesc from config.
 * @param ini IniFile handle to the ini file with the source data
 * @param grpname character string identifying the section-header of the ini file that will be parsed
 * @param desc Destination WindowDesc
 */
void IniLoadWindowSettings(IniFile *ini, const char *grpname, void *desc)
{
	IniLoadSettings(ini, _window_settings, grpname, desc, false);
}

/**
 * Save a WindowDesc to config.
 * @param ini IniFile handle to the ini file where the destination data is saved
 * @param grpname character string identifying the section-header of the ini file
 * @param desc Source WindowDesc
 */
void IniSaveWindowSettings(IniFile *ini, const char *grpname, void *desc)
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
	if (!do_command && !(this->save.conv & SLF_NO_NETWORK_SYNC) && _networking && !_network_server && !(this->flags & SGF_PER_COMPANY)) return false;
	if ((this->flags & SGF_NETWORK_ONLY) && !_networking && _game_mode != GM_MENU) return false;
	if ((this->flags & SGF_NO_NETWORK) && _networking) return false;
	if ((this->flags & SGF_NEWGAME_ONLY) &&
			(_game_mode == GM_NORMAL ||
			(_game_mode == GM_EDITOR && !(this->flags & SGF_SCENEDIT_TOO)))) return false;
	if ((this->flags & SGF_SCENEDIT_ONLY) && _game_mode != GM_EDITOR) return false;
	return true;
}

/**
 * Return the type of the setting.
 * @return type of setting
 */
SettingType SettingDesc::GetType() const
{
	if (this->flags & SGF_PER_COMPANY) return ST_COMPANY;
	return (this->save.conv & SLF_NOT_IN_SAVE) ? ST_CLIENT : ST_GAME;
}

/**
 * Check whether this setting is an integer type setting.
 * @return True when the underlying type is an integer.
 */
bool SettingDesc::IsIntSetting() const {
	return this->cmd == SDT_BOOLX || this->cmd == SDT_NUMX || this->cmd == SDT_ONEOFMANY || this->cmd == SDT_MANYOFMANY;
}

/**
 * Check whether this setting is an string type setting.
 * @return True when the underlying type is a string.
 */
bool SettingDesc::IsStringSetting() const {
	return this->cmd == SDT_STDSTRING;
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

/* Begin - Callback Functions for the various settings. */

/** Reposition the main toolbar as the setting changed. */
static bool v_PositionMainToolbar(int32 p1)
{
	if (_game_mode != GM_MENU) PositionMainToolbar(nullptr);
	return true;
}

/** Reposition the statusbar as the setting changed. */
static bool v_PositionStatusbar(int32 p1)
{
	if (_game_mode != GM_MENU) {
		PositionStatusbar(nullptr);
		PositionNewsMessage(nullptr);
		PositionNetworkChatWindow(nullptr);
	}
	return true;
}

static bool PopulationInLabelActive(int32 p1)
{
	UpdateAllTownVirtCoords();
	return true;
}

static bool RedrawScreen(int32 p1)
{
	MarkWholeScreenDirty();
	return true;
}

/**
 * Redraw the smallmap after a colour scheme change.
 * @param p1 Callback parameter.
 * @return Always true.
 */
static bool RedrawSmallmap(int32 p1)
{
	BuildLandLegend();
	BuildOwnerLegend();
	SetWindowClassesDirty(WC_SMALLMAP);
	return true;
}

static bool InvalidateDetailsWindow(int32 p1)
{
	SetWindowClassesDirty(WC_VEHICLE_DETAILS);
	return true;
}

static bool StationSpreadChanged(int32 p1)
{
	InvalidateWindowData(WC_SELECT_STATION, 0);
	InvalidateWindowData(WC_BUILD_STATION, 0);
	return true;
}

static bool InvalidateBuildIndustryWindow(int32 p1)
{
	InvalidateWindowData(WC_BUILD_INDUSTRY, 0);
	return true;
}

static bool CloseSignalGUI(int32 p1)
{
	if (p1 == 0) {
		DeleteWindowByClass(WC_BUILD_SIGNAL);
	}
	return true;
}

static bool InvalidateTownViewWindow(int32 p1)
{
	InvalidateWindowClassesData(WC_TOWN_VIEW, p1);
	return true;
}

static bool DeleteSelectStationWindow(int32 p1)
{
	DeleteWindowById(WC_SELECT_STATION, 0);
	return true;
}

static bool UpdateConsists(int32 p1)
{
	for (Train *t : Train::Iterate()) {
		/* Update the consist of all trains so the maximum speed is set correctly. */
		if (t->IsFrontEngine() || t->IsFreeWagon()) t->ConsistChanged(CCF_TRACK);
	}
	InvalidateWindowClassesData(WC_BUILD_VEHICLE, 0);
	return true;
}

/* Check service intervals of vehicles, p1 is value of % or day based servicing */
static bool CheckInterval(int32 p1)
{
	bool update_vehicles;
	VehicleDefaultSettings *vds;
	if (_game_mode == GM_MENU || !Company::IsValidID(_current_company)) {
		vds = &_settings_client.company.vehicle;
		update_vehicles = false;
	} else {
		vds = &Company::Get(_current_company)->settings.vehicle;
		update_vehicles = true;
	}

	if (p1 != 0) {
		vds->servint_trains   = 50;
		vds->servint_roadveh  = 50;
		vds->servint_aircraft = 50;
		vds->servint_ships    = 50;
	} else {
		vds->servint_trains   = 150;
		vds->servint_roadveh  = 150;
		vds->servint_aircraft = 100;
		vds->servint_ships    = 360;
	}

	if (update_vehicles) {
		const Company *c = Company::Get(_current_company);
		for (Vehicle *v : Vehicle::Iterate()) {
			if (v->owner == _current_company && v->IsPrimaryVehicle() && !v->ServiceIntervalIsCustom()) {
				v->SetServiceInterval(CompanyServiceInterval(c, v->type));
				v->SetServiceIntervalIsPercent(p1 != 0);
			}
		}
	}

	InvalidateDetailsWindow(0);

	return true;
}

static bool UpdateInterval(VehicleType type, int32 p1)
{
	bool update_vehicles;
	VehicleDefaultSettings *vds;
	if (_game_mode == GM_MENU || !Company::IsValidID(_current_company)) {
		vds = &_settings_client.company.vehicle;
		update_vehicles = false;
	} else {
		vds = &Company::Get(_current_company)->settings.vehicle;
		update_vehicles = true;
	}

	/* Test if the interval is valid */
	uint16 interval = GetServiceIntervalClamped(p1, vds->servint_ispercent);
	if (interval != p1) return false;

	if (update_vehicles) {
		for (Vehicle *v : Vehicle::Iterate()) {
			if (v->owner == _current_company && v->type == type && v->IsPrimaryVehicle() && !v->ServiceIntervalIsCustom()) {
				v->SetServiceInterval(p1);
			}
		}
	}

	InvalidateDetailsWindow(0);

	return true;
}

static bool UpdateIntervalTrains(int32 p1)
{
	return UpdateInterval(VEH_TRAIN, p1);
}

static bool UpdateIntervalRoadVeh(int32 p1)
{
	return UpdateInterval(VEH_ROAD, p1);
}

static bool UpdateIntervalShips(int32 p1)
{
	return UpdateInterval(VEH_SHIP, p1);
}

static bool UpdateIntervalAircraft(int32 p1)
{
	return UpdateInterval(VEH_AIRCRAFT, p1);
}

static bool TrainAccelerationModelChanged(int32 p1)
{
	for (Train *t : Train::Iterate()) {
		if (t->IsFrontEngine()) {
			t->tcache.cached_max_curve_speed = t->GetCurveSpeedLimit();
			t->UpdateAcceleration();
		}
	}

	/* These windows show acceleration values only when realistic acceleration is on. They must be redrawn after a setting change. */
	SetWindowClassesDirty(WC_ENGINE_PREVIEW);
	InvalidateWindowClassesData(WC_BUILD_VEHICLE, 0);
	SetWindowClassesDirty(WC_VEHICLE_DETAILS);

	return true;
}

/**
 * This function updates the train acceleration cache after a steepness change.
 * @param p1 Callback parameter.
 * @return Always true.
 */
static bool TrainSlopeSteepnessChanged(int32 p1)
{
	for (Train *t : Train::Iterate()) {
		if (t->IsFrontEngine()) t->CargoChanged();
	}

	return true;
}

/**
 * This function updates realistic acceleration caches when the setting "Road vehicle acceleration model" is set.
 * @param p1 Callback parameter
 * @return Always true
 */
static bool RoadVehAccelerationModelChanged(int32 p1)
{
	if (_settings_game.vehicle.roadveh_acceleration_model != AM_ORIGINAL) {
		for (RoadVehicle *rv : RoadVehicle::Iterate()) {
			if (rv->IsFrontEngine()) {
				rv->CargoChanged();
			}
		}
	}

	/* These windows show acceleration values only when realistic acceleration is on. They must be redrawn after a setting change. */
	SetWindowClassesDirty(WC_ENGINE_PREVIEW);
	InvalidateWindowClassesData(WC_BUILD_VEHICLE, 0);
	SetWindowClassesDirty(WC_VEHICLE_DETAILS);

	return true;
}

/**
 * This function updates the road vehicle acceleration cache after a steepness change.
 * @param p1 Callback parameter.
 * @return Always true.
 */
static bool RoadVehSlopeSteepnessChanged(int32 p1)
{
	for (RoadVehicle *rv : RoadVehicle::Iterate()) {
		if (rv->IsFrontEngine()) rv->CargoChanged();
	}

	return true;
}

static bool DragSignalsDensityChanged(int32)
{
	InvalidateWindowData(WC_BUILD_SIGNAL, 0);

	return true;
}

static bool TownFoundingChanged(int32 p1)
{
	if (_game_mode != GM_EDITOR && _settings_game.economy.found_town == TF_FORBIDDEN) {
		DeleteWindowById(WC_FOUND_TOWN, 0);
		return true;
	}
	InvalidateWindowData(WC_FOUND_TOWN, 0);
	return true;
}

static bool InvalidateVehTimetableWindow(int32 p1)
{
	InvalidateWindowClassesData(WC_VEHICLE_TIMETABLE, VIWD_MODIFY_ORDERS);
	return true;
}

static bool ZoomMinMaxChanged(int32 p1)
{
	extern void ConstrainAllViewportsZoom();
	ConstrainAllViewportsZoom();
	GfxClearSpriteCache();
	if (_settings_client.gui.zoom_min > _gui_zoom) {
		/* Restrict GUI zoom if it is no longer available. */
		_gui_zoom = _settings_client.gui.zoom_min;
		UpdateCursorSize();
		LoadStringWidthTable();
	}
	return true;
}

static bool SpriteZoomMinChanged(int32 p1) {
	GfxClearSpriteCache();
	/* Force all sprites to redraw at the new chosen zoom level */
	MarkWholeScreenDirty();
	return true;
}

/**
 * Update any possible saveload window and delete any newgrf dialogue as
 * its widget parts might change. Reinit all windows as it allows access to the
 * newgrf debug button.
 * @param p1 unused.
 * @return Always true.
 */
static bool InvalidateNewGRFChangeWindows(int32 p1)
{
	InvalidateWindowClassesData(WC_SAVELOAD);
	DeleteWindowByClass(WC_GAME_OPTIONS);
	ReInitAllWindows(_gui_zoom_cfg);
	return true;
}

static bool InvalidateCompanyLiveryWindow(int32 p1)
{
	InvalidateWindowClassesData(WC_COMPANY_COLOUR, -1);
	ResetVehicleColourMap();
	return RedrawScreen(p1);
}

static bool InvalidateIndustryViewWindow(int32 p1)
{
	InvalidateWindowClassesData(WC_INDUSTRY_VIEW);
	return true;
}

static bool InvalidateAISettingsWindow(int32 p1)
{
	InvalidateWindowClassesData(WC_AI_SETTINGS);
	return true;
}

/**
 * Update the town authority window after a town authority setting change.
 * @param p1 Unused.
 * @return Always true.
 */
static bool RedrawTownAuthority(int32 p1)
{
	SetWindowClassesDirty(WC_TOWN_AUTHORITY);
	return true;
}

/**
 * Invalidate the company infrastructure details window after a infrastructure maintenance setting change.
 * @param p1 Unused.
 * @return Always true.
 */
static bool InvalidateCompanyInfrastructureWindow(int32 p1)
{
	InvalidateWindowClassesData(WC_COMPANY_INFRASTRUCTURE);
	return true;
}

/**
 * Invalidate the company details window after the shares setting changed.
 * @param p1 Unused.
 * @return Always true.
 */
static bool InvalidateCompanyWindow(int32 p1)
{
	InvalidateWindowClassesData(WC_COMPANY);
	return true;
}

/** Checks if any settings are set to incorrect values, and sets them to correct values in that case. */
static void ValidateSettings()
{
	/* Do not allow a custom sea level with the original land generator. */
	if (_settings_newgame.game_creation.land_generator == LG_ORIGINAL &&
			_settings_newgame.difficulty.quantity_sea_lakes == CUSTOM_SEA_LEVEL_NUMBER_DIFFICULTY) {
		_settings_newgame.difficulty.quantity_sea_lakes = CUSTOM_SEA_LEVEL_MIN_PERCENTAGE;
	}
}

static bool DifficultyNoiseChange(int32 i)
{
	if (_game_mode == GM_NORMAL) {
		UpdateAirportsNoise();
		if (_settings_game.economy.station_noise_level) {
			InvalidateWindowClassesData(WC_TOWN_VIEW, 0);
		}
	}

	return true;
}

static bool MaxNoAIsChange(int32 i)
{
	if (GetGameSettings().difficulty.max_no_competitors != 0 &&
			AI::GetInfoList()->size() == 0 &&
			(!_networking || _network_server)) {
		ShowErrorMessage(STR_WARNING_NO_SUITABLE_AI, INVALID_STRING_ID, WL_CRITICAL);
	}

	InvalidateWindowClassesData(WC_GAME_OPTIONS, 0);
	return true;
}

/**
 * Check whether the road side may be changed.
 * @param p1 unused
 * @return true if the road side may be changed.
 */
static bool CheckRoadSide(int p1)
{
	extern bool RoadVehiclesAreBuilt();
	return _game_mode == GM_MENU || !RoadVehiclesAreBuilt();
}

/**
 * Conversion callback for _gameopt_settings_game.landscape
 * It converts (or try) between old values and the new ones,
 * without losing initial setting of the user
 * @param value that was read from config file
 * @return the "hopefully" converted value
 */
static size_t ConvertLandscape(const char *value)
{
	/* try with the old values */
	static std::vector<std::string> _old_landscape_values{"normal", "hilly", "desert", "candy"};
	return OneOfManySettingDesc::ParseSingleValue(value, strlen(value), _old_landscape_values);
}

static bool CheckFreeformEdges(int32 p1)
{
	if (_game_mode == GM_MENU) return true;
	if (p1 != 0) {
		for (Ship *s : Ship::Iterate()) {
			/* Check if there is a ship on the northern border. */
			if (TileX(s->tile) == 0 || TileY(s->tile) == 0) {
				ShowErrorMessage(STR_CONFIG_SETTING_EDGES_NOT_EMPTY, INVALID_STRING_ID, WL_ERROR);
				return false;
			}
		}
		for (const BaseStation *st : BaseStation::Iterate()) {
			/* Check if there is a non-deleted buoy on the northern border. */
			if (st->IsInUse() && (TileX(st->xy) == 0 || TileY(st->xy) == 0)) {
				ShowErrorMessage(STR_CONFIG_SETTING_EDGES_NOT_EMPTY, INVALID_STRING_ID, WL_ERROR);
				return false;
			}
		}
		for (uint x = 0; x < MapSizeX(); x++) MakeVoid(TileXY(x, 0));
		for (uint y = 0; y < MapSizeY(); y++) MakeVoid(TileXY(0, y));
	} else {
		for (uint i = 0; i < MapMaxX(); i++) {
			if (TileHeight(TileXY(i, 1)) != 0) {
				ShowErrorMessage(STR_CONFIG_SETTING_EDGES_NOT_WATER, INVALID_STRING_ID, WL_ERROR);
				return false;
			}
		}
		for (uint i = 1; i < MapMaxX(); i++) {
			if (!IsTileType(TileXY(i, MapMaxY() - 1), MP_WATER) || TileHeight(TileXY(1, MapMaxY())) != 0) {
				ShowErrorMessage(STR_CONFIG_SETTING_EDGES_NOT_WATER, INVALID_STRING_ID, WL_ERROR);
				return false;
			}
		}
		for (uint i = 0; i < MapMaxY(); i++) {
			if (TileHeight(TileXY(1, i)) != 0) {
				ShowErrorMessage(STR_CONFIG_SETTING_EDGES_NOT_WATER, INVALID_STRING_ID, WL_ERROR);
				return false;
			}
		}
		for (uint i = 1; i < MapMaxY(); i++) {
			if (!IsTileType(TileXY(MapMaxX() - 1, i), MP_WATER) || TileHeight(TileXY(MapMaxX(), i)) != 0) {
				ShowErrorMessage(STR_CONFIG_SETTING_EDGES_NOT_WATER, INVALID_STRING_ID, WL_ERROR);
				return false;
			}
		}
		/* Make tiles at the border water again. */
		for (uint i = 0; i < MapMaxX(); i++) {
			SetTileHeight(TileXY(i, 0), 0);
			SetTileType(TileXY(i, 0), MP_WATER);
		}
		for (uint i = 0; i < MapMaxY(); i++) {
			SetTileHeight(TileXY(0, i), 0);
			SetTileType(TileXY(0, i), MP_WATER);
		}
	}
	MarkWholeScreenDirty();
	return true;
}

/**
 * Changing the setting "allow multiple NewGRF sets" is not allowed
 * if there are vehicles.
 */
static bool ChangeDynamicEngines(int32 p1)
{
	if (_game_mode == GM_MENU) return true;

	if (!EngineOverrideManager::ResetToCurrentNewGRFConfig()) {
		ShowErrorMessage(STR_CONFIG_SETTING_DYNAMIC_ENGINES_EXISTING_VEHICLES, INVALID_STRING_ID, WL_ERROR);
		return false;
	}

	return true;
}

static bool ChangeMaxHeightLevel(int32 p1)
{
	if (_game_mode == GM_NORMAL) return false;
	if (_game_mode != GM_EDITOR) return true;

	/* Check if at least one mountain on the map is higher than the new value.
	 * If yes, disallow the change. */
	for (TileIndex t = 0; t < MapSize(); t++) {
		if ((int32)TileHeight(t) > p1) {
			ShowErrorMessage(STR_CONFIG_SETTING_TOO_HIGH_MOUNTAIN, INVALID_STRING_ID, WL_ERROR);
			/* Return old, unchanged value */
			return false;
		}
	}

	/* The smallmap uses an index from heightlevels to colours. Trigger rebuilding it. */
	InvalidateWindowClassesData(WC_SMALLMAP, 2);

	return true;
}

static bool StationCatchmentChanged(int32 p1)
{
	Station::RecomputeCatchmentForAll();
	MarkWholeScreenDirty();
	return true;
}

static bool MaxVehiclesChanged(int32 p1)
{
	InvalidateWindowClassesData(WC_BUILD_TOOLBAR);
	MarkWholeScreenDirty();
	return true;
}

static bool InvalidateShipPathCache(int32 p1)
{
	for (Ship *s : Ship::Iterate()) {
		s->path.clear();
	}
	return true;
}

static bool UpdateClientName(int32 p1)
{
	NetworkUpdateClientName();
	return true;
}

static bool UpdateServerPassword(int32 p1)
{
	if (_settings_client.network.server_password.compare("*") == 0) {
		_settings_client.network.server_password.clear();
	}

	NetworkServerUpdateGameInfo();
	return true;
}

static bool UpdateRconPassword(int32 p1)
{
	if (_settings_client.network.rcon_password.compare("*") == 0) {
		_settings_client.network.rcon_password.clear();
	}

	return true;
}

static bool UpdateClientConfigValues(int32 p1)
{
	NetworkServerUpdateGameInfo();
	if (_network_server) NetworkServerSendConfigUpdate();

	return true;
}

/* End - Callback Functions */

/**
 * Prepare for reading and old diff_custom by zero-ing the memory.
 */
static void PrepareOldDiffCustom()
{
	memset(_old_diff_custom, 0, sizeof(_old_diff_custom));
}

/**
 * Reading of the old diff_custom array and transforming it to the new format.
 * @param savegame is it read from the config or savegame. In the latter case
 *                 we are sure there is an array; in the former case we have
 *                 to check that.
 */
static void HandleOldDiffCustom(bool savegame)
{
	uint options_to_load = GAME_DIFFICULTY_NUM - ((savegame && IsSavegameVersionBefore(SLV_4)) ? 1 : 0);

	if (!savegame) {
		/* If we did read to old_diff_custom, then at least one value must be non 0. */
		bool old_diff_custom_used = false;
		for (uint i = 0; i < options_to_load && !old_diff_custom_used; i++) {
			old_diff_custom_used = (_old_diff_custom[i] != 0);
		}

		if (!old_diff_custom_used) return;
	}

	for (uint i = 0; i < options_to_load; i++) {
		const SettingDesc *sd = GetSettingDescription(i);
		/* Skip deprecated options */
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		int32 value = (int32)((i == 4 ? 1000 : 1) * _old_diff_custom[i]);
		sd->AsIntSetting()->Write_ValidateSetting(savegame ? &_settings_game : &_settings_newgame, value);
	}
}

static void AILoadConfig(IniFile *ini, const char *grpname)
{
	IniGroup *group = ini->GetGroup(grpname);
	IniItem *item;

	/* Clean any configured AI */
	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		AIConfig::GetConfig(c, AIConfig::SSS_FORCE_NEWGAME)->Change(nullptr);
	}

	/* If no group exists, return */
	if (group == nullptr) return;

	CompanyID c = COMPANY_FIRST;
	for (item = group->item; c < MAX_COMPANIES && item != nullptr; c++, item = item->next) {
		AIConfig *config = AIConfig::GetConfig(c, AIConfig::SSS_FORCE_NEWGAME);

		config->Change(item->name.c_str());
		if (!config->HasScript()) {
			if (item->name != "none") {
				DEBUG(script, 0, "The AI by the name '%s' was no longer found, and removed from the list.", item->name.c_str());
				continue;
			}
		}
		if (item->value.has_value()) config->StringToSettings(item->value->c_str());
	}
}

static void GameLoadConfig(IniFile *ini, const char *grpname)
{
	IniGroup *group = ini->GetGroup(grpname);
	IniItem *item;

	/* Clean any configured GameScript */
	GameConfig::GetConfig(GameConfig::SSS_FORCE_NEWGAME)->Change(nullptr);

	/* If no group exists, return */
	if (group == nullptr) return;

	item = group->item;
	if (item == nullptr) return;

	GameConfig *config = GameConfig::GetConfig(AIConfig::SSS_FORCE_NEWGAME);

	config->Change(item->name.c_str());
	if (!config->HasScript()) {
		if (item->name != "none") {
			DEBUG(script, 0, "The GameScript by the name '%s' was no longer found, and removed from the list.", item->name.c_str());
			return;
		}
	}
	if (item->value.has_value()) config->StringToSettings(item->value->c_str());
}

/**
 * Convert a character to a hex nibble value, or \c -1 otherwise.
 * @param c Character to convert.
 * @return Hex value of the character, or \c -1 if not a hex digit.
 */
static int DecodeHexNibble(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c + 10 - 'A';
	if (c >= 'a' && c <= 'f') return c + 10 - 'a';
	return -1;
}

/**
 * Parse a sequence of characters (supposedly hex digits) into a sequence of bytes.
 * After the hex number should be a \c '|' character.
 * @param pos First character to convert.
 * @param[out] dest Output byte array to write the bytes.
 * @param dest_size Number of bytes in \a dest.
 * @return Whether reading was successful.
 */
static bool DecodeHexText(const char *pos, uint8 *dest, size_t dest_size)
{
	while (dest_size > 0) {
		int hi = DecodeHexNibble(pos[0]);
		int lo = (hi >= 0) ? DecodeHexNibble(pos[1]) : -1;
		if (lo < 0) return false;
		*dest++ = (hi << 4) | lo;
		pos += 2;
		dest_size--;
	}
	return *pos == '|';
}

/**
 * Load a GRF configuration
 * @param ini       The configuration to read from.
 * @param grpname   Group name containing the configuration of the GRF.
 * @param is_static GRF is static.
 */
static GRFConfig *GRFLoadConfig(IniFile *ini, const char *grpname, bool is_static)
{
	IniGroup *group = ini->GetGroup(grpname);
	IniItem *item;
	GRFConfig *first = nullptr;
	GRFConfig **curr = &first;

	if (group == nullptr) return nullptr;

	for (item = group->item; item != nullptr; item = item->next) {
		GRFConfig *c = nullptr;

		uint8 grfid_buf[4], md5sum[16];
		const char *filename = item->name.c_str();
		bool has_grfid = false;
		bool has_md5sum = false;

		/* Try reading "<grfid>|" and on success, "<md5sum>|". */
		has_grfid = DecodeHexText(filename, grfid_buf, lengthof(grfid_buf));
		if (has_grfid) {
			filename += 1 + 2 * lengthof(grfid_buf);
			has_md5sum = DecodeHexText(filename, md5sum, lengthof(md5sum));
			if (has_md5sum) filename += 1 + 2 * lengthof(md5sum);

			uint32 grfid = grfid_buf[0] | (grfid_buf[1] << 8) | (grfid_buf[2] << 16) | (grfid_buf[3] << 24);
			if (has_md5sum) {
				const GRFConfig *s = FindGRFConfig(grfid, FGCM_EXACT, md5sum);
				if (s != nullptr) c = new GRFConfig(*s);
			}
			if (c == nullptr && !FioCheckFileExists(filename, NEWGRF_DIR)) {
				const GRFConfig *s = FindGRFConfig(grfid, FGCM_NEWEST_VALID);
				if (s != nullptr) c = new GRFConfig(*s);
			}
		}
		if (c == nullptr) c = new GRFConfig(filename);

		/* Parse parameters */
		if (item->value.has_value() && !item->value->empty()) {
			int count = ParseIntList(item->value->c_str(), c->param, lengthof(c->param));
			if (count < 0) {
				SetDParamStr(0, filename);
				ShowErrorMessage(STR_CONFIG_ERROR, STR_CONFIG_ERROR_ARRAY, WL_CRITICAL);
				count = 0;
			}
			c->num_params = count;
		}

		/* Check if item is valid */
		if (!FillGRFDetails(c, is_static) || HasBit(c->flags, GCF_INVALID)) {
			if (c->status == GCS_NOT_FOUND) {
				SetDParam(1, STR_CONFIG_ERROR_INVALID_GRF_NOT_FOUND);
			} else if (HasBit(c->flags, GCF_UNSAFE)) {
				SetDParam(1, STR_CONFIG_ERROR_INVALID_GRF_UNSAFE);
			} else if (HasBit(c->flags, GCF_SYSTEM)) {
				SetDParam(1, STR_CONFIG_ERROR_INVALID_GRF_SYSTEM);
			} else if (HasBit(c->flags, GCF_INVALID)) {
				SetDParam(1, STR_CONFIG_ERROR_INVALID_GRF_INCOMPATIBLE);
			} else {
				SetDParam(1, STR_CONFIG_ERROR_INVALID_GRF_UNKNOWN);
			}

			SetDParamStr(0, StrEmpty(filename) ? item->name : filename);
			ShowErrorMessage(STR_CONFIG_ERROR, STR_CONFIG_ERROR_INVALID_GRF, WL_CRITICAL);
			delete c;
			continue;
		}

		/* Check for duplicate GRFID (will also check for duplicate filenames) */
		bool duplicate = false;
		for (const GRFConfig *gc = first; gc != nullptr; gc = gc->next) {
			if (gc->ident.grfid == c->ident.grfid) {
				SetDParamStr(0, c->filename);
				SetDParamStr(1, gc->filename);
				ShowErrorMessage(STR_CONFIG_ERROR, STR_CONFIG_ERROR_DUPLICATE_GRFID, WL_CRITICAL);
				duplicate = true;
				break;
			}
		}
		if (duplicate) {
			delete c;
			continue;
		}

		/* Mark file as static to avoid saving in savegame. */
		if (is_static) SetBit(c->flags, GCF_STATIC);

		/* Add item to list */
		*curr = c;
		curr = &c->next;
	}

	return first;
}

static void AISaveConfig(IniFile *ini, const char *grpname)
{
	IniGroup *group = ini->GetGroup(grpname);

	if (group == nullptr) return;
	group->Clear();

	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		AIConfig *config = AIConfig::GetConfig(c, AIConfig::SSS_FORCE_NEWGAME);
		const char *name;
		std::string value = config->SettingsToString();

		if (config->HasScript()) {
			name = config->GetName();
		} else {
			name = "none";
		}

		IniItem *item = new IniItem(group, name);
		item->SetValue(value);
	}
}

static void GameSaveConfig(IniFile *ini, const char *grpname)
{
	IniGroup *group = ini->GetGroup(grpname);

	if (group == nullptr) return;
	group->Clear();

	GameConfig *config = GameConfig::GetConfig(AIConfig::SSS_FORCE_NEWGAME);
	const char *name;
	std::string value = config->SettingsToString();

	if (config->HasScript()) {
		name = config->GetName();
	} else {
		name = "none";
	}

	IniItem *item = new IniItem(group, name);
	item->SetValue(value);
}

/**
 * Save the version of OpenTTD to the ini file.
 * @param ini the ini to write to
 */
static void SaveVersionInConfig(IniFile *ini)
{
	IniGroup *group = ini->GetGroup("version");

	char version[9];
	seprintf(version, lastof(version), "%08X", _openttd_newgrf_version);

	const char * const versions[][2] = {
		{ "version_string", _openttd_revision },
		{ "version_number", version }
	};

	for (uint i = 0; i < lengthof(versions); i++) {
		group->GetItem(versions[i][0], true)->SetValue(versions[i][1]);
	}
}

/* Save a GRF configuration to the given group name */
static void GRFSaveConfig(IniFile *ini, const char *grpname, const GRFConfig *list)
{
	ini->RemoveGroup(grpname);
	IniGroup *group = ini->GetGroup(grpname);
	const GRFConfig *c;

	for (c = list; c != nullptr; c = c->next) {
		/* Hex grfid (4 bytes in nibbles), "|", hex md5sum (16 bytes in nibbles), "|", file system path. */
		char key[4 * 2 + 1 + 16 * 2 + 1 + MAX_PATH];
		char params[512];
		GRFBuildParamList(params, c, lastof(params));

		char *pos = key + seprintf(key, lastof(key), "%08X|", BSWAP32(c->ident.grfid));
		pos = md5sumToString(pos, lastof(key), c->ident.md5sum);
		seprintf(pos, lastof(key), "|%s", c->filename);
		group->GetItem(key, true)->SetValue(params);
	}
}

/* Common handler for saving/loading variables to the configuration file */
static void HandleSettingDescs(IniFile *ini, SettingDescProc *proc, SettingDescProcList *proc_list, bool only_startup = false)
{
	proc(ini, _misc_settings,    "misc",  nullptr, only_startup);
#if defined(_WIN32) && !defined(DEDICATED)
	proc(ini, _win32_settings,   "win32", nullptr, only_startup);
#endif /* _WIN32 */

	proc(ini, _settings,         "patches",  &_settings_newgame, only_startup);
	proc(ini, _currency_settings,"currency", &_custom_currency, only_startup);
	proc(ini, _company_settings, "company",  &_settings_client.company, only_startup);

	if (!only_startup) {
		proc_list(ini, "server_bind_addresses", _network_bind_list);
		proc_list(ini, "servers", _network_host_list);
		proc_list(ini, "bans",    _network_ban_list);
	}
}

static IniFile *IniLoadConfig()
{
	IniFile *ini = new IniFile(_list_group_names);
	ini->LoadFromDisk(_config_file, NO_DIRECTORY);
	return ini;
}

/**
 * Load the values from the configuration files
 * @param startup Load the minimal amount of the configuration to "bootstrap" the blitter and such.
 */
void LoadFromConfig(bool startup)
{
	IniFile *ini = IniLoadConfig();
	if (!startup) ResetCurrencies(false); // Initialize the array of currencies, without preserving the custom one

	/* Load basic settings only during bootstrap, load other settings not during bootstrap */
	HandleSettingDescs(ini, IniLoadSettings, IniLoadSettingList, startup);

	if (!startup) {
		_grfconfig_newgame = GRFLoadConfig(ini, "newgrf", false);
		_grfconfig_static  = GRFLoadConfig(ini, "newgrf-static", true);
		AILoadConfig(ini, "ai_players");
		GameLoadConfig(ini, "game_scripts");

		PrepareOldDiffCustom();
		IniLoadSettings(ini, _gameopt_settings, "gameopt", &_settings_newgame, false);
		HandleOldDiffCustom(false);

		ValidateSettings();

		/* Display scheduled errors */
		extern void ScheduleErrorMessage(ErrorList &datas);
		ScheduleErrorMessage(_settings_error_list);
		if (FindWindowById(WC_ERRMSG, 0) == nullptr) ShowFirstError();
	}

	delete ini;
}

/** Save the values to the configuration file */
void SaveToConfig()
{
	IniFile *ini = IniLoadConfig();

	/* Remove some obsolete groups. These have all been loaded into other groups. */
	ini->RemoveGroup("patches");
	ini->RemoveGroup("yapf");
	ini->RemoveGroup("gameopt");

	HandleSettingDescs(ini, IniSaveSettings, IniSaveSettingList);
	GRFSaveConfig(ini, "newgrf", _grfconfig_newgame);
	GRFSaveConfig(ini, "newgrf-static", _grfconfig_static);
	AISaveConfig(ini, "ai_players");
	GameSaveConfig(ini, "game_scripts");
	SaveVersionInConfig(ini);
	ini->SaveToDisk(_config_file);
	delete ini;
}

/**
 * Get the list of known NewGrf presets.
 * @returns List of preset names.
 */
StringList GetGRFPresetList()
{
	StringList list;

	std::unique_ptr<IniFile> ini(IniLoadConfig());
	for (IniGroup *group = ini->group; group != nullptr; group = group->next) {
		if (group->name.compare(0, 7, "preset-") == 0) {
			list.push_back(group->name.substr(7));
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
GRFConfig *LoadGRFPresetFromConfig(const char *config_name)
{
	size_t len = strlen(config_name) + 8;
	char *section = (char*)alloca(len);
	seprintf(section, section + len - 1, "preset-%s", config_name);

	IniFile *ini = IniLoadConfig();
	GRFConfig *config = GRFLoadConfig(ini, section, false);
	delete ini;

	return config;
}

/**
 * Save a NewGRF configuration with a preset name.
 * @param config_name Name of the preset.
 * @param config      NewGRF configuration to save.
 * @see GetGRFPresetList
 */
void SaveGRFPresetToConfig(const char *config_name, GRFConfig *config)
{
	size_t len = strlen(config_name) + 8;
	char *section = (char*)alloca(len);
	seprintf(section, section + len - 1, "preset-%s", config_name);

	IniFile *ini = IniLoadConfig();
	GRFSaveConfig(ini, section, config);
	ini->SaveToDisk(_config_file);
	delete ini;
}

/**
 * Delete a NewGRF configuration by preset name.
 * @param config_name Name of the preset.
 */
void DeleteGRFPresetFromConfig(const char *config_name)
{
	size_t len = strlen(config_name) + 8;
	char *section = (char*)alloca(len);
	seprintf(section, section + len - 1, "preset-%s", config_name);

	IniFile *ini = IniLoadConfig();
	ini->RemoveGroup(section);
	ini->SaveToDisk(_config_file);
	delete ini;
}

/**
 * Handle changing a value. This performs validation of the input value and
 * calls the appropriate callbacks, and saves it when the value is changed.
 * @param object The object the setting is in.
 * @param newval The new value for the setting.
 */
void IntSettingDesc::ChangeValue(const void *object, int32 newval) const
{
	void *var = GetVariableAddress(object, &this->save);

	int32 oldval = (int32)ReadValue(var, this->save.conv);

	this->Write_ValidateSetting(object, newval);
	newval = (int32)ReadValue(var, this->save.conv);

	if (oldval == newval) return;

	if (this->proc != nullptr && !this->proc(newval)) {
		/* The change was not allowed, so revert. */
		WriteValue(var, this->save.conv, (int64)oldval);
		return;
	}

	if (this->flags & SGF_NO_NETWORK) {
		GamelogStartAction(GLAT_SETTING);
		GamelogSetting(this->name, oldval, newval);
		GamelogStopAction();
	}

	SetWindowClassesDirty(WC_GAME_OPTIONS);

	if (_save_config) SaveToConfig();
}

/**
 * Network-safe changing of settings (server-only).
 * @param tile unused
 * @param flags operation to perform
 * @param p1 the index of the setting in the SettingDesc array which identifies it
 * @param p2 the new value for the setting
 * The new value is properly clamped to its minimum/maximum when setting
 * @param text unused
 * @return the cost of this operation or an error
 * @see _settings
 */
CommandCost CmdChangeSetting(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	const SettingDesc *sd = GetSettingDescription(p1);

	if (sd == nullptr) return CMD_ERROR;
	if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) return CMD_ERROR;
	if (!sd->IsIntSetting()) return CMD_ERROR;

	if (!sd->IsEditable(true)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		sd->AsIntSetting()->ChangeValue(&GetGameSettings(), p2);
	}

	return CommandCost();
}

/**
 * Change one of the per-company settings.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 the index of the setting in the _company_settings array which identifies it
 * @param p2 the new value for the setting
 * The new value is properly clamped to its minimum/maximum when setting
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdChangeCompanySetting(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	const SettingDesc *sd = GetCompanySettingDescription(p1);
	if (sd == nullptr) return CMD_ERROR;
	if (!sd->IsIntSetting()) return CMD_ERROR;

	if (flags & DC_EXEC) {
		sd->AsIntSetting()->ChangeValue(&Company::Get(_current_company)->settings, p2);
	}

	return CommandCost();
}

/**
 * Get the index of the given setting in the setting table.
 * @param settings The settings to look through.
 * @param setting The setting to look for.
 * @return The index, or UINT32_MAX when it has not been found.
 */
static uint GetSettingIndex(const SettingTable &settings, const SettingDesc *setting)
{
	uint index = 0;
	for (auto &sd : settings) {
		if (sd.get() == setting) return index;
		index++;
	}
	return UINT32_MAX;
}

/**
 * Get the index of the setting with this description.
 * @param sd the setting to get the index for.
 * @return the index of the setting to be used for CMD_CHANGE_SETTING.
 */
uint GetSettingIndex(const SettingDesc *sd)
{
	assert(sd != nullptr && (sd->flags & SGF_PER_COMPANY) == 0);
	return GetSettingIndex(_settings, sd);
}

/**
 * Get the index of the company setting with this description.
 * @param sd the setting to get the index for.
 * @return the index of the setting to be used for CMD_CHANGE_COMPANY_SETTING.
 */
static uint GetCompanySettingIndex(const SettingDesc *sd)
{
	assert(sd != nullptr && (sd->flags & SGF_PER_COMPANY) != 0);
	return GetSettingIndex(_company_settings, sd);
}

/**
 * Top function to save the new value of an element of the Settings struct
 * @param index offset in the SettingDesc array of the Settings struct which
 * identifies the setting member we want to change
 * @param value new value of the setting
 * @param force_newgame force the newgame settings
 */
bool SetSettingValue(const IntSettingDesc *sd, int32 value, bool force_newgame)
{
	const IntSettingDesc *setting = sd->AsIntSetting();
	if ((setting->flags & SGF_PER_COMPANY) != 0) {
		if (Company::IsValidID(_local_company) && _game_mode != GM_MENU) {
			return DoCommandP(0, GetCompanySettingIndex(setting), value, CMD_CHANGE_COMPANY_SETTING);
		}

		setting->ChangeValue(&_settings_client.company, value);
		return true;
	}

	/* If an item is company-based, we do not send it over the network
	 * (if any) to change. Also *hack*hack* we update the _newgame version
	 * of settings because changing a company-based setting in a game also
	 * changes its defaults. At least that is the convention we have chosen */
	if (setting->save.conv & SLF_NO_NETWORK_SYNC) {
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
		return DoCommandP(0, GetSettingIndex(setting), value, CMD_CHANGE_SETTING);
	}
	return false;
}

/**
 * Set the company settings for a new company to their default values.
 */
void SetDefaultCompanySettings(CompanyID cid)
{
	Company *c = Company::Get(cid);
	for (auto &sd : _company_settings) {
		const IntSettingDesc *int_setting = sd->AsIntSetting();
		int_setting->Write_ValidateSetting(&c->settings, int_setting->def);
	}
}

/**
 * Sync all company settings in a multiplayer game.
 */
void SyncCompanySettings()
{
	uint i = 0;
	for (auto &sd : _company_settings) {
		const void *old_var = GetVariableAddress(&Company::Get(_current_company)->settings, &sd->save);
		const void *new_var = GetVariableAddress(&_settings_client.company, &sd->save);
		uint32 old_value = (uint32)ReadValue(old_var, sd->save.conv);
		uint32 new_value = (uint32)ReadValue(new_var, sd->save.conv);
		if (old_value != new_value) NetworkSendCommand(0, i, new_value, CMD_CHANGE_COMPANY_SETTING, nullptr, nullptr, _local_company);
		i++;
	}
}

/**
 * Get the index in the _company_settings array of a setting
 * @param name The name of the setting
 * @return The index in the _company_settings array
 */
uint GetCompanySettingIndex(const char *name)
{
	return GetCompanySettingIndex(GetSettingFromName(name));
}

/**
 * Set a setting value with a string.
 * @param sd the setting to change.
 * @param value the value to write
 * @param force_newgame force the newgame settings
 * @note Strings WILL NOT be synced over the network
 */
bool SetSettingValue(const StringSettingDesc *sd, const char *value, bool force_newgame)
{
	assert(sd->save.conv & SLF_NO_NETWORK_SYNC);

	if (GetVarMemType(sd->save.conv) == SLE_VAR_STRQ && strcmp(value, "(null)") == 0) {
		value = nullptr;
	}

	const void *object = (_game_mode == GM_MENU || force_newgame) ? &_settings_newgame : &_settings_game;
	sd->AsStringSetting()->ChangeValue(object, value);
	return true;
}

/**
 * Handle changing a string value. This performs validation of the input value
 * and calls the appropriate callbacks, and saves it when the value is changed.
 * @param object The object the setting is in.
 * @param newval The new value for the setting.
 */
void StringSettingDesc::ChangeValue(const void *object, const char *newval) const
{
	this->Write_ValidateSetting(object, newval);
	if (this->proc != nullptr) this->proc(0);

	if (_save_config) SaveToConfig();
}

/**
 * Given a name of setting, return a setting description of it.
 * @param name  Name of the setting to return a setting description of
 * @param i     Pointer to an integer that will contain the index of the setting after the call, if it is successful.
 * @return Pointer to the setting description of setting \a name if it can be found,
 *         \c nullptr indicates failure to obtain the description
 */
const SettingDesc *GetSettingFromName(const char *name)
{
	/* First check all full names */
	for (auto &sd : _settings) {
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (strcmp(sd->name, name) == 0) return sd.get();
	}

	/* Then check the shortcut variant of the name. */
	for (auto &sd : _settings) {
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		const char *short_name = strchr(sd->name, '.');
		if (short_name != nullptr) {
			short_name++;
			if (strcmp(short_name, name) == 0) return sd.get();
		}
	}

	if (strncmp(name, "company.", 8) == 0) name += 8;
	/* And finally the company-based settings */
	for (auto &sd : _company_settings) {
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (strcmp(sd->name, name) == 0) return sd.get();
	}

	return nullptr;
}

/* Those 2 functions need to be here, else we have to make some stuff non-static
 * and besides, it is also better to keep stuff like this at the same place */
void IConsoleSetSetting(const char *name, const char *value, bool force_newgame)
{
	const SettingDesc *sd = GetSettingFromName(name);

	if (sd == nullptr) {
		IConsolePrintF(CC_WARNING, "'%s' is an unknown setting.", name);
		return;
	}

	bool success;
	if (sd->cmd == SDT_STDSTRING) {
		success = SetSettingValue(sd->AsStringSetting(), value, force_newgame);
	} else {
		uint32 val;
		extern bool GetArgumentInteger(uint32 *value, const char *arg);
		success = GetArgumentInteger(&val, value);
		if (!success) {
			IConsolePrintF(CC_ERROR, "'%s' is not an integer.", value);
			return;
		}

		success = SetSettingValue(sd->AsIntSetting(), val, force_newgame);
	}

	if (!success) {
		if (_network_server) {
			IConsoleError("This command/variable is not available during network games.");
		} else {
			IConsoleError("This command/variable is only available to a network server.");
		}
	}
}

void IConsoleSetSetting(const char *name, int value)
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
void IConsoleGetSetting(const char *name, bool force_newgame)
{
	const SettingDesc *sd = GetSettingFromName(name);
	if (sd == nullptr) {
		IConsolePrintF(CC_WARNING, "'%s' is an unknown setting.", name);
		return;
	}

	const void *object = (_game_mode == GM_MENU || force_newgame) ? &_settings_newgame : &_settings_game;

	if (sd->cmd == SDT_STDSTRING) {
		const void *ptr = GetVariableAddress(object, &sd->save);
		IConsolePrintF(CC_WARNING, "Current value for '%s' is: '%s'", name, reinterpret_cast<const std::string *>(ptr)->c_str());
	} else if (sd->IsIntSetting()) {
		char value[20];
		sd->FormatValue(value, lastof(value), object);
		const IntSettingDesc *int_setting = sd->AsIntSetting();
		IConsolePrintF(CC_WARNING, "Current value for '%s' is: '%s' (min: %s%d, max: %u)",
			name, value, (sd->flags & SGF_0ISDISABLED) ? "(0) " : "", int_setting->min, int_setting->max);
	}
}

/**
 * List all settings and their value to the console
 *
 * @param prefilter  If not \c nullptr, only list settings with names that begin with \a prefilter prefix
 */
void IConsoleListSettings(const char *prefilter)
{
	IConsolePrintF(CC_WARNING, "All settings with their current value:");

	for (auto &sd : _settings) {
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (prefilter != nullptr && strstr(sd->name, prefilter) == nullptr) continue;
		char value[80];
		sd->FormatValue(value, lastof(value), &GetGameSettings());
		IConsolePrintF(CC_DEFAULT, "%s = %s", sd->name, value);
	}

	IConsolePrintF(CC_WARNING, "Use 'setting' command to change a value");
}

/**
 * Save and load handler for settings
 * @param settings SettingDesc struct containing all information
 * @param object can be either nullptr in which case we load global variables or
 * a pointer to a struct which is getting saved
 */
static void LoadSettings(const SettingTable &settings, void *object)
{
	for (auto &osd : settings) {
		void *ptr = GetVariableAddress(object, &osd->save);

		if (!SlObjectMember(ptr, &osd->save)) continue;
		if (osd->IsIntSetting()) osd->AsIntSetting()->Write_ValidateSetting(object, ReadValue(ptr, osd->save.conv));
	}
}

/**
 * Save and load handler for settings
 * @param settings SettingDesc struct containing all information
 * @param object can be either nullptr in which case we load global variables or
 * a pointer to a struct which is getting saved
 */
static void SaveSettings(const SettingTable &settings, void *object)
{
	/* We need to write the CH_RIFF header, but unfortunately can't call
	 * SlCalcLength() because we have a different format. So do this manually */
	size_t length = 0;
	for (auto &sd : settings) {
		length += SlCalcObjMemberLength(object, &sd->save);
	}
	SlSetLength(length);

	for (auto &sd : settings) {
		void *ptr = GetVariableAddress(object, &sd->save);
		SlObjectMember(ptr, &sd->save);
	}
}

static void Load_OPTS()
{
	/* Copy over default setting since some might not get loaded in
	 * a networking environment. This ensures for example that the local
	 * autosave-frequency stays when joining a network-server */
	PrepareOldDiffCustom();
	LoadSettings(_gameopt_settings, &_settings_game);
	HandleOldDiffCustom(true);
}

static void Load_PATS()
{
	/* Copy over default setting since some might not get loaded in
	 * a networking environment. This ensures for example that the local
	 * currency setting stays when joining a network-server */
	LoadSettings(_settings, &_settings_game);
}

static void Check_PATS()
{
	LoadSettings(_settings, &_load_check_data.settings);
}

static void Save_PATS()
{
	SaveSettings(_settings, &_settings_game);
}

extern const ChunkHandler _setting_chunk_handlers[] = {
	{ 'OPTS', nullptr,      Load_OPTS, nullptr, nullptr,       CH_RIFF},
	{ 'PATS', Save_PATS, Load_PATS, nullptr, Check_PATS, CH_RIFF | CH_LAST},
};

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
