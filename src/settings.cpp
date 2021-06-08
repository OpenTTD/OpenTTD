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
#include <array>
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
	const byte *p = static_cast<const byte *>(GetVariableAddress(object, this->save));
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
	uint id = (uint)this->Read(object);
	this->FormatSingleValue(buf, last, id);
}

void ManyOfManySettingDesc::FormatValue(char *buf, const char *last, const void *object) const
{
	uint bitmask = (uint)this->Read(object);
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
 * Make the value valid and then write it to the setting.
 * See #MakeValidValid and #Write for more details.
 * @param object The object the setting is to be saved in.
 * @param val Signed version of the new value.
 */
void IntSettingDesc::MakeValueValidAndWrite(const void *object, int32 val) const
{
	this->MakeValueValid(val);
	this->Write(object, val);
}

/**
 * Make the value valid given the limitations of this setting.
 *
 * In the case of int settings this is ensuring the value is between the minimum and
 * maximum value, with a special case for 0 if SF_GUI_0_IS_SPECIAL is set.
 * This is generally done by clamping the value so it is within the allowed value range.
 * However, for SF_GUI_DROPDOWN the default is used when the value is not valid.
 * @param val The value to make valid.
 */
void IntSettingDesc::MakeValueValid(int32 &val) const
{
	/* We need to take special care of the uint32 type as we receive from the function
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
			if (!(this->flags & SF_GUI_0_IS_SPECIAL) || val != 0) {
				if (!(this->flags & SF_GUI_DROPDOWN)) {
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
			/* Override the minimum value. No value below this->min, except special value 0 */
			uint32 uval = (uint32)val;
			if (!(this->flags & SF_GUI_0_IS_SPECIAL) || uval != 0) {
				if (!(this->flags & SF_GUI_DROPDOWN)) {
					/* Clamp value-type setting to its valid range */
					uval = ClampU(uval, this->min, this->max);
				} else if (uval < (uint)this->min || uval > this->max) {
					/* Reset invalid discrete setting to its default value */
					uval = (uint32)this->def;
				}
			}
			val = (int32)val;
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
void IntSettingDesc::Write(const void *object, int32 val) const
{
	void *ptr = GetVariableAddress(object, this->save);
	WriteValue(ptr, this->save.conv, (int64)val);
}

/**
 * Read the integer from the the actual setting.
 * @param object The object the setting is to be saved in.
 * @return The value of the saved integer.
 */
int32 IntSettingDesc::Read(const void *object) const
{
	void *ptr = GetVariableAddress(object, this->save);
	return (int32)ReadValue(ptr, this->save.conv);
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
	std::string stdstr(str, this->max_length - 1);
	str.assign(StrMakeValid(stdstr, SVS_NONE));
}

/**
 * Write a string to the actual setting.
 * @param object The object the setting is to be saved in.
 * @param str The string to save.
 */
void StringSettingDesc::Write(const void *object, const std::string &str) const
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
	this->MakeValueValidAndWrite(object, (int32)val);
}

void StringSettingDesc::ParseValue(const IniItem *item, void *object) const
{
	std::string str = (item == nullptr) ? this->def : item->value.value_or("");
	this->MakeValueValid(str);
	this->Write(object, str);
}

void ListSettingDesc::ParseValue(const IniItem *item, void *object) const
{
	const char *str = (item == nullptr) ? this->def : item->value.has_value() ? item->value->c_str() : nullptr;
	void *ptr = GetVariableAddress(object, this->save);
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
		if (sd->flags & SF_NOT_IN_CONFIG) continue;

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
	uint32 i = (uint32)this->Read(object);
	seprintf(buf, last, IsSignedVarMemType(this->save.conv) ? "%d" : "%u", i);
}

void BoolSettingDesc::FormatValue(char *buf, const char *last, const void *object) const
{
	bool val = this->Read(object) != 0;
	strecpy(buf, val ? "true" : "false", last);
}

bool IntSettingDesc::IsSameValue(const IniItem *item, void *object) const
{
	int32 item_value = (int32)this->ParseValue(item->value->c_str());
	int32 object_value = this->Read(object);
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
	if (!do_command && !(this->flags & SF_NO_NETWORK_SYNC) && _networking && !_network_server && !(this->flags & SF_PER_COMPANY)) return false;
	if ((this->flags & SF_NETWORK_ONLY) && !_networking && _game_mode != GM_MENU) return false;
	if ((this->flags & SF_NO_NETWORK) && _networking) return false;
	if ((this->flags & SF_NEWGAME_ONLY) &&
			(_game_mode == GM_NORMAL ||
			(_game_mode == GM_EDITOR && !(this->flags & SF_SCENEDIT_TOO)))) return false;
	if ((this->flags & SF_SCENEDIT_ONLY) && _game_mode != GM_EDITOR) return false;
	return true;
}

/**
 * Return the type of the setting.
 * @return type of setting
 */
SettingType SettingDesc::GetType() const
{
	if (this->flags & SF_PER_COMPANY) return ST_COMPANY;
	return (this->flags & SF_NOT_IN_SAVE) ? ST_CLIENT : ST_GAME;
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
static void v_PositionMainToolbar(int32 new_value)
{
	if (_game_mode != GM_MENU) PositionMainToolbar(nullptr);
}

/** Reposition the statusbar as the setting changed. */
static void v_PositionStatusbar(int32 new_value)
{
	if (_game_mode != GM_MENU) {
		PositionStatusbar(nullptr);
		PositionNewsMessage(nullptr);
		PositionNetworkChatWindow(nullptr);
	}
}

/**
 * Redraw the smallmap after a colour scheme change.
 * @param p1 Callback parameter.
 */
static void RedrawSmallmap(int32 new_value)
{
	BuildLandLegend();
	BuildOwnerLegend();
	SetWindowClassesDirty(WC_SMALLMAP);
}

static void StationSpreadChanged(int32 p1)
{
	InvalidateWindowData(WC_SELECT_STATION, 0);
	InvalidateWindowData(WC_BUILD_STATION, 0);
}

static void CloseSignalGUI(int32 new_value)
{
	if (new_value == 0) {
		CloseWindowByClass(WC_BUILD_SIGNAL);
	}
}

static void UpdateConsists(int32 new_value)
{
	for (Train *t : Train::Iterate()) {
		/* Update the consist of all trains so the maximum speed is set correctly. */
		if (t->IsFrontEngine() || t->IsFreeWagon()) t->ConsistChanged(CCF_TRACK);
	}
	InvalidateWindowClassesData(WC_BUILD_VEHICLE, 0);
}

/* Check service intervals of vehicles, newvalue is value of % or day based servicing */
static void UpdateServiceInterval(int32 new_value)
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

	if (new_value != 0) {
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
				v->SetServiceIntervalIsPercent(new_value != 0);
			}
		}
	}

	SetWindowClassesDirty(WC_VEHICLE_DETAILS);
}

