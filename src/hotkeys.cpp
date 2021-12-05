/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file hotkeys.cpp Implementation of hotkey related functions */

#include <sstream>

#include "stdafx.h"
#include "openttd.h"
#include "hotkeys.h"
#include "ini_type.h"
#include "string_func.h"
#include "window_gui.h"

#include "safeguards.h"

std::string _hotkeys_file;

/*
 * Returns the actual character on the backquote button (` on US keyboard)
 * TODO implement for other platforms.
 */
#ifdef _WIN32
#include <Windows.h>
const char* GetBackquoteButtonCharacter()
{
	std::stringstream ss;
	ss << static_cast<char>(MapVirtualKey(VK_OEM_3, MAPVK_VK_TO_CHAR));
	static std::string backquote = ss.str();
	return backquote.c_str();
}
#else
const char* GetBackquoteButtonCharacter() { return "`"; }
#endif

/**
 * List of all HotkeyLists.
 * This is a pointer to ensure initialisation order with the various static HotkeyList instances.
 */
static std::vector<HotkeyList*> *_hotkey_lists = nullptr;

/** String representation of a keycode */
struct KeycodeNames {
	const char *name;         ///< Name of the keycode
	WindowKeyCodes keycode;   ///< The keycode
	const char *display_name; ///< The "pretty" name used to display the hotkey in the GUI

	KeycodeNames(const char *name, WindowKeyCodes keycode, const char *display_name = nullptr)
	{
		this->name = name;
		this->keycode = keycode;
		this->display_name = display_name;
	}
};

