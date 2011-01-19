/* $Id$ */

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
#include "strings_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "company_func.h"
#include "rev.h"
#ifdef WITH_FREETYPE
#include "fontcache.h"
#endif
#include "textbuf_gui.h"
#include "rail_gui.h"
#include "elrail_func.h"
#include "gui.h"
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
#include "newgrf.h"
#include "ship.h"
#include "smallmap_gui.h"
#include "roadveh.h"
#include "fios.h"

#include "void_map.h"
#include "station_base.h"

#include "table/strings.h"
#include "table/settings.h"

ClientSettings _settings_client;
GameSettings _settings_game;     ///< Game settings of a running game or the scenario editor.
GameSettings _settings_newgame;  ///< Game settings for new games (updated from the intro screen).
VehicleDefaultSettings _old_vds; ///< Used for loading default vehicles settings from old savegames
char *_config_file; ///< Configuration file of OpenTTD

typedef void SettingDescProc(IniFile *ini, const SettingDesc *desc, const char *grpname, void *object);
typedef void SettingDescProcList(IniFile *ini, const char *grpname, StringList *list);

static bool IsSignedVarMemType(VarType vt);

/**
 * Groups in openttd.cfg that are actually lists.
 */
static const char * const _list_group_names[] = {
	"bans",
	"newgrf",
	"servers",
	"server_bind_addresses",
	NULL
};

/**
 * Find the index value of a ONEofMANY type in a string seperated by |
 * @param many full domain of values the ONEofMANY setting can have
 * @param one the current value of the setting for which a value needs found
 * @param onelen force calculation of the *one parameter
 * @return the integer index of the full-list, or -1 if not found
 */
static int LookupOneOfMany(const char *many, const char *one, size_t onelen = 0)
{
	const char *s;
	int idx;

	if (onelen == 0) onelen = strlen(one);

	/* check if it's an integer */
	if (*one >= '0' && *one <= '9') return strtoul(one, NULL, 0);

	idx = 0;
	for (;;) {
		/* find end of item */
		s = many;
		while (*s != '|' && *s != 0) s++;
		if ((size_t)(s - many) == onelen && !memcmp(one, many, onelen)) return idx;
		if (*s == 0) return -1;
		many = s + 1;
		idx++;
	}
}

/**
 * Find the set-integer value MANYofMANY type in a string
 * @param many full domain of values the MANYofMANY setting can have
 * @param str the current string value of the setting, each individual
 * of seperated by a whitespace,tab or | character
 * @return the 'fully' set integer, or -1 if a set is not found
 */
static uint32 LookupManyOfMany(const char *many, const char *str)
{
	const char *s;
	int r;
	uint32 res = 0;

	for (;;) {
		/* skip "whitespace" */
		while (*str == ' ' || *str == '\t' || *str == '|') str++;
		if (*str == 0) break;

		s = str;
		while (*s != 0 && *s != ' ' && *s != '\t' && *s != '|') s++;

		r = LookupOneOfMany(many, str, s - str);
		if (r == -1) return (uint32)-1;

		SetBit(res, r); // value found, set it
		if (*s == 0) break;
		str = s + 1;
	}
	return res;
}

/**
 * Parse an integerlist string and set each found value
 * @param p the string to be parsed. Each element in the list is seperated by a
 * comma or a space character
 * @param items pointer to the integerlist-array that will be filled with values
 * @param maxitems the maximum number of elements the integerlist-array has
 * @return returns the number of items found, or -1 on an error
 */