static bool CanUpdateServiceInterval(VehicleType type, int32 &new_value)
{
	VehicleDefaultSettings *vds;
	if (_game_mode == GM_MENU || !Company::IsValidID(_current_company)) {
		vds = &_settings_client.company.vehicle;
	} else {
		vds = &Company::Get(_current_company)->settings.vehicle;
	}

	/* Test if the interval is valid */
	int32 interval = GetServiceIntervalClamped(new_value, vds->servint_ispercent);
	return interval == new_value;
}

static void UpdateServiceInterval(VehicleType type, int32 new_value)
{
	if (_game_mode != GM_MENU && Company::IsValidID(_current_company)) {
		for (Vehicle *v : Vehicle::Iterate()) {
			if (v->owner == _current_company && v->type == type && v->IsPrimaryVehicle() && !v->ServiceIntervalIsCustom()) {
				v->SetServiceInterval(new_value);
			}
		}
	}

	SetWindowClassesDirty(WC_VEHICLE_DETAILS);
}

static void TrainAccelerationModelChanged(int32 new_value)
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
}

/**
 * This function updates the train acceleration cache after a steepness change.
 * @param new_value Unused new value of setting.
 */
static void TrainSlopeSteepnessChanged(int32 new_value)
{
	for (Train *t : Train::Iterate()) {
		if (t->IsFrontEngine()) t->CargoChanged();
	}
}

/**
 * This function updates realistic acceleration caches when the setting "Road vehicle acceleration model" is set.
 * @param new_value Unused new value of setting.
 */
static void RoadVehAccelerationModelChanged(int32 new_value)
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
}

/**
 * This function updates the road vehicle acceleration cache after a steepness change.
 * @param new_value Unused new value of setting.
 */
static void RoadVehSlopeSteepnessChanged(int32 new_value)
{
	for (RoadVehicle *rv : RoadVehicle::Iterate()) {
		if (rv->IsFrontEngine()) rv->CargoChanged();
	}
}

