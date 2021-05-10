/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file hotkeys.h %Hotkey related functions. */

#ifndef HOTKEYS_H
#define HOTKEYS_H

#include "core/smallvec_type.hpp"
#include "gfx_type.h"
#include "window_type.h"
#include "string_type.h"

/**
 * All data for a single hotkey. The name (for saving/loading a configfile),
 * a list of keycodes and a number to help identifying this hotkey.
 */
struct Hotkey {
	Hotkey(uint16 default_keycode, const char *name, int num);
	Hotkey(const uint16 *default_keycodes, const char *name, int num);

	void AddKeycode(uint16 keycode);

	const char *name;
	int num;
	std::vector<uint16> keycodes;
};

#define HOTKEY_LIST_END Hotkey((uint16)0, nullptr, -1)

struct IniFile;

/**
 * List of hotkeys for a window.
 */
struct HotkeyList {
	typedef EventState (*GlobalHotkeyHandlerFunc)(int hotkey);

	HotkeyList(const char *ini_group, Hotkey *items, GlobalHotkeyHandlerFunc global_hotkey_handler = nullptr);
	~HotkeyList();

	void Load(IniFile *ini);
	void Save(IniFile *ini) const;

	int CheckMatch(uint16 keycode, bool global_only = false) const;

	GlobalHotkeyHandlerFunc global_hotkey_handler;
private:
	const char *ini_group;
	Hotkey *items;

	/**
	 * Dummy private copy constructor to prevent compilers from
	 * copying the structure, which fails due to _hotkey_lists.
	 */
	HotkeyList(const HotkeyList &other);
};

bool IsQuitKey(uint16 keycode);

void LoadHotkeysFromConfig();
void SaveHotkeysToConfig();


void HandleGlobalHotkeys(WChar key, uint16 keycode);

#endif /* HOTKEYS_H */