static int ParseIntList(const char *p, int *items, int maxitems)
{
	int n = 0; // number of items read so far
	bool comma = false; // do we accept comma?

	while (*p != '\0') {
		switch (*p) {
			case ',':
				/* Do not accept multiple commas between numbers */
				if (!comma) return -1;
				comma = false;
				/* FALL THROUGH */
			case ' ':
				p++;
				break;

			default: {
				if (n == maxitems) return -1; // we don't accept that many numbers
				char *end;
				long v = strtol(p, &end, 0);
				if (p == end) return -1; // invalid character (not a number)
				if (sizeof(int) < sizeof(long)) v = ClampToI32(v);
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
	int items[64];
	int i, nitems;

	if (str == NULL) {
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
 * is seperated by a comma or a space character
 * @param buf output buffer where the string-representation will be stored
 * @param last last item to write to in the output buffer
 * @param array pointer to the integer-arrays that is read from
 * @param nelems the number of elements the array holds.
 * @param type the type of elements the array holds (eg INT8, UINT16, etc.)
 */
static void MakeIntList(char *buf, const char *last, const void *array, int nelems, VarType type)
{
	int i, v = 0;
	byte *p = (byte*)array;

	for (i = 0; i != nelems; i++) {
		switch (type) {
		case SLE_VAR_BL:
		case SLE_VAR_I8:  v = *(int8*)p;   p += 1; break;
		case SLE_VAR_U8:  v = *(byte*)p;   p += 1; break;
		case SLE_VAR_I16: v = *(int16*)p;  p += 2; break;
		case SLE_VAR_U16: v = *(uint16*)p; p += 2; break;
		case SLE_VAR_I32: v = *(int32*)p;  p += 4; break;
		case SLE_VAR_U32: v = *(uint32*)p; p += 4; break;
		default: NOT_REACHED();
		}
		buf += seprintf(buf, last, (i == 0) ? "%d" : ",%d", v);
	}
}

/**
 * Convert a ONEofMANY structure to a string representation.
 * @param buf output buffer where the string-representation will be stored
 * @param last last item to write to in the output buffer
 * @param many the full-domain string of possible values
 * @param id the value of the variable and whose string-representation must be found
 */
static void MakeOneOfMany(char *buf, const char *last, const char *many, int id)
{
	int orig_id = id;

	/* Look for the id'th element */
	while (--id >= 0) {
		for (; *many != '|'; many++) {
			if (*many == '\0') { // not found
				seprintf(buf, last, "%d", orig_id);
				return;
			}
		}
		many++; // pass the |-character
	}

	/* copy string until next item (|) or the end of the list if this is the last one */
	while (*many != '\0' && *many != '|' && buf < last) *buf++ = *many++;
	*buf = '\0';
}

/**
 * Convert a MANYofMANY structure to a string representation.
 * @param buf output buffer where the string-representation will be stored
 * @param last last item to write to in the output buffer
 * @param many the full-domain string of possible values
 * @param x the value of the variable and whose string-representation must
 *        be found in the bitmasked many string
 */
static void MakeManyOfMany(char *buf, const char *last, const char *many, uint32 x)
{
	const char *start;
	int i = 0;
	bool init = true;

	for (; x != 0; x >>= 1, i++) {
		start = many;
		while (*many != 0 && *many != '|') many++; // advance to the next element

		if (HasBit(x, 0)) { // item found, copy it
			if (!init) buf += seprintf(buf, last, "|");
			init = false;
			if (start == many) {
				buf += seprintf(buf, last, "%d", i);
			} else {
				memcpy(buf, start, many - start);
				buf += many - start;
			}
		}

		if (*many == '|') many++;
	}

	*buf = '\0';
}

/**
 * Convert a string representation (external) of a setting to the internal rep.
 * @param desc SettingDesc struct that holds all information about the variable
 * @param orig_str input string that will be parsed based on the type of desc
 * @return return the parsed value of the setting
 */
static const void *StringToVal(const SettingDescBase *desc, const char *orig_str)
{
	const char *str = orig_str == NULL ? "" : orig_str;
	switch (desc->cmd) {
	case SDT_NUMX: {
		char *end;
		unsigned long val = strtoul(str, &end, 0);
		if (*end != '\0') ShowInfoF("ini: trailing characters at end of setting '%s'", desc->name);
		return (void*)val;
	}
	case SDT_ONEOFMANY: {
		long r = LookupOneOfMany(desc->many, str);
		/* if the first attempt of conversion from string to the appropriate value fails,
		 * look if we have defined a converter from old value to new value. */
		if (r == -1 && desc->proc_cnvt != NULL) r = desc->proc_cnvt(str);
		if (r != -1) return (void*)r; // and here goes converted value
		ShowInfoF("ini: invalid value '%s' for '%s'", str, desc->name); // sorry, we failed
		return 0;
	}
	case SDT_MANYOFMANY: {
		unsigned long r = LookupManyOfMany(desc->many, str);
		if (r != (unsigned long)-1) return (void*)r;
		ShowInfoF("ini: invalid value '%s' for '%s'", str, desc->name);
		return 0;
	}
	case SDT_BOOLX:
		if (strcmp(str, "true")  == 0 || strcmp(str, "on")  == 0 || strcmp(str, "1") == 0) return (void*)true;
		if (strcmp(str, "false") == 0 || strcmp(str, "off") == 0 || strcmp(str, "0") == 0) return (void*)false;
		ShowInfoF("ini: invalid setting value '%s' for '%s'", str, desc->name);
		break;

	case SDT_STRING: return orig_str;
	case SDT_INTLIST: return str;
	default: break;
	}

	return NULL;
}

/**
 * Set the value of a setting and if needed clamp the value to
 * the preset minimum and maximum.
 * @param ptr the variable itself
 * @param sd pointer to the 'information'-database of the variable
 * @param val signed long version of the new value
 * @pre SettingDesc is of type SDT_BOOLX, SDT_NUMX,
 * SDT_ONEOFMANY or SDT_MANYOFMANY. Other types are not supported as of now
 */
static void Write_ValidateSetting(void *ptr, const SettingDesc *sd, int32 val)
{
	const SettingDescBase *sdb = &sd->desc;

	if (sdb->cmd != SDT_BOOLX &&
			sdb->cmd != SDT_NUMX &&
			sdb->cmd != SDT_ONEOFMANY &&
			sdb->cmd != SDT_MANYOFMANY) {
		return;
	}

	/* We cannot know the maximum value of a bitset variable, so just have faith */
	if (sdb->cmd != SDT_MANYOFMANY) {
		/* We need to take special care of the uint32 type as we receive from the function
		 * a signed integer. While here also bail out on 64-bit settings as those are not
		 * supported. Unsigned 8 and 16-bit variables are safe since they fit into a signed
		 * 32-bit variable
		 * TODO: Support 64-bit settings/variables */
		switch (GetVarMemType(sd->save.conv)) {
			case SLE_VAR_NULL: return;
			case SLE_VAR_BL:
			case SLE_VAR_I8:
			case SLE_VAR_U8:
			case SLE_VAR_I16:
			case SLE_VAR_U16:
			case SLE_VAR_I32: {
				/* Override the minimum value. No value below sdb->min, except special value 0 */
				if (!(sdb->flags & SGF_0ISDISABLED) || val != 0) val = Clamp(val, sdb->min, sdb->max);
				break;
			}
			case SLE_VAR_U32: {
				/* Override the minimum value. No value below sdb->min, except special value 0 */
				uint min = ((sdb->flags & SGF_0ISDISABLED) && (uint)val <= (uint)sdb->min) ? 0 : sdb->min;
				WriteValue(ptr, SLE_VAR_U32, (int64)ClampU(val, min, sdb->max));
				return;
			}
			case SLE_VAR_I64:
			case SLE_VAR_U64:
			default: NOT_REACHED();
		}
	}

	WriteValue(ptr, sd->save.conv, (int64)val);
}

/**
 * Load values from a group of an IniFile structure into the internal representation
 * @param ini pointer to IniFile structure that holds administrative information
 * @param sd pointer to SettingDesc structure whose internally pointed variables will
 *        be given values
 * @param grpname the group of the IniFile to search in for the new values
 * @param object pointer to the object been loaded
 */
static void IniLoadSettings(IniFile *ini, const SettingDesc *sd, const char *grpname, void *object)
{
	IniGroup *group;
	IniGroup *group_def = ini->GetGroup(grpname);
	IniItem *item;
	const void *p;
	void *ptr;
	const char *s;

	for (; sd->save.cmd != SL_END; sd++) {
		const SettingDescBase *sdb = &sd->desc;
		const SaveLoad        *sld = &sd->save;

		if (!SlIsObjectCurrentlyValid(sld->version_from, sld->version_to)) continue;

		/* For settings.xx.yy load the settings from [xx] yy = ? */
		s = strchr(sdb->name, '.');
		if (s != NULL) {
			group = ini->GetGroup(sdb->name, s - sdb->name);
			s++;
		} else {
			s = sdb->name;
			group = group_def;
		}

		item = group->GetItem(s, false);
		if (item == NULL && group != group_def) {
			/* For settings.xx.yy load the settings from [settingss] yy = ? in case the previous
			 * did not exist (e.g. loading old config files with a [settings] section */
			item = group_def->GetItem(s, false);
		}
		if (item == NULL) {
			/* For settings.xx.zz.yy load the settings from [zz] yy = ? in case the previous
			 * did not exist (e.g. loading old config files with a [yapf] section */
			const char *sc = strchr(s, '.');
			if (sc != NULL) item = ini->GetGroup(s, sc - s)->GetItem(sc + 1, false);
		}

		p = (item == NULL) ? sdb->def : StringToVal(sdb, item->value);
		ptr = GetVariableAddress(object, sld);

		switch (sdb->cmd) {
		case SDT_BOOLX: // All four are various types of (integer) numbers
		case SDT_NUMX:
		case SDT_ONEOFMANY:
		case SDT_MANYOFMANY:
			Write_ValidateSetting(ptr, sd, (int32)(size_t)p); break;

		case SDT_STRING:
			switch (GetVarMemType(sld->conv)) {
				case SLE_VAR_STRB:
				case SLE_VAR_STRBQ:
					if (p != NULL) ttd_strlcpy((char*)ptr, (const char*)p, sld->length);
					break;
				case SLE_VAR_STR:
				case SLE_VAR_STRQ:
					free(*(char**)ptr);
					*(char**)ptr = p == NULL ? NULL : strdup((const char*)p);
					break;
				case SLE_VAR_CHAR: if (p != NULL) *(char*)ptr = *(char*)p; break;
				default: NOT_REACHED();
			}
			break;

		case SDT_INTLIST: {
			if (!LoadIntList((const char*)p, ptr, sld->length, GetVarMemType(sld->conv))) {
				ShowInfoF("ini: error in array '%s'", sdb->name);
			} else if (sd->desc.proc_cnvt != NULL) {
				sd->desc.proc_cnvt((const char*)p);
			}
			break;
		}
		default: NOT_REACHED();
		}
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
static void IniSaveSettings(IniFile *ini, const SettingDesc *sd, const char *grpname, void *object)
{
	IniGroup *group_def = NULL, *group;
	IniItem *item;
	char buf[512];
	const char *s;
	void *ptr;

	for (; sd->save.cmd != SL_END; sd++) {
		const SettingDescBase *sdb = &sd->desc;
		const SaveLoad        *sld = &sd->save;

		/* If the setting is not saved to the configuration
		 * file, just continue with the next setting */
		if (!SlIsObjectCurrentlyValid(sld->version_from, sld->version_to)) continue;
		if (sld->conv & SLF_CONFIG_NO) continue;

		/* XXX - wtf is this?? (group override?) */
		s = strchr(sdb->name, '.');
		if (s != NULL) {
			group = ini->GetGroup(sdb->name, s - sdb->name);
			s++;
		} else {
			if (group_def == NULL) group_def = ini->GetGroup(grpname);
			s = sdb->name;
			group = group_def;
		}

		item = group->GetItem(s, true);
		ptr = GetVariableAddress(object, sld);

		if (item->value != NULL) {
			/* check if the value is the same as the old value */
			const void *p = StringToVal(sdb, item->value);

			/* The main type of a variable/setting is in bytes 8-15
			 * The subtype (what kind of numbers do we have there) is in 0-7 */
			switch (sdb->cmd) {
			case SDT_BOOLX:
			case SDT_NUMX:
			case SDT_ONEOFMANY:
			case SDT_MANYOFMANY:
				switch (GetVarMemType(sld->conv)) {
				case SLE_VAR_BL:
					if (*(bool*)ptr == (p != NULL)) continue;
					break;
				case SLE_VAR_I8:
				case SLE_VAR_U8:
					if (*(byte*)ptr == (byte)(unsigned long)p) continue;
					break;
				case SLE_VAR_I16:
				case SLE_VAR_U16:
					if (*(uint16*)ptr == (uint16)(unsigned long)p) continue;
					break;
				case SLE_VAR_I32:
				case SLE_VAR_U32:
					if (*(uint32*)ptr == (uint32)(unsigned long)p) continue;
					break;
				default: NOT_REACHED();
				}
				break;
			default: break; // Assume the other types are always changed
			}
		}

		/* Value has changed, get the new value and put it into a buffer */
		switch (sdb->cmd) {
		case SDT_BOOLX:
		case SDT_NUMX:
		case SDT_ONEOFMANY:
		case SDT_MANYOFMANY: {
			uint32 i = (uint32)ReadValue(ptr, sld->conv);

			switch (sdb->cmd) {
			case SDT_BOOLX:      strecpy(buf, (i != 0) ? "true" : "false", lastof(buf)); break;
			case SDT_NUMX:       seprintf(buf, lastof(buf), IsSignedVarMemType(sld->conv) ? "%d" : "%u", i); break;
			case SDT_ONEOFMANY:  MakeOneOfMany(buf, lastof(buf), sdb->many, i); break;
			case SDT_MANYOFMANY: MakeManyOfMany(buf, lastof(buf), sdb->many, i); break;
			default: NOT_REACHED();
			}
			break;
		}

		case SDT_STRING:
			switch (GetVarMemType(sld->conv)) {
			case SLE_VAR_STRB: strecpy(buf, (char*)ptr, lastof(buf)); break;
			case SLE_VAR_STRBQ:seprintf(buf, lastof(buf), "\"%s\"", (char*)ptr); break;
			case SLE_VAR_STR:  strecpy(buf, *(char**)ptr, lastof(buf)); break;
			case SLE_VAR_STRQ:
				if (*(char**)ptr == NULL) {
					buf[0] = '\0';
				} else {
					seprintf(buf, lastof(buf), "\"%s\"", *(char**)ptr);
				}
				break;
			case SLE_VAR_CHAR: buf[0] = *(char*)ptr; buf[1] = '\0'; break;
			default: NOT_REACHED();
			}
			break;

		case SDT_INTLIST:
			MakeIntList(buf, lastof(buf), ptr, sld->length, GetVarMemType(sld->conv));
			break;
		default: NOT_REACHED();
		}

		/* The value is different, that means we have to write it to the ini */
		free(item->value);
		item->value = strdup(buf);
	}
}

/**
 * Loads all items from a 'grpname' section into a list
 * The list parameter can be a NULL pointer, in this case nothing will be
 * saved and a callback function should be defined that will take over the
 * list-handling and store the data itself somewhere.
 * @param ini IniFile handle to the ini file with the source data
 * @param grpname character string identifying the section-header of the ini file that will be parsed
 * @param list new list with entries of the given section
 */
static void IniLoadSettingList(IniFile *ini, const char *grpname, StringList *list)
{
	IniGroup *group = ini->GetGroup(grpname);

	if (group == NULL || list == NULL) return;

	list->Clear();

	for (const IniItem *item = group->item; item != NULL; item = item->next) {
		if (item->name != NULL) *list->Append() = strdup(item->name);
	}
}

/**
 * Saves all items from a list into the 'grpname' section
 * The list parameter can be a NULL pointer, in this case a callback function
 * should be defined that will provide the source data to be saved.
 * @param ini IniFile handle to the ini file where the destination data is saved
 * @param grpname character string identifying the section-header of the ini file
 * @param list pointer to an string(pointer) array that will be used as the
 *             source to be saved into the relevant ini section
 */
static void IniSaveSettingList(IniFile *ini, const char *grpname, StringList *list)
{
	IniGroup *group = ini->GetGroup(grpname);

	if (group == NULL || list == NULL) return;
	group->Clear();

	for (char **iter = list->Begin(); iter != list->End(); iter++) {
		group->GetItem(*iter, true)->SetValue("");
	}
}

/* Begin - Callback Functions for the various settings. */

/** Reposition the main toolbar as the setting changed. */
static bool v_PositionMainToolbar(int32 p1)
{
	if (_game_mode != GM_MENU) PositionMainToolbar(NULL);
	return true;
}

/** Reposition the statusbar as the setting changed. */
static bool v_PositionStatusbar(int32 p1)
{
	if (_game_mode != GM_MENU) {
		PositionStatusbar(NULL);
		PositionNewsMessage(NULL);
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

static bool InvalidateStationBuildWindow(int32 p1)
{
	SetWindowDirty(WC_BUILD_STATION, 0);
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
	Train *t;
	FOR_ALL_TRAINS(t) {
		/* Update the consist of all trains so the maximum speed is set correctly. */
		if (t->IsFrontEngine() || t->IsFreeWagon()) t->ConsistChanged(true);
	}
	return true;
}

/* Check service intervals of vehicles, p1 is value of % or day based servicing */
static bool CheckInterval(int32 p1)
{
	VehicleDefaultSettings *vds;
	if (_game_mode == GM_MENU || !Company::IsValidID(_current_company)) {
		vds = &_settings_client.company.vehicle;
	} else {
		vds = &Company::Get(_current_company)->settings.vehicle;
	}

	if (p1) {
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

	InvalidateDetailsWindow(0);

	return true;
}

static bool TrainAccelerationModelChanged(int32 p1)
{
	Train *t;
	FOR_ALL_TRAINS(t) {
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
	Train *t;
	FOR_ALL_TRAINS(t) {
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
		RoadVehicle *rv;
		FOR_ALL_ROADVEHICLES(rv) {
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
	RoadVehicle *rv;
	FOR_ALL_ROADVEHICLES(rv) {
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
	InvalidateWindowClassesData(WC_VEHICLE_TIMETABLE, -2);
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
	ReInitAllWindows();
	return true;
}

static bool InvalidateCompanyLiveryWindow(int32 p1)
{
	InvalidateWindowClassesData(WC_COMPANY_COLOUR);
	return RedrawScreen(p1);
}

static bool InvalidateIndustryViewWindow(int32 p1)
{
	InvalidateWindowClassesData(WC_INDUSTRY_VIEW);
	return true;
}

/*
 * A: competitors
 * B: competitor start time. Deprecated since savegame version 110.
 * C: town count (3 = high, 0 = very low)
 * D: industry count (4 = high, 0 = none)
 * E: inital loan (in GBP)
 * F: interest rate
 * G: running costs (0 = low, 2 = high)
 * H: construction speed of competitors (0 = very slow, 4 = very fast)
 * I: competitor intelligence. Deprecated since savegame version 110.
 * J: breakdowns (0 = off, 2 = normal)
 * K: subsidy multiplier (0 = 1.5, 3 = 4.0)
 * L: construction cost (0-2)
 * M: terrain type (0 = very flat, 3 = mountainous)
 * N: amount of water (0 = very low, 3 = high)
 * O: economy (0 = steady, 1 = fluctuating)
 * P: Train reversing (0 = end of line + stations, 1 = end of line)
 * Q: disasters
 * R: area restructuring (0 = permissive, 2 = hostile)
 * S: the difficulty level
 */
static const DifficultySettings _default_game_diff[3] = { /*
	 A, C, D,      E, F, G, H, J, K, L, M, N, O, P, Q, R, S*/
	{2, 2, 4, 300000, 2, 0, 2, 1, 2, 0, 1, 0, 0, 0, 0, 0, 0}, ///< easy
	{4, 2, 3, 150000, 3, 1, 3, 2, 1, 1, 2, 1, 1, 1, 1, 1, 1}, ///< medium
	{7, 3, 3, 100000, 4, 1, 3, 2, 0, 2, 3, 2, 1, 1, 1, 2, 2}, ///< hard
};

void SetDifficultyLevel(int mode, DifficultySettings *gm_opt)
{
	assert(mode <= 3);

	if (mode != 3) {
		*gm_opt = _default_game_diff[mode];
	} else {
		gm_opt->diff_level = 3;
	}
}

/** Checks if any settings are set to incorrect values, and sets them to correct values in that case. */
static void ValidateSettings()
{
	/* Force the difficulty levels to correct values if they are invalid. */
	if (_settings_newgame.difficulty.diff_level != 3) {
		SetDifficultyLevel(_settings_newgame.difficulty.diff_level, &_settings_newgame.difficulty);
	}

	/* Do not allow a custom sea level with the original land generator. */
	if (_settings_newgame.game_creation.land_generator == 0 &&
			_settings_newgame.difficulty.quantity_sea_lakes == CUSTOM_SEA_LEVEL_NUMBER_DIFFICULTY) {
		_settings_newgame.difficulty.quantity_sea_lakes = CUSTOM_SEA_LEVEL_MIN_PERCENTAGE;
	}
}

static bool DifficultyReset(int32 level)
{
	/* In game / in the scenario editor you can set the difficulty level only to custom. This is
	 * needed by the AI Gui code that sets the difficulty level when you change any AI settings. */
	if (_game_mode != GM_MENU && level != 3) return false;
	SetDifficultyLevel(level, &GetGameSettings().difficulty);
	return true;
}

static bool DifficultyChange(int32)
{
	if (_game_mode == GM_MENU) {
		if (_settings_newgame.difficulty.diff_level != 3) {
			ShowErrorMessage(STR_WARNING_DIFFICULTY_TO_CUSTOM, INVALID_STRING_ID, WL_WARNING);
			_settings_newgame.difficulty.diff_level = 3;
		}
		SetWindowClassesDirty(WC_SELECT_GAME);
	} else {
		_settings_game.difficulty.diff_level = 3;
	}

	/* If we are a network-client, update the difficult setting (if it is open).
	 * Use this instead of just dirtying the window because we need to load in
	 * the new difficulty settings */
	if (_networking && FindWindowById(WC_GAME_OPTIONS, 0) != NULL) {
		ShowGameDifficulty();
	}

	return true;
}

static bool DifficultyNoiseChange(int32 i)
{
	if (_game_mode == GM_NORMAL) {
		UpdateAirportsNoise();
		if (_settings_game.economy.station_noise_level) {
			InvalidateWindowClassesData(WC_TOWN_VIEW, 0);
		}
	}

	return DifficultyChange(i);
}

static bool MaxNoAIsChange(int32 i)
{
	if (GetGameSettings().difficulty.max_no_competitors != 0 &&
#ifdef ENABLE_AI
			AI::GetInfoList()->size() == 0 &&
#endif /* ENABLE_AI */
			(!_networking || _network_server)) {
		ShowErrorMessage(STR_WARNING_NO_SUITABLE_AI, INVALID_STRING_ID, WL_CRITICAL);
	}

	return DifficultyChange(i);
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
static int32 ConvertLandscape(const char *value)
{
	/* try with the old values */
	return LookupOneOfMany("normal|hilly|desert|candy", value);
}

static bool CheckFreeformEdges(int32 p1)
{
	if (_game_mode == GM_MENU) return true;
	if (p1 != 0) {
		Ship *s;
		FOR_ALL_SHIPS(s) {
			if (TileX(s->tile) == 0 || TileY(s->tile) == 0) {
				ShowErrorMessage(STR_CONFIG_SETTING_EDGES_NOT_EMPTY, INVALID_STRING_ID, WL_ERROR);
				return false;
			}
		}
		Station *st;
		FOR_ALL_STATIONS(st) {
			if (TileX(st->xy) == 0 || TileY(st->xy) == 0) {
				ShowErrorMessage(STR_CONFIG_SETTING_EDGES_NOT_EMPTY, INVALID_STRING_ID, WL_ERROR);
				return false;
			}
		}
		for (uint i = 0; i < MapSizeX(); i++) MakeVoid(TileXY(i, 0));
		for (uint i = 0; i < MapSizeY(); i++) MakeVoid(TileXY(0, i));
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

	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (IsCompanyBuildableVehicleType(v)) {
			ShowErrorMessage(STR_CONFIG_SETTING_DYNAMIC_ENGINES_EXISTING_VEHICLES, INVALID_STRING_ID, WL_ERROR);
			return false;
		}
	}

	/* Reset the engines, they will get new EngineIDs */
	_engine_mngr.ResetToDefaultMapping();
	ReloadNewGRFData();

	return true;
}

static bool StationCatchmentChanged(int32 p1)
{
	Station::RecomputeIndustriesNearForAll();
	return true;
}

#ifdef ENABLE_NETWORK

static bool UpdateClientName(int32 p1)
{
	NetworkUpdateClientName();
	return true;
}

static bool UpdateServerPassword(int32 p1)
{
	if (strcmp(_settings_client.network.server_password, "*") == 0) {
		_settings_client.network.server_password[0] = '\0';
	}

	return true;
}

static bool UpdateRconPassword(int32 p1)
{
	if (strcmp(_settings_client.network.rcon_password, "*") == 0) {
		_settings_client.network.rcon_password[0] = '\0';
	}

	return true;
}

static bool UpdateClientConfigValues(int32 p1)
{
	if (_network_server) NetworkServerSendConfigUpdate();

	return true;
}

#endif /* ENABLE_NETWORK */


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
	uint options_to_load = GAME_DIFFICULTY_NUM - ((savegame && IsSavegameVersionBefore(4)) ? 1 : 0);

	if (!savegame) {
		/* If we did read to old_diff_custom, then at least one value must be non 0. */
		bool old_diff_custom_used = false;
		for (uint i = 0; i < options_to_load && !old_diff_custom_used; i++) {
			old_diff_custom_used = (_old_diff_custom[i] != 0);
		}

		if (!old_diff_custom_used) return;
	}

	for (uint i = 0; i < options_to_load; i++) {
		const SettingDesc *sd = &_settings[i];
		/* Skip deprecated options */
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		void *var = GetVariableAddress(savegame ? &_settings_game : &_settings_newgame, &sd->save);
		Write_ValidateSetting(var, sd, (int32)((i == 4 ? 1000 : 1) * _old_diff_custom[i]));
	}
}

/**
 * tries to convert newly introduced news settings based on old ones
 * @param name pointer to the string defining name of the old news config
 * @param value pointer to the string defining value of the old news config
 * @returns true if conversion could have been made
 */
static bool ConvertOldNewsSetting(const char *name, const char *value)
{
	if (strcasecmp(name, "openclose") == 0) {
		/* openclose has been split in "open" and "close".
		 * So the job is now to decrypt the value of the old news config
		 * and give it to the two newly introduced ones*/

		NewsDisplay display = ND_OFF; // default
		if (strcasecmp(value, "full") == 0) {
			display = ND_FULL;
		} else if (strcasecmp(value, "summarized") == 0) {
			display = ND_SUMMARY;
		}
		/* tranfert of values */
		_news_type_data[NT_INDUSTRY_OPEN].display = display;
		_news_type_data[NT_INDUSTRY_CLOSE].display = display;
		return true;
	}
	return false;
}

/**
 * Load newstype settings from a configuration file.
 * @param ini the configuration to read from.
 * @param grpname Name of the group containing the news type settings.
 */
static void NewsDisplayLoadConfig(IniFile *ini, const char *grpname)
{
	IniGroup *group = ini->GetGroup(grpname);
	IniItem *item;

	/* If no group exists, return */
	if (group == NULL) return;

	for (item = group->item; item != NULL; item = item->next) {
		int news_item = -1;
		for (int i = 0; i < NT_END; i++) {
			if (strcasecmp(item->name, _news_type_data[i].name) == 0) {
				news_item = i;
				break;
			}
		}

		/* the config been read is not within current aceptable config */
		if (news_item == -1) {
			/* if the conversion function cannot process it, advice by a debug warning*/
			if (!ConvertOldNewsSetting(item->name, item->value)) {
				DEBUG(misc, 0, "Invalid display option: %s", item->name);
			}
			/* in all cases, there is nothing left to do */
			continue;
		}

		if (StrEmpty(item->value)) {
			DEBUG(misc, 0, "Empty display value for newstype %s", item->name);
			continue;
		} else if (strcasecmp(item->value, "full") == 0) {
			_news_type_data[news_item].display = ND_FULL;
		} else if (strcasecmp(item->value, "off") == 0) {
			_news_type_data[news_item].display = ND_OFF;
		} else if (strcasecmp(item->value, "summarized") == 0) {
			_news_type_data[news_item].display = ND_SUMMARY;
		} else {
			DEBUG(misc, 0, "Invalid display value for newstype %s: %s", item->name, item->value);
			continue;
		}
	}
}

static void AILoadConfig(IniFile *ini, const char *grpname)
{
#ifdef ENABLE_AI
	IniGroup *group = ini->GetGroup(grpname);
	IniItem *item;

	/* Clean any configured AI */
	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		AIConfig::GetConfig(c, AIConfig::AISS_FORCE_NEWGAME)->ChangeAI(NULL);
	}

	/* If no group exists, return */
	if (group == NULL) return;

	CompanyID c = COMPANY_FIRST;
	for (item = group->item; c < MAX_COMPANIES && item != NULL; c++, item = item->next) {
		AIConfig *config = AIConfig::GetConfig(c, AIConfig::AISS_FORCE_NEWGAME);

		config->ChangeAI(item->name);
		if (!config->HasAI()) {
			if (strcmp(item->name, "none") != 0) {
				DEBUG(ai, 0, "The AI by the name '%s' was no longer found, and removed from the list.", item->name);
				continue;
			}
		}
		if (item->value != NULL) config->StringToSettings(item->value);
	}
#endif /* ENABLE_AI */
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
	GRFConfig *first = NULL;
	GRFConfig **curr = &first;

	if (group == NULL) return NULL;

	for (item = group->item; item != NULL; item = item->next) {
		GRFConfig *c = new GRFConfig(item->name);

		/* Parse parameters */
		if (!StrEmpty(item->value)) {
			c->num_params = ParseIntList(item->value, (int*)c->param, lengthof(c->param));
			if (c->num_params == (byte)-1) {
				ShowInfoF("ini: error in array '%s'", item->name);
				c->num_params = 0;
			}
		}

		/* Check if item is valid */
		if (!FillGRFDetails(c, is_static) || HasBit(c->flags, GCF_INVALID)) {
			const char *msg;

			if (c->status == GCS_NOT_FOUND) {
				msg = "not found";
			} else if (HasBit(c->flags, GCF_UNSAFE)) {
				msg = "unsafe for static use";
			} else if (HasBit(c->flags, GCF_SYSTEM)) {
				msg = "system NewGRF";
			} else if (HasBit(c->flags, GCF_INVALID)) {
				msg = "incompatible to this version of OpenTTD";
			} else {
				msg = "unknown";
			}

			ShowInfoF("ini: ignoring invalid NewGRF '%s': %s", item->name, msg);
			delete c;
			continue;
		}

		/* Check for duplicate GRFID (will also check for duplicate filenames) */
		bool duplicate = false;
		for (const GRFConfig *gc = first; gc != NULL; gc = gc->next) {
			if (gc->ident.grfid == c->ident.grfid) {
				ShowInfoF("ini: ignoring  NewGRF '%s': duplicate GRF ID with '%s'", item->name, gc->filename);
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

/**
 * Write newstype settings to a configuration file.
 * @param ini     The configuration to write to.
 * @param grpname Name of the group containing the news type settings.
 */
static void NewsDisplaySaveConfig(IniFile *ini, const char *grpname)
{
	IniGroup *group = ini->GetGroup(grpname);

	for (int i = 0; i < NT_END; i++) {
		const char *value;
		int v = _news_type_data[i].display;

		value = (v == ND_OFF ? "off" : (v == ND_SUMMARY ? "summarized" : "full"));

		group->GetItem(_news_type_data[i].name, true)->SetValue(value);
	}
}

static void AISaveConfig(IniFile *ini, const char *grpname)
{
#ifdef ENABLE_AI
	IniGroup *group = ini->GetGroup(grpname);

	if (group == NULL) return;
	group->Clear();

	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		AIConfig *config = AIConfig::GetConfig(c, AIConfig::AISS_FORCE_NEWGAME);
		const char *name;
		char value[1024];
		config->SettingsToString(value, lengthof(value));

		if (config->HasAI()) {
			name = config->GetName();
		} else {
			name = "none";
		}

		IniItem *item = new IniItem(group, name, strlen(name));
		item->SetValue(value);
	}
#endif /* ENABLE_AI */
}

/**
 * Save the version of OpenTTD to the ini file.
 * @param ini the ini to write to
 */
static void SaveVersionInConfig(IniFile *ini)
{
	IniGroup *group = ini->GetGroup("version");

	char version[9];
	snprintf(version, lengthof(version), "%08X", _openttd_newgrf_version);

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

	for (c = list; c != NULL; c = c->next) {
		char params[512];
		GRFBuildParamList(params, c, lastof(params));

		group->GetItem(c->filename, true)->SetValue(params);
	}
}

/* Common handler for saving/loading variables to the configuration file */
static void HandleSettingDescs(IniFile *ini, SettingDescProc *proc, SettingDescProcList *proc_list)
{
	proc(ini, (const SettingDesc*)_misc_settings,    "misc",  NULL);
	proc(ini, (const SettingDesc*)_music_settings,   "music", &_msf);
#if defined(WIN32) && !defined(DEDICATED)
	proc(ini, (const SettingDesc*)_win32_settings,   "win32", NULL);
#endif /* WIN32 */

	proc(ini, _settings,         "patches",  &_settings_newgame);
	proc(ini, _currency_settings,"currency", &_custom_currency);
	proc(ini, _company_settings, "company",  &_settings_client.company);

#ifdef ENABLE_NETWORK
	proc_list(ini, "server_bind_addresses", &_network_bind_list);
	proc_list(ini, "servers", &_network_host_list);
	proc_list(ini, "bans",    &_network_ban_list);
#endif /* ENABLE_NETWORK */
}

static IniFile *IniLoadConfig()
{
	IniFile *ini = new IniFile(_list_group_names);
	ini->LoadFromDisk(_config_file);
	return ini;
}

/** Load the values from the configuration files */
void LoadFromConfig()
{
	IniFile *ini = IniLoadConfig();
	ResetCurrencies(false); // Initialize the array of curencies, without preserving the custom one

	HandleSettingDescs(ini, IniLoadSettings, IniLoadSettingList);
	_grfconfig_newgame = GRFLoadConfig(ini, "newgrf", false);
	_grfconfig_static  = GRFLoadConfig(ini, "newgrf-static", true);
	NewsDisplayLoadConfig(ini, "news_display");
	AILoadConfig(ini, "ai_players");

	PrepareOldDiffCustom();
	IniLoadSettings(ini, _gameopt_settings, "gameopt", &_settings_newgame);
	HandleOldDiffCustom(false);

	ValidateSettings();
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
	NewsDisplaySaveConfig(ini, "news_display");
	AISaveConfig(ini, "ai_players");
	SaveVersionInConfig(ini);
	ini->SaveToDisk(_config_file);
	delete ini;
}

/**
 * Get the list of known NewGrf presets.
 * @param list[inout] Pointer to list for storing the preset names.
 */
void GetGRFPresetList(GRFPresetList *list)
{
	list->Clear();

	IniFile *ini = IniLoadConfig();
	IniGroup *group;
	for (group = ini->group; group != NULL; group = group->next) {
		if (strncmp(group->name, "preset-", 7) == 0) {
			*list->Append() = strdup(group->name + 7);
		}
	}

	delete ini;
}

/**
 * Load a NewGRF configuration by preset-name.
 * @param config_name Name of the preset.
 * @return NewGRF configuration.
 * @see GetGRFPresetList
 */
GRFConfig *LoadGRFPresetFromConfig(const char *config_name)
{
	char *section = (char*)alloca(strlen(config_name) + 8);
	sprintf(section, "preset-%s", config_name);

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
	char *section = (char*)alloca(strlen(config_name) + 8);
	sprintf(section, "preset-%s", config_name);

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
	char *section = (char*)alloca(strlen(config_name) + 8);
	sprintf(section, "preset-%s", config_name);

	IniFile *ini = IniLoadConfig();
	ini->RemoveGroup(section);
	ini->SaveToDisk(_config_file);
	delete ini;
}

static const SettingDesc *GetSettingDescription(uint index)
{
	if (index >= lengthof(_settings)) return NULL;
	return &_settings[index];
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

	if (sd == NULL) return CMD_ERROR;
	if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) return CMD_ERROR;

	if ((sd->desc.flags & SGF_NETWORK_ONLY) && !_networking && _game_mode != GM_MENU) return CMD_ERROR;
	if ((sd->desc.flags & SGF_NO_NETWORK) && _networking) return CMD_ERROR;
	if ((sd->desc.flags & SGF_NEWGAME_ONLY) &&
			(_game_mode == GM_NORMAL ||
			(_game_mode == GM_EDITOR && (sd->desc.flags & SGF_SCENEDIT_TOO) == 0))) {
		return CMD_ERROR;
	}

	if (flags & DC_EXEC) {
		void *var = GetVariableAddress(&GetGameSettings(), &sd->save);

		int32 oldval = (int32)ReadValue(var, sd->save.conv);
		int32 newval = (int32)p2;

		Write_ValidateSetting(var, sd, newval);
		newval = (int32)ReadValue(var, sd->save.conv);

		if (oldval == newval) return CommandCost();

		if (sd->desc.proc != NULL && !sd->desc.proc(newval)) {
			WriteValue(var, sd->save.conv, (int64)oldval);
			return CommandCost();
		}

		if (sd->desc.flags & SGF_NO_NETWORK) {
			GamelogStartAction(GLAT_SETTING);
			GamelogSetting(sd->desc.name, oldval, newval);
			GamelogStopAction();
		}

		SetWindowDirty(WC_GAME_OPTIONS, 0);
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
	if (p1 >= lengthof(_company_settings)) return CMD_ERROR;
	const SettingDesc *sd = &_company_settings[p1];

	if (flags & DC_EXEC) {
		void *var = GetVariableAddress(&Company::Get(_current_company)->settings, &sd->save);

		int32 oldval = (int32)ReadValue(var, sd->save.conv);
		int32 newval = (int32)p2;

		Write_ValidateSetting(var, sd, newval);
		newval = (int32)ReadValue(var, sd->save.conv);

		if (oldval == newval) return CommandCost();

		if (sd->desc.proc != NULL && !sd->desc.proc(newval)) {
			WriteValue(var, sd->save.conv, (int64)oldval);
			return CommandCost();
		}

		SetWindowDirty(WC_GAME_OPTIONS, 0);
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
bool SetSettingValue(uint index, int32 value, bool force_newgame)
{
	const SettingDesc *sd = &_settings[index];
	/* If an item is company-based, we do not send it over the network
	 * (if any) to change. Also *hack*hack* we update the _newgame version
	 * of settings because changing a company-based setting in a game also
	 * changes its defaults. At least that is the convention we have chosen */
	if (sd->save.conv & SLF_NETWORK_NO) {
		void *var = GetVariableAddress(&GetGameSettings(), &sd->save);
		Write_ValidateSetting(var, sd, value);

		if (_game_mode != GM_MENU) {
			void *var2 = GetVariableAddress(&_settings_newgame, &sd->save);
			Write_ValidateSetting(var2, sd, value);
		}
		if (sd->desc.proc != NULL) sd->desc.proc((int32)ReadValue(var, sd->save.conv));
		SetWindowDirty(WC_GAME_OPTIONS, 0);
		return true;
	}

	if (force_newgame) {
		void *var2 = GetVariableAddress(&_settings_newgame, &sd->save);
		Write_ValidateSetting(var2, sd, value);
		return true;
	}

	/* send non-company-based settings over the network */
	if (!_networking || (_networking && _network_server)) {
		return DoCommandP(0, index, value, CMD_CHANGE_SETTING);
	}
	return false;
}

/**
 * Top function to save the new value of an element of the Settings struct
 * @param index offset in the SettingDesc array of the CompanySettings struct
 * which identifies the setting member we want to change
 * @param value new value of the setting
 */
void SetCompanySetting(uint index, int32 value)
{
	const SettingDesc *sd = &_company_settings[index];
	if (Company::IsValidID(_local_company) && _game_mode != GM_MENU) {
		DoCommandP(0, index, value, CMD_CHANGE_COMPANY_SETTING);
	} else {
		void *var = GetVariableAddress(&_settings_client.company, &sd->save);
		Write_ValidateSetting(var, sd, value);
		if (sd->desc.proc != NULL) sd->desc.proc((int32)ReadValue(var, sd->save.conv));
	}
}

/**
 * Set the company settings for a new company to their default values.
 */
void SetDefaultCompanySettings(CompanyID cid)
{
	Company *c = Company::Get(cid);
	const SettingDesc *sd;
	for (sd = _company_settings; sd->save.cmd != SL_END; sd++) {
		void *var = GetVariableAddress(&c->settings, &sd->save);
		Write_ValidateSetting(var, sd, (int32)(size_t)sd->desc.def);
	}
}

#if defined(ENABLE_NETWORK)
/**
 * Sync all company settings in a multiplayer game.
 */
void SyncCompanySettings()
{
	const SettingDesc *sd;
	uint i = 0;
	for (sd = _company_settings; sd->save.cmd != SL_END; sd++, i++) {
		const void *old_var = GetVariableAddress(&Company::Get(_current_company)->settings, &sd->save);
		const void *new_var = GetVariableAddress(&_settings_client.company, &sd->save);
		uint32 old_value = (uint32)ReadValue(old_var, sd->save.conv);
		uint32 new_value = (uint32)ReadValue(new_var, sd->save.conv);
		if (old_value != new_value) NetworkSendCommand(0, i, new_value, CMD_CHANGE_COMPANY_SETTING, NULL, NULL, _local_company);
	}
}
#endif /* ENABLE_NETWORK */

/**
 * Get the index in the _company_settings array of a setting
 * @param name The name of the setting
 * @return The index in the _company_settings array
 */
uint GetCompanySettingIndex(const char *name)
{
	uint i;
	const SettingDesc *sd = GetSettingFromName(name, &i);
	assert(sd != NULL && (sd->desc.flags & SGF_PER_COMPANY) != 0);
	return i;
}

/**
 * Set a setting value with a string.
 * @param index the settings index.
 * @param value the value to write
 * @param force_newgame force the newgame settings
 * @note Strings WILL NOT be synced over the network
 */
bool SetSettingValue(uint index, const char *value, bool force_newgame)
{
	const SettingDesc *sd = &_settings[index];
	assert(sd->save.conv & SLF_NETWORK_NO);

	if (GetVarMemType(sd->save.conv) == SLE_VAR_STRQ) {
		char **var = (char**)GetVariableAddress((_game_mode == GM_MENU || force_newgame) ? &_settings_newgame : &_settings_game, &sd->save);
		free(*var);
		*var = strcmp(value, "(null)") == 0 ? NULL : strdup(value);
	} else {
		char *var = (char*)GetVariableAddress(NULL, &sd->save);
		ttd_strlcpy(var, value, sd->save.length);
	}
	if (sd->desc.proc != NULL) sd->desc.proc(0);

	return true;
}

/**
 * Given a name of setting, return a setting description of it.
 * @param name  Name of the setting to return a setting description of
 * @param i     Pointer to an integer that will contain the index of the setting after the call, if it is successful.
 * @return Pointer to the setting description of setting \a name if it can be found,
 *         \c NULL indicates failure to obtain the description
 */
const SettingDesc *GetSettingFromName(const char *name, uint *i)
{
	const SettingDesc *sd;

	/* First check all full names */
	for (*i = 0, sd = _settings; sd->save.cmd != SL_END; sd++, (*i)++) {
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (strcmp(sd->desc.name, name) == 0) return sd;
	}

	/* Then check the shortcut variant of the name. */
	for (*i = 0, sd = _settings; sd->save.cmd != SL_END; sd++, (*i)++) {
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		const char *short_name = strchr(sd->desc.name, '.');
		if (short_name != NULL) {
			short_name++;
			if (strcmp(short_name, name) == 0) return sd;
		}
	}

	if (strncmp(name, "company.", 8) == 0) name += 8;
	/* And finally the company-based settings */
	for (*i = 0, sd = _company_settings; sd->save.cmd != SL_END; sd++, (*i)++) {
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (strcmp(sd->desc.name, name) == 0) return sd;
	}

	return NULL;
}

/* Those 2 functions need to be here, else we have to make some stuff non-static
 * and besides, it is also better to keep stuff like this at the same place */
void IConsoleSetSetting(const char *name, const char *value, bool force_newgame)
{
	uint index;
	const SettingDesc *sd = GetSettingFromName(name, &index);

	if (sd == NULL) {
		IConsolePrintF(CC_WARNING, "'%s' is an unknown setting.", name);
		return;
	}

	bool success;
	if (sd->desc.cmd == SDT_STRING) {
		success = SetSettingValue(index, value, force_newgame);
	} else {
		uint32 val;
		extern bool GetArgumentInteger(uint32 *value, const char *arg);
		success = GetArgumentInteger(&val, value);
		if (!success) {
			IConsolePrintF(CC_ERROR, "'%s' is not an integer.", value);
			return;
		}

		success = SetSettingValue(index, val, force_newgame);
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
	uint index;
	const SettingDesc *sd = GetSettingFromName(name, &index);
	assert(sd != NULL);
	SetSettingValue(index, value);
}

/**
 * Output value of a specific setting to the console
 * @param name  Name of the setting to output its value
 * @param force_newgame force the newgame settings
 */
void IConsoleGetSetting(const char *name, bool force_newgame)
{
	char value[20];
	uint index;
	const SettingDesc *sd = GetSettingFromName(name, &index);
	const void *ptr;

	if (sd == NULL) {
		IConsolePrintF(CC_WARNING, "'%s' is an unknown setting.", name);
		return;
	}

	ptr = GetVariableAddress((_game_mode == GM_MENU || force_newgame) ? &_settings_newgame : &_settings_game, &sd->save);

	if (sd->desc.cmd == SDT_STRING) {
		IConsolePrintF(CC_WARNING, "Current value for '%s' is: '%s'", name, (GetVarMemType(sd->save.conv) == SLE_VAR_STRQ) ? *(const char **)ptr : (const char *)ptr);
	} else {
		if (sd->desc.cmd == SDT_BOOLX) {
			snprintf(value, sizeof(value), (*(bool*)ptr == 1) ? "on" : "off");
		} else {
			snprintf(value, sizeof(value), sd->desc.min < 0 ? "%d" : "%u", (int32)ReadValue(ptr, sd->save.conv));
		}

		IConsolePrintF(CC_WARNING, "Current value for '%s' is: '%s' (min: %s%d, max: %u)",
			name, value, (sd->desc.flags & SGF_0ISDISABLED) ? "(0) " : "", sd->desc.min, sd->desc.max);
	}
}

/**
 * List all settings and their value to the console
 *
 * @param prefilter  If not \c NULL, only list settings with names that begin with \a prefilter prefix
 */
void IConsoleListSettings(const char *prefilter)
{
	IConsolePrintF(CC_WARNING, "All settings with their current value:");

	for (const SettingDesc *sd = _settings; sd->save.cmd != SL_END; sd++) {
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (prefilter != NULL && strstr(sd->desc.name, prefilter) == NULL) continue;
		char value[80];
		const void *ptr = GetVariableAddress(&GetGameSettings(), &sd->save);

		if (sd->desc.cmd == SDT_BOOLX) {
			snprintf(value, lengthof(value), (*(bool*)ptr == 1) ? "on" : "off");
		} else if (sd->desc.cmd == SDT_STRING) {
			snprintf(value, sizeof(value), "%s", (GetVarMemType(sd->save.conv) == SLE_VAR_STRQ) ? *(const char **)ptr : (const char *)ptr);
		} else {
			snprintf(value, lengthof(value), sd->desc.min < 0 ? "%d" : "%u", (int32)ReadValue(ptr, sd->save.conv));
		}
		IConsolePrintF(CC_DEFAULT, "%s = %s", sd->desc.name, value);
	}

	IConsolePrintF(CC_WARNING, "Use 'setting' command to change a value");
}

/**
 * Save and load handler for settings
 * @param osd SettingDesc struct containing all information
 * @param object can be either NULL in which case we load global variables or
 * a pointer to a struct which is getting saved
 */
static void LoadSettings(const SettingDesc *osd, void *object)
{
	for (; osd->save.cmd != SL_END; osd++) {
		const SaveLoad *sld = &osd->save;
		void *ptr = GetVariableAddress(object, sld);

		if (!SlObjectMember(ptr, sld)) continue;
		if (IsNumericType(sld->conv)) Write_ValidateSetting(ptr, osd, ReadValue(ptr, sld->conv));
	}
}

/**
 * Save and load handler for settings
 * @param sd SettingDesc struct containing all information
 * @param object can be either NULL in which case we load global variables or
 * a pointer to a struct which is getting saved
 */
static void SaveSettings(const SettingDesc *sd, void *object)
{
	/* We need to write the CH_RIFF header, but unfortunately can't call
	 * SlCalcLength() because we have a different format. So do this manually */
	const SettingDesc *i;
	size_t length = 0;
	for (i = sd; i->save.cmd != SL_END; i++) {
		length += SlCalcObjMemberLength(object, &i->save);
	}
	SlSetLength(length);

	for (i = sd; i->save.cmd != SL_END; i++) {
		void *ptr = GetVariableAddress(object, &i->save);
		SlObjectMember(ptr, &i->save);
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
	 * signal_side stays when joining a network-server */
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

void CheckConfig()
{
	/*
	 * Increase old default values for pf_maxdepth and pf_maxlength
	 * to support big networks.
	 */
	if (_settings_newgame.pf.opf.pf_maxdepth == 16 && _settings_newgame.pf.opf.pf_maxlength == 512) {
		_settings_newgame.pf.opf.pf_maxdepth = 48;
		_settings_newgame.pf.opf.pf_maxlength = 4096;
	}
}

extern const ChunkHandler _setting_chunk_handlers[] = {
	{ 'OPTS', NULL,      Load_OPTS, NULL, NULL,       CH_RIFF},
	{ 'PATS', Save_PATS, Load_PATS, NULL, Check_PATS, CH_RIFF | CH_LAST},
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