static void TownFoundingChanged(int32 new_value)
{
	if (_game_mode != GM_EDITOR && _settings_game.economy.found_town == TF_FORBIDDEN) {
		CloseWindowById(WC_FOUND_TOWN, 0);
	} else {
		InvalidateWindowData(WC_FOUND_TOWN, 0);
	}
}

static void ZoomMinMaxChanged(int32 new_value)
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
}

static void SpriteZoomMinChanged(int32 new_value)
{
	GfxClearSpriteCache();
	/* Force all sprites to redraw at the new chosen zoom level */
	MarkWholeScreenDirty();
}

/**
 * Update any possible saveload window and delete any newgrf dialogue as
 * its widget parts might change. Reinit all windows as it allows access to the
 * newgrf debug button.
 * @param new_value unused.
 */
static void InvalidateNewGRFChangeWindows(int32 new_value)
{
	InvalidateWindowClassesData(WC_SAVELOAD);
	CloseWindowByClass(WC_GAME_OPTIONS);
	ReInitAllWindows(_gui_zoom_cfg);
}

static void InvalidateCompanyLiveryWindow(int32 new_value)
{
	InvalidateWindowClassesData(WC_COMPANY_COLOUR, -1);
	ResetVehicleColourMap();
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

static void DifficultyNoiseChange(int32 new_value)
{
	if (_game_mode == GM_NORMAL) {
		UpdateAirportsNoise();
		if (_settings_game.economy.station_noise_level) {
			InvalidateWindowClassesData(WC_TOWN_VIEW, 0);
		}
	}
}

static void MaxNoAIsChange(int32 new_value)
{
	if (GetGameSettings().difficulty.max_no_competitors != 0 &&
			AI::GetInfoList()->size() == 0 &&
			(!_networking || _network_server)) {
		ShowErrorMessage(STR_WARNING_NO_SUITABLE_AI, INVALID_STRING_ID, WL_CRITICAL);
	}

	InvalidateWindowClassesData(WC_GAME_OPTIONS, 0);
}

/**
 * Check whether the road side may be changed.
 * @param new_value unused
 * @return true if the road side may be changed.
 */
static bool CheckRoadSide(int32 &new_value)
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

static bool CheckFreeformEdges(int32 &new_value)
{
	if (_game_mode == GM_MENU) return true;
	if (new_value != 0) {
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
	}
	return true;
}

static void UpdateFreeformEdges(int32 new_value)
{
	if (_game_mode == GM_MENU) return;

	if (new_value != 0) {
		for (uint x = 0; x < MapSizeX(); x++) MakeVoid(TileXY(x, 0));
		for (uint y = 0; y < MapSizeY(); y++) MakeVoid(TileXY(0, y));
	} else {
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
}

/**
 * Changing the setting "allow multiple NewGRF sets" is not allowed
 * if there are vehicles.
 */
static bool CheckDynamicEngines(int32 &new_value)
{
	if (_game_mode == GM_MENU) return true;

	if (!EngineOverrideManager::ResetToCurrentNewGRFConfig()) {
		ShowErrorMessage(STR_CONFIG_SETTING_DYNAMIC_ENGINES_EXISTING_VEHICLES, INVALID_STRING_ID, WL_ERROR);
		return false;
	}

	return true;
}

static bool CheckMaxHeightLevel(int32 &new_value)
{
	if (_game_mode == GM_NORMAL) return false;
	if (_game_mode != GM_EDITOR) return true;

	/* Check if at least one mountain on the map is higher than the new value.
	 * If yes, disallow the change. */
	for (TileIndex t = 0; t < MapSize(); t++) {
		if ((int32)TileHeight(t) > new_value) {
			ShowErrorMessage(STR_CONFIG_SETTING_TOO_HIGH_MOUNTAIN, INVALID_STRING_ID, WL_ERROR);
			/* Return old, unchanged value */
			return false;
		}
	}

	return true;
}

static void StationCatchmentChanged(int32 new_value)
{
	Station::RecomputeCatchmentForAll();
	MarkWholeScreenDirty();
}

static void MaxVehiclesChanged(int32 new_value)
{
	InvalidateWindowClassesData(WC_BUILD_TOOLBAR);
	MarkWholeScreenDirty();
}

static void InvalidateShipPathCache(int32 new_value)
{
	for (Ship *s : Ship::Iterate()) {
		s->path.clear();
	}
}

/**
 * Replace a passwords that are a literal asterisk with an empty string.
 * @param newval The new string value for this password field.
 * @return Always true.
 */
static bool ReplaceAsteriskWithEmptyPassword(std::string &newval)
{
	if (newval.compare("*") == 0) newval.clear();
	return true;
}

/** Update the game info, and send it to the clients when we are running as a server. */
static void UpdateClientConfigValues()
{
	NetworkServerUpdateGameInfo();
	if (_network_server) NetworkServerSendConfigUpdate();
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
	/* Savegames before v4 didn't have "town_council_tolerance" in savegame yet. */
	bool has_no_town_council_tolerance = savegame && IsSavegameVersionBefore(SLV_4);
	uint options_to_load = GAME_DIFFICULTY_NUM - (has_no_town_council_tolerance ? 1 : 0);

	if (!savegame) {
		/* If we did read to old_diff_custom, then at least one value must be non 0. */
		bool old_diff_custom_used = false;
		for (uint i = 0; i < options_to_load && !old_diff_custom_used; i++) {
			old_diff_custom_used = (_old_diff_custom[i] != 0);
		}

		if (!old_diff_custom_used) return;
	}

	/* Iterate over all the old difficulty settings, and convert the list-value to the new setting. */
	uint i = 0;
	for (const auto &name : _old_diff_settings) {
		if (has_no_town_council_tolerance && name == "town_council_tolerance") continue;

		std::string fullname = "difficulty." + name;
		const SettingDesc *sd = GetSettingFromName(fullname);

		/* Some settings are no longer in use; skip reading those. */
		if (sd == nullptr) {
			i++;
			continue;
		}

		int32 value = (int32)((name == "max_loan" ? 1000 : 1) * _old_diff_custom[i++]);
		sd->AsIntSetting()->MakeValueValidAndWrite(savegame ? &_settings_game : &_settings_newgame, value);
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
				Debug(script, 0, "The AI by the name '{}' was no longer found, and removed from the list.", item->name);
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
			Debug(script, 0, "The GameScript by the name '{}' was no longer found, and removed from the list.", item->name);
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
	int32 oldval = this->Read(object);
	this->MakeValueValid(newval);
	if (this->pre_check != nullptr && !this->pre_check(newval)) return;
	if (oldval == newval) return;

	this->Write(object, newval);
	if (this->post_callback != nullptr) this->post_callback(newval);

	if (this->flags & SF_NO_NETWORK) {
		GamelogStartAction(GLAT_SETTING);
		GamelogSetting(this->name, oldval, newval);
		GamelogStopAction();
	}

	SetWindowClassesDirty(WC_GAME_OPTIONS);

	if (_save_config) SaveToConfig();
}

/**
 * Given a name of setting, return a setting description from the table.
 * @param name Name of the setting to return a setting description of.
 * @param settings Table to look in for the setting.
 * @return Pointer to the setting description of setting \a name if it can be found,
 *         \c nullptr indicates failure to obtain the description.
 */
static const SettingDesc *GetSettingFromName(const std::string_view name, const SettingTable &settings)
{
	/* First check all full names */
	for (auto &sd : settings) {
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (sd->name == name) return sd.get();
	}

	/* Then check the shortcut variant of the name. */
	std::string short_name_suffix = std::string{ "." }.append(name);
	for (auto &sd : settings) {
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (StrEndsWith(sd->name, short_name_suffix)) return sd.get();
	}

	return nullptr;
}

/**
 * Get the SaveLoad from all settings matching the prefix.
 * @param prefix The prefix to look for.
 * @param saveloads A vector to store the result in.
 */
void GetSettingSaveLoadByPrefix(std::string_view prefix, std::vector<SaveLoad> &saveloads)
{
	for (auto &sd : _settings) {
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (StrStartsWith(sd->name, prefix)) saveloads.push_back(sd->save);
	}
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
	if (StrStartsWith(name, company_prefix)) name.remove_prefix(company_prefix.size());
	return GetSettingFromName(name, _company_settings);
}

/**
 * Given a name of any setting, return any setting description of it.
 * @param name  Name of the setting to return a setting description of.
 * @return Pointer to the setting description of setting \a name if it can be found,
 *         \c nullptr indicates failure to obtain the description.
 */
const SettingDesc *GetSettingFromName(const std::string_view name)
{
	auto sd = GetSettingFromName(name, _settings);
	if (sd != nullptr) return sd;

	return GetCompanySettingFromName(name);
}

/**
 * Network-safe changing of settings (server-only).
 * @param tile unused
 * @param flags operation to perform
 * @param p1 unused
 * @param p2 the new value for the setting
 * The new value is properly clamped to its minimum/maximum when setting
 * @param text the name of the setting to change
 * @return the cost of this operation or an error
 * @see _settings
 */
CommandCost CmdChangeSetting(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const std::string &text)
{
	if (text.empty()) return CMD_ERROR;
	const SettingDesc *sd = GetSettingFromName(text);

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
 * @param p1 unused
 * @param p2 the new value for the setting
 * The new value is properly clamped to its minimum/maximum when setting
 * @param text the name of the company setting to change
 * @return the cost of this operation or an error
 */
CommandCost CmdChangeCompanySetting(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const std::string &text)
{
	if (text.empty()) return CMD_ERROR;
	const SettingDesc *sd = GetCompanySettingFromName(text.c_str());

	if (sd == nullptr) return CMD_ERROR;
	if (!sd->IsIntSetting()) return CMD_ERROR;

	if (flags & DC_EXEC) {
		sd->AsIntSetting()->ChangeValue(&Company::Get(_current_company)->settings, p2);
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
bool SetSettingValue(const IntSettingDesc *sd, int32 value, bool force_newgame)
{
	const IntSettingDesc *setting = sd->AsIntSetting();
	if ((setting->flags & SF_PER_COMPANY) != 0) {
		if (Company::IsValidID(_local_company) && _game_mode != GM_MENU) {
			return DoCommandP(0, 0, value, CMD_CHANGE_COMPANY_SETTING, nullptr, setting->name);
		}

		setting->ChangeValue(&_settings_client.company, value);
		return true;
	}

	/* If an item is company-based, we do not send it over the network
	 * (if any) to change. Also *hack*hack* we update the _newgame version
	 * of settings because changing a company-based setting in a game also
	 * changes its defaults. At least that is the convention we have chosen */
	if (setting->flags & SF_NO_NETWORK_SYNC) {
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
		return DoCommandP(0, 0, value, CMD_CHANGE_SETTING, nullptr, setting->name);
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
		int_setting->MakeValueValidAndWrite(&c->settings, int_setting->def);
	}
}

/**
 * Sync all company settings in a multiplayer game.
 */
void SyncCompanySettings()
{
	const void *old_object = &Company::Get(_current_company)->settings;
	const void *new_object = &_settings_client.company;
	for (auto &sd : _company_settings) {
		uint32 old_value = (uint32)sd->AsIntSetting()->Read(new_object);
		uint32 new_value = (uint32)sd->AsIntSetting()->Read(old_object);
		if (old_value != new_value) NetworkSendCommand(0, 0, new_value, CMD_CHANGE_COMPANY_SETTING, nullptr, sd->name, _local_company);
	}
}

/**
 * Set a setting value with a string.
 * @param sd the setting to change.
 * @param value the value to write
 * @param force_newgame force the newgame settings
 * @note Strings WILL NOT be synced over the network
 */
bool SetSettingValue(const StringSettingDesc *sd, std::string value, bool force_newgame)
{
	assert(sd->flags & SF_NO_NETWORK_SYNC);

	if (GetVarMemType(sd->save.conv) == SLE_VAR_STRQ && value.compare("(null)") == 0) {
		value.clear();
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
void StringSettingDesc::ChangeValue(const void *object, std::string &newval) const
{
	this->MakeValueValid(newval);
	if (this->pre_check != nullptr && !this->pre_check(newval)) return;

	this->Write(object, newval);
	if (this->post_callback != nullptr) this->post_callback(newval);

	if (_save_config) SaveToConfig();
}

/* Those 2 functions need to be here, else we have to make some stuff non-static
 * and besides, it is also better to keep stuff like this at the same place */
void IConsoleSetSetting(const char *name, const char *value, bool force_newgame)
{
	const SettingDesc *sd = GetSettingFromName(name);
	if (sd == nullptr) {
		IConsolePrint(CC_ERROR, "'{}' is an unknown setting.", name);
		return;
	}

	bool success = true;
	if (sd->IsStringSetting()) {
		success = SetSettingValue(sd->AsStringSetting(), value, force_newgame);
	} else if (sd->IsIntSetting()) {
		uint32 val;
		extern bool GetArgumentInteger(uint32 *value, const char *arg);
		success = GetArgumentInteger(&val, value);
		if (!success) {
			IConsolePrint(CC_ERROR, "'{}' is not an integer.", value);
			return;
		}

		success = SetSettingValue(sd->AsIntSetting(), val, force_newgame);
	}

	if (!success) {
		if (_network_server) {
			IConsolePrint(CC_ERROR, "This command/variable is not available during network games.");
		} else {
			IConsolePrint(CC_ERROR, "This command/variable is only available to a network server.");
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
		IConsolePrint(CC_ERROR, "'{}' is an unknown setting.", name);
		return;
	}

	const void *object = (_game_mode == GM_MENU || force_newgame) ? &_settings_newgame : &_settings_game;

	if (sd->IsStringSetting()) {
		IConsolePrint(CC_INFO, "Current value for '{}' is '{}'.", sd->name, sd->AsStringSetting()->Read(object));
	} else if (sd->IsIntSetting()) {
		char value[20];
		sd->FormatValue(value, lastof(value), object);
		const IntSettingDesc *int_setting = sd->AsIntSetting();
		IConsolePrint(CC_INFO, "Current value for '{}' is '{}' (min: {}{}, max: {}).",
			sd->name.c_str(), value, (sd->flags & SF_GUI_0_IS_SPECIAL) ? "(0) " : "", int_setting->min, int_setting->max);
	}
}

/**
 * List all settings and their value to the console
 *
 * @param prefilter  If not \c nullptr, only list settings with names that begin with \a prefilter prefix
 */
void IConsoleListSettings(const char *prefilter)
{
	IConsolePrint(CC_HELP, "All settings with their current value:");

	for (auto &sd : _settings) {
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (prefilter != nullptr && sd->name.find(prefilter) == std::string::npos) continue;
		char value[80];
		sd->FormatValue(value, lastof(value), &GetGameSettings());
		IConsolePrint(CC_DEFAULT, "{} = {}", sd->name, value);
	}

	IConsolePrint(CC_HELP, "Use 'setting' command to change a value.");
}

/**
 * Get the SaveLoad description for the SettingTable.
 * @param settings SettingDesc struct containing all information.
 * @param is_loading True iff the SaveLoad table is for loading.
 * @return Vector with SaveLoad entries for the SettingTable.
 */
static std::vector<SaveLoad> GetSettingsDesc(const SettingTable &settings, bool is_loading)
{
	std::vector<SaveLoad> saveloads;
	for (auto &sd : settings) {
		if (sd->flags & SF_NOT_IN_SAVE) continue;

		if (is_loading && (sd->flags & SF_NO_NETWORK_SYNC) && _networking && !_network_server) {
			/* We don't want to read this setting, so we do need to skip over it. */
			saveloads.push_back({sd->save.cmd, GetVarFileType(sd->save.conv) | SLE_VAR_NULL, sd->save.length, sd->save.version_from, sd->save.version_to, 0, nullptr, 0, nullptr});
			continue;
		}

		saveloads.push_back(sd->save);
	}

	return saveloads;
}


/**
 * Save and load handler for settings
 * @param settings SettingDesc struct containing all information
 * @param object can be either nullptr in which case we load global variables or
 * a pointer to a struct which is getting saved
 */
static void LoadSettings(const SettingTable &settings, void *object)
{
	const std::vector<SaveLoad> slt = GetSettingsDesc(settings, true);

	SlObject(object, slt);

	/* Ensure all IntSettings are valid (min/max could have changed between versions etc). */
	for (auto &sd : settings) {
		if (sd->flags & SF_NOT_IN_SAVE) continue;
		if ((sd->flags & SF_NO_NETWORK_SYNC) && _networking && !_network_server) continue;
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;

		if (sd->IsIntSetting()) {
			const IntSettingDesc *int_setting = sd->AsIntSetting();
			int_setting->MakeValueValidAndWrite(object, int_setting->Read(object));
		}
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
	const std::vector<SaveLoad> slt = GetSettingsDesc(settings, false);

	SlObject(object, slt);
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

static const ChunkHandler setting_chunk_handlers[] = {
	{ 'OPTS', nullptr,   Load_OPTS, nullptr, nullptr,    CH_RIFF },
	{ 'PATS', Save_PATS, Load_PATS, nullptr, Check_PATS, CH_RIFF },
};

extern const ChunkHandlerTable _setting_chunk_handlers(setting_chunk_handlers);

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
