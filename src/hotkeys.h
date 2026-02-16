/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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
	 * @param other The list not to copy from.
	 */
	HotkeyList(const HotkeyList &other);
};

bool IsQuitKey(uint16_t keycode);

void LoadHotkeysFromConfig();
void SaveHotkeysToConfig();

void HandleGlobalHotkeys(char32_t key, uint16_t keycode);

static constexpr int SPECIAL_HOTKEY_BIT = 30; ///< Bit which denotes that hotkey isn't bound to UI button.

/**
 * Checks if hotkey index is special or not.
 * @param hotkey Hotkey index to check.
 * @return True iff the index is for special hotkey.
 */
inline bool IsSpecialHotkey(const int &hotkey)
{
	return HasBit(hotkey, SPECIAL_HOTKEY_BIT);
}

/**
 * Indexes for special hotkeys to navigate in lists.
 * Values have to have SPECIAL_HOTKEY_BIT set.
 */
enum class SpecialListHotkeys : int {
	PreviousItem = 1 << SPECIAL_HOTKEY_BIT, ///< Hotkey to select previous item in the list.
	NextItem, ///< Hotkey to select next item in the list.
	FirstItem, ///< Hotkey to select first item in the list.
	LastItem, ///< Hotkey to select last item in the list.
};

/**
 * Gets the first index in the list for given hotkey and the step to look for another if first is invalid.
 * @tparam ItemType The type of items stored in the list.
 * @tparam ListType The type of the list. For example: std::vector<ItemType>, std::list<ItemType>.
 * @param hotkey The special hotkey to get the index and step for.
 * @param list The list containing items.
 * @param current_item The item that is currently chosen, used for next and previous hotkeys.
 * @return Tuple containing index as first element and step as second. The step for backward direction has positive value, use `% list.size()` to remain in bounds.
 */
template<class ItemType, class ListType>
std::tuple<size_t, size_t> GetListIndexStep(SpecialListHotkeys hotkey, const ListType &list, const ItemType &current_item)
{
	/* Don't use -1, because how % is implemented for negative numbers. */
	size_t step_back = list.size() - 1;
	auto get_relative_index_step = [list, current_item](size_t step) -> std::tuple<size_t, size_t> {
		size_t index = std::distance(list.begin(), std::ranges::find(list, current_item));
		return {(index + step) % list.size(), step};
	};

	switch (hotkey) {
		default: NOT_REACHED();
		case SpecialListHotkeys::FirstItem:
			return {0, 1};

		case SpecialListHotkeys::LastItem:
			return {list.size() - 1, step_back};

		case SpecialListHotkeys::PreviousItem:
			return get_relative_index_step(step_back);

		case SpecialListHotkeys::NextItem:
			return get_relative_index_step(1);
	}
}

#endif /* HOTKEYS_H */
