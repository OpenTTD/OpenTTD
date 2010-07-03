/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file hotkeys.cpp Implementation of hotkey related functions */

#include "stdafx.h"
#include "hotkeys.h"
#include "ini_type.h"
#include "string_func.h"
#include "gfx_type.h"
#include <string.h>

char *_hotkeys_file;

/** String representation of a keycode */
struct KeycodeNames {
	const char *name;       ///< Name of the keycode
	WindowKeyCodes keycode; ///< The keycode
};

/** Array of non-standard keycodes that can be used in the hotkeys config file. */
static const KeycodeNames _keycode_to_name[] = {
	{"SHIFT", WKC_SHIFT},
	{"CTRL", WKC_CTRL},
	{"ALT", WKC_ALT},
	{"META", WKC_META},
	{"ESC", WKC_ESC},
	{"DEL", WKC_DELETE},
	{"RETURN", WKC_RETURN},
	{"BACKQUOTE", WKC_BACKQUOTE},
	{"F1", WKC_F1},
	{"F2", WKC_F2},
	{"F3", WKC_F3},
	{"F4", WKC_F4},
	{"F5", WKC_F5},
	{"F6", WKC_F6},
	{"F7", WKC_F7},
	{"F8", WKC_F8},
	{"F9", WKC_F9},
	{"F10", WKC_F10},
	{"F11", WKC_F11},
	{"F12", WKC_F12},
	{"PAUSE", WKC_PAUSE},
	{"PLUS", (WindowKeyCodes)'+'},
	{"COMMA", (WindowKeyCodes)','},
};

/**
 * Try to parse a single part of a keycode.
 * @param start Start of the string to parse.
 * @param end End of the string to parse.
 * @return A keycode if a match is found or 0.
 */
static uint16 ParseCode(const char *start, const char *end)
{
	assert(start <= end);
	while (start < end && *start == ' ') start++;
	while (end > start && *end == ' ') end--;
	if (end - start == 1) {
		if (*start >= 'a' && *start <= 'z') return *start - ('a' - 'A');
		return *start;
	}
	for (uint i = 0; i < lengthof(_keycode_to_name); i++) {
		if (strnicmp(start, _keycode_to_name[i].name, end - start) == 0) {
			return _keycode_to_name[i].keycode;
		}
	}
	return 0;
}

/**
 * Parse a string representation of a keycode.
 * @param start Start of the input.
 * @param end End of the input.
 * @return A valid keycode or 0.
 */
static uint16 ParseKeycode(const char *start, const char *end)
{
	assert(start <= end);
	uint16 keycode = 0;
	while (true) {
		const char *cur = start;
		while (*cur != '+' && cur != end) cur++;
		uint16 code = ParseCode(start, cur);
		if (code == 0) return 0;
		if (code & WKC_SPECIAL_KEYS) {
			keycode |= code;
		} else {
			/* Ignore the code if it has more then 1 letter. */
			if (keycode & ~WKC_SPECIAL_KEYS) return 0;
			keycode |= code;
		}
		if (cur == end) break;
		assert(cur < end);
		start = cur + 1;
	}
	return keycode;
}

/**
 * Parse a string to the keycodes it represents
 * @param hotkey The hotkey object to add the keycodes to
 * @param value The string to parse
 */
template<class T>
static void ParseHotkeys(Hotkey<T> *hotkey, const char *value)
{
	const char *start = value;
	while (*start != '\0') {
		const char *end = start;
		while (*end != '\0' && *end != ',') end++;
		uint16 keycode = ParseKeycode(start, end);
		if (keycode != 0) hotkey->AddKeycode(keycode);
		start = (*end == ',') ? end + 1: end;
	}
}

/**
 * Convert a hotkey to it's string representation so it can be written to the
 * config file. Seperate parts of the keycode (like "CTRL" and "F1" are split
 * by a '+'.
 * @param keycode The keycode to convert to a string.
 * @return A string representation of this keycode.
 * @note The return value is a static buffer, strdup the result before calling
 *  this function again.
 */
