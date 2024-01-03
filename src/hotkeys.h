/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file hotkeys.h %Hotkey related functions. */

#ifndef HOTKEYS_H
#define HOTKEYS_H

#include "gfx_type.h"
#include "window_type.h"
#include "string_type.h"

/**
 * All data for a single hotkey. The name (for saving/loading a configfile),
 * a list of keycodes and a number to help identifying this hotkey.
 */
struct Hotkey {
	Hotkey(uint16_t default_keycode, const std::string &name, int num);
	Hotkey(const std::vector<uint16_t> &default_keycodes, const std::string &name, int num);

	void AddKeycode(uint16_t keycode);

	const std::string name;
	int num;
	std::set<uint16_t> keycodes;
};

struct IniFile;

/**
 * List of hotkeys for a window.
 */
struct HotkeyList {
	typedef EventState (*GlobalHotkeyHandlerFunc)(int hotkey);

	HotkeyList(const std::string &ini_group, const std::vector<Hotkey> &items, GlobalHotkeyHandlerFunc global_hotkey_handler = nullptr);
	~HotkeyList();

	void Load(const IniFile &ini);
	void Save(IniFile &ini) const;

	int CheckMatch(uint16_t keycode, bool global_only = false) const;

	GlobalHotkeyHandlerFunc global_hotkey_handler;
private:
	const std::string ini_group;
	std::vector<Hotkey> items;

	/**
	 * Dummy private copy constructor to prevent compilers from
	 * copying the structure, which fails due to _hotkey_lists.
	 */
	HotkeyList(const HotkeyList &other);
};

bool IsQuitKey(uint16_t keycode);

void LoadHotkeysFromConfig();
void SaveHotkeysToConfig();


void HandleGlobalHotkeys(char32_t key, uint16_t keycode);

#endif /* HOTKEYS_H */