/** Array of non-standard keycodes that can be used in the hotkeys config file. */
static const KeycodeNames _keycode_to_name[] = {
	{"SHIFT", WKC_SHIFT, "Shift"},
	{"CTRL", WKC_CTRL, "Ctrl"},
	{"ALT", WKC_ALT, "Alt"},
	{"META", WKC_META},
	{"GLOBAL", WKC_GLOBAL_HOTKEY},
	{"ESC", WKC_ESC, "Esc"},
	{"TAB", WKC_TAB, "Tab"},
	{"BACKSPACE", WKC_BACKSPACE, "Backspace"},
	{"INS", WKC_INSERT, "Insert"},
	{"DEL", WKC_DELETE, "Delete"},
	{"PAGEUP", WKC_PAGEUP, "Page up"},
	{"PAGEDOWN", WKC_PAGEDOWN, "Page down"},
	{"END", WKC_END, "End"},
	{"HOME", WKC_HOME, "Home"},
	{"RETURN", WKC_RETURN, "Enter"},
	{"SPACE", WKC_SPACE, "Space"},
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
	{"BACKQUOTE", WKC_BACKQUOTE, GetBackquoteButtonCharacter()},
	{"PAUSE", WKC_PAUSE, "Pause Break"},
	{"NUM_DIV", WKC_NUM_DIV, "Numpad /"},
	{"NUM_MUL", WKC_NUM_MUL, "Numpad *"},
	{"NUM_MINUS", WKC_NUM_MINUS, "Numpad -"},
	{"NUM_PLUS", WKC_NUM_PLUS, "Numpad +"},
	{"NUM_ENTER", WKC_NUM_ENTER, "Numpad Enter"},
	{"NUM_DOT", WKC_NUM_DECIMAL, "Numpad ."},
	{"SLASH", WKC_SLASH, "/"},
	{"/", WKC_SLASH}, /* deprecated, use SLASH */
	{"SEMICOLON", WKC_SEMICOLON, ";"},
	{";", WKC_SEMICOLON}, /* deprecated, use SEMICOLON */
	{"EQUALS", WKC_EQUALS, "="},
	{"=", WKC_EQUALS}, /* deprecated, use EQUALS */
	{"L_BRACKET", WKC_L_BRACKET, "["},
	{"[", WKC_L_BRACKET}, /* deprecated, use L_BRACKET */
	{"BACKSLASH", WKC_BACKSLASH, "\\"},
	{"\\", WKC_BACKSLASH}, /* deprecated, use BACKSLASH */
	{"R_BRACKET", WKC_R_BRACKET, "]"},
	{"]", WKC_R_BRACKET}, /* deprecated, use R_BRACKET */
	{"SINGLEQUOTE", WKC_SINGLEQUOTE, "'"},
	{"'", WKC_SINGLEQUOTE}, /* deprecated, use SINGLEQUOTE */
	{"COMMA", WKC_COMMA, ","},
	{"PERIOD", WKC_PERIOD, "."},
	{".", WKC_PERIOD}, /* deprecated, use PERIOD */
	{"MINUS", WKC_MINUS, "-"},
	{"-", WKC_MINUS}, /* deprecated, use MINUS */
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
	for (uint i = 0; i < lengthof(_keycode_to_name); i++) {
		if (strlen(_keycode_to_name[i].name) == (size_t)(end - start) && strncasecmp(start, _keycode_to_name[i].name, end - start) == 0) {
			return _keycode_to_name[i].keycode;
		}
	}
	if (end - start == 1) {
		if (*start >= 'a' && *start <= 'z') return *start - ('a'-'A');
		/* Ignore invalid keycodes */
		if (*(const uint8 *)start < 128) return *start;
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
	for (;;) {
		const char *cur = start;
		while (*cur != '+' && cur != end) cur++;
		uint16 code = ParseCode(start, cur);
		if (code == 0) return 0;
		if (code & WKC_SPECIAL_KEYS) {
			/* Some completely wrong keycode we don't support. */
			if (code & ~WKC_SPECIAL_KEYS) return 0;
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
static void ParseHotkeys(Hotkey *hotkey, const char *value)
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
 * config file. Separate parts of the keycode (like "CTRL" and "F1" are split
 * by a '+'.
 * @param keycode The keycode to convert to a string.
 * @return A string representation of this keycode.
 * @note The return value is a static buffer, stredup the result before calling
 *  this function again.
 */
static const char *KeycodeToString(uint16 keycode)
{
	static char buf[32];
	buf[0] = '\0';
	bool first = true;
	if (keycode & WKC_GLOBAL_HOTKEY) {
		strecat(buf, "GLOBAL", lastof(buf));
		first = false;
	}
	if (keycode & WKC_SHIFT) {
		if (!first) strecat(buf, "+", lastof(buf));
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
 * Convert a hotkey to it's "pretty" string representation so it can be displayed
 * @param keycode The keycode to convert to a string.
 * @return A string representation of this keycode.
 */
static std::string KeycodeToDisplayString(uint16 keycode)
{
	std::stringstream ss;

	auto append = [&ss](auto text) {
		if (ss.tellp() > 0) ss << "+";
		ss << text;
	};

	///* WKC_GLOBAL_HOTKEY is ignored here */
	if (keycode & WKC_SHIFT) append("Shift");
	if (keycode & WKC_CTRL) append("Ctrl");
	if (keycode & WKC_ALT) append("Alt");
	if (keycode & WKC_META) append("Meta");

	keycode = keycode & ~WKC_SPECIAL_KEYS;
	if (keycode == 0) return ss.str();

	for (uint i = 0; i < lengthof(_keycode_to_name); i++) {
		if (_keycode_to_name[i].keycode == keycode) {
			append(_keycode_to_name[i].display_name ? _keycode_to_name[i].display_name : _keycode_to_name[i].name);
			return ss.str();
		}
	}

	assert(keycode < 128);
	append((char)keycode);
	return ss.str();
}

/**
 * Convert all keycodes attached to a hotkey to a single string. If multiple
 * keycodes are attached to the hotkey they are split by a comma.
 * @param hotkey The keycodes of this hotkey need to be converted to a string.
 * @return A string representation of all keycodes.
 * @note The return value is a static buffer, stredup the result before calling
 *  this function again.
 */
const char *SaveKeycodes(const Hotkey *hotkey)
{
	static char buf[128];
	buf[0] = '\0';
	for (uint i = 0; i < hotkey->keycodes.size(); i++) {
		const char *str = KeycodeToString(hotkey->keycodes[i]);
		if (i > 0) strecat(buf, ",", lastof(buf));
		strecat(buf, str, lastof(buf));
	}
	return buf;
}

/**
 * Create a new Hotkey object with a single default keycode.
 * @param default_keycode The default keycode for this hotkey.
 * @param name The name of this hotkey.
 * @param num Number of this hotkey, should be unique within the hotkey list.
 */
Hotkey::Hotkey(uint16 default_keycode, const char *name, int num) :
	name(name),
	num(num)
{
	if (default_keycode != 0) this->AddKeycode(default_keycode);
}

/**
 * Create a new Hotkey object with multiple default keycodes.
 * @param default_keycodes An array of default keycodes terminated with 0.
 * @param name The name of this hotkey.
 * @param num Number of this hotkey, should be unique within the hotkey list.
 */
Hotkey::Hotkey(const uint16 *default_keycodes, const char *name, int num) :
	name(name),
	num(num)
{
	const uint16 *keycode = default_keycodes;
	while (*keycode != 0) {
		this->AddKeycode(*keycode);
		keycode++;
	}
}

/**
 * Add a keycode to this hotkey, from now that keycode will be matched
 * in addition to any previously added keycodes.
 * @param keycode The keycode to add.
 */
void Hotkey::AddKeycode(uint16 keycode)
{
	include(this->keycodes, keycode);
}

/**
 * Appends the hotkeys to the provided string.
 * @param input The original string to append to.
 * @param hotkey The hotkey to inspect and append. Can be NULL.
 */
std::string AppendHotkeyToString(const std::string& input, const Hotkey* hotkey)
{
	if (!hotkey) return input;

	std::stringstream ss;
	ss << input;
	if (!hotkey->keycodes.empty()) ss << " ";
	for (uint16 keycode : hotkey->keycodes)
	{
		const bool is_global = keycode & WKC_GLOBAL_HOTKEY;
		ss << (is_global ? "[" : "(");
		ss << KeycodeToDisplayString(keycode);
		ss << (is_global ? "]" : ")");
	}
	return ss.str();
}

HotkeyList::HotkeyList(const char *ini_group, Hotkey *items, GlobalHotkeyHandlerFunc global_hotkey_handler) :
	global_hotkey_handler(global_hotkey_handler), ini_group(ini_group), items(items)
{
	if (_hotkey_lists == nullptr) _hotkey_lists = new std::vector<HotkeyList*>();
	_hotkey_lists->push_back(this);
}

HotkeyList::~HotkeyList()
{
	_hotkey_lists->erase(std::find(_hotkey_lists->begin(), _hotkey_lists->end(), this));
}

/**
 * Load HotkeyList from IniFile.
 * @param ini IniFile to load from.
 */
void HotkeyList::Load(IniFile *ini)
{
	IniGroup *group = ini->GetGroup(this->ini_group);
	for (Hotkey *hotkey = this->items; hotkey->name != nullptr; ++hotkey) {
		IniItem *item = group->GetItem(hotkey->name, false);
		if (item != nullptr) {
			hotkey->keycodes.clear();
			if (item->value.has_value()) ParseHotkeys(hotkey, item->value->c_str());
		}
	}
}

/**
 * Save HotkeyList to IniFile.
 * @param ini IniFile to save to.
 */
void HotkeyList::Save(IniFile *ini) const
{
	IniGroup *group = ini->GetGroup(this->ini_group);
	for (const Hotkey *hotkey = this->items; hotkey->name != nullptr; ++hotkey) {
		IniItem *item = group->GetItem(hotkey->name, true);
		item->SetValue(SaveKeycodes(hotkey));
	}
}

/**
 * Check if a keycode is bound to something.
 * @param keycode The keycode that was pressed
 * @param global_only Limit the search to hotkeys defined as 'global'.
 * @return The number of the matching hotkey or -1.
 */
int HotkeyList::CheckMatch(uint16 keycode, bool global_only) const
{
	for (const Hotkey *list = this->items; list->name != nullptr; ++list) {
		auto begin = list->keycodes.begin();
		auto end = list->keycodes.end();
		if (std::find(begin, end, keycode | WKC_GLOBAL_HOTKEY) != end || (!global_only && std::find(begin, end, keycode) != end)) {
			return list->num;
		}
	}
	return -1;
}

/**
 * Returns the hotkey corresponding to a given number.
 * @param num The number to compare against
 * @return The hotkey if a match was found, NULL otherwise.
 */
const Hotkey* HotkeyList::GetHotkeyByNum(int num) const
{
	for (const Hotkey* hotkey = this->items; hotkey->name != nullptr; ++hotkey) {
		if (hotkey->num == num)
			return hotkey;
	}
	return nullptr;
}

static void SaveLoadHotkeys(bool save)
{
	IniFile *ini = new IniFile();
	ini->LoadFromDisk(_hotkeys_file, NO_DIRECTORY);

	for (HotkeyList *list : *_hotkey_lists) {
		if (save) {
			list->Save(ini);
		} else {
			list->Load(ini);
		}
	}

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

void HandleGlobalHotkeys(WChar key, uint16 keycode)
{
	for (HotkeyList *list : *_hotkey_lists) {
		if (list->global_hotkey_handler == nullptr) continue;

		int hotkey = list->CheckMatch(keycode, true);
		if (hotkey >= 0 && (list->global_hotkey_handler(hotkey) == ES_HANDLED)) return;
	}
}