static const char *KeycodeToString(uint16 keycode)
{
	static char buf[32];
	buf[0] = '\0';
	bool first = true;
	if (keycode & WKC_SHIFT) {
		strecat(buf, "SHIFT", lastof(buf));
		first = false;
	}
	if (keycode & WKC_CTRL) {
		if (!first) strecat(buf, "+", lastof(buf));
		strecat(buf, "CTRL", lastof(buf));
		first = false;
	}
	if (keycode & WKC_ALT) {
		if (!first) strecat(buf, "+", lastof(buf));
		strecat(buf, "ALT", lastof(buf));
		first = false;
	}
	if (keycode & WKC_META) {
		if (!first) strecat(buf, "+", lastof(buf));
		strecat(buf, "META", lastof(buf));
		first = false;
	}
	if (!first) strecat(buf, "+", lastof(buf));
	keycode = keycode & ~WKC_SPECIAL_KEYS;

	for (uint i = 0; i < lengthof(_keycode_to_name); i++) {
		if (_keycode_to_name[i].keycode == keycode) {
			strecat(buf, _keycode_to_name[i].name, lastof(buf));
			return buf;
		}
	}
	assert(keycode < 128);
	char key[2];
	key[0] = keycode;
	key[1] = '\0';
	strecat(buf, key, lastof(buf));
	return buf;
}

/**
 * Convert all keycodes attached to a hotkey to a single string. If multiple
 * keycodes are attached to the hotkey they are split by a comma.
 * @param hotkey The keycodes of this hotkey need to be converted to a string.
 * @return A string representation of all keycodes.
 * @note The return value is a static buffer, strdup the result before calling
 *  this function again.
 */
template<class T>
const char *SaveKeycodes(const Hotkey<T> *hotkey)
{
	static char buf[128];
	buf[0] = '\0';
	for (uint i = 0; i < hotkey->keycodes.Length(); i++) {
		const char *str = KeycodeToString(hotkey->keycodes[i]);
		if (i > 0) strecat(buf, ",", lastof(buf));
		strecat(buf, str, lastof(buf));
	}
	return buf;
}

template<class T>
void LoadHotkeyGroup(IniGroup *group, T *hotkey_list)
{
	for (uint i = 0; hotkey_list[i].num != -1; i++) {
		T *hotkey = &hotkey_list[i];
		IniItem *item = group->GetItem(hotkey->name, false);
		if (item != NULL) {
			hotkey->keycodes.Clear();
			ParseHotkeys(hotkey, item->value);
		}
	}
}

template<class T>
void SaveHotkeyGroup(IniGroup *group, T *hotkey_list)
{
	for (uint i = 0; hotkey_list[i].num != -1; i++) {
		T *hotkey = &hotkey_list[i];
		IniItem *item = group->GetItem(hotkey->name, true);
		if (hotkey->keycodes.Length() > 0) {
			item->value = strdup(SaveKeycodes(hotkey));
		}
	}
}

template<class T>
void SaveLoadHotkeyGroup(IniGroup *group, T *hotkey_list, bool save)
{
	if (save) {
		SaveHotkeyGroup(group, hotkey_list);
	} else {
		LoadHotkeyGroup(group, hotkey_list);
	}
}

struct TerraformToolbarWindow;
struct ScenarioEditorLandscapeGenerationWindow;

static void SaveLoadHotkeys(bool save)
{
	IniFile *ini = new IniFile();
	ini->LoadFromDisk(_hotkeys_file);

	IniGroup *group;

#define SL_HOTKEYS(name, window_name) \
	extern Hotkey<window_name> *_##name##_hotkeys;\
	group = ini->GetGroup(#name);\
	SaveLoadHotkeyGroup(group, _##name##_hotkeys, save);

	SL_HOTKEYS(terraform, TerraformToolbarWindow);
	SL_HOTKEYS(terraform_editor, ScenarioEditorLandscapeGenerationWindow);


#undef SL_HOTKEYS
	if (save) ini->SaveToDisk(_hotkeys_file);
	delete ini;
}


/** Load the hotkeys from the config file */
void LoadHotkeysFromConfig()
{
	SaveLoadHotkeys(false);
}

/** Save the hotkeys to the config file */
void SaveHotkeysToConfig()
{
	SaveLoadHotkeys(true);
}

