/* $Id$ */

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

/**
 * All data for a single hotkey. The name (for saving/loading a configfile),
 * a list of keycodes and a number to help identifying this hotkey.
 */
template<class T>
struct Hotkey {
	typedef void (T::*hotkey_callback)(int);

	/**
	 * A wrapper around the callback function. This wrapper is needed because
	 * the size of a pointer to a member function depends on the class
	 * definition. The possible solutions are to either wrap the callback
	 * pointer in a class and dynamically allocate memory for it like we do
	 * now or making all class definitions available in hotkeys.cpp.
	 */
	struct CallbackWrapper {
		CallbackWrapper(hotkey_callback callback) :
			callback(callback)
		{}
		hotkey_callback callback;
	};

	/**
	 * Create a new Hotkey object with a single default keycode.
	 * @param default_keycode The default keycode for this hotkey.
	 * @param name The name of this hotkey.
	 * @param num Number of this hotkey, should be unique within the hotkey list.
	 * @param callback The function to call if the hotkey is pressed.
	 */
	Hotkey(uint16 default_keycode, const char *name, int num, hotkey_callback callback = NULL) :
		name(name),
		num(num)
	{
		if (callback == NULL) {
			this->callback = NULL;
		} else {
			this->callback = new CallbackWrapper(callback);
		}
		if (default_keycode != 0) this->AddKeycode(default_keycode);
	}

	/**
	 * Create a new Hotkey object with multiple default keycodes.
	 * @param default_keycodes An array of default keycodes terminated with 0.
	 * @param name The name of this hotkey.
	 * @param num Number of this hotkey, should be unique within the hotkey list.
	 * @param callback The function to call if the hotkey is pressed.
	 */
	Hotkey(const uint16 *default_keycodes, const char *name, int num, hotkey_callback callback = NULL) :
		name(name),
		num(num)
	{
		if (callback == NULL) {
			this->callback = NULL;
		} else {
			this->callback = new CallbackWrapper(callback);
		}

		const uint16 *keycode = default_keycodes;
		while (*keycode != 0) {
			this->AddKeycode(*keycode);
			keycode++;
		}
	}

	~Hotkey()
	{
		delete this->callback;
	}

	/**
	 * Add a keycode to this hotkey, from now that keycode will be matched
	 * in addition to any previously added keycodes.
	 * @param keycode The keycode to add.
	 */
	void AddKeycode(uint16 keycode)
	{
		this->keycodes.Include(keycode);
	}

	const char *name;
	int num;
	SmallVector<uint16, 1> keycodes;
	CallbackWrapper *callback;
};

#define HOTKEY_LIST_END(window_class) Hotkey<window_class>((uint16)0, NULL, -1)

/**
 * Check if a keycode is bound to something.
 * @param list The list with hotkeys to check
 * @param keycode The keycode that was pressed
 * @param w The window-pointer to give to the callback function (if any).
 * @param global_only Limit the search to hotkeys defined as 'global'.
 * @return The number of the matching hotkey or -1.
 */
template<class T>
int CheckHotkeyMatch(Hotkey<T> *list, uint16 keycode, T *w, bool global_only = false)
{
	while (list->num != -1) {
		if (list->keycodes.Contains(keycode | WKC_GLOBAL_HOTKEY) || (!global_only && list->keycodes.Contains(keycode))) {
			if (!global_only && list->callback != NULL) (w->*(list->callback->callback))(-1);
			return list->num;
		}
		list++;
	}
	return -1;
}

bool IsQuitKey(uint16 keycode);

void LoadHotkeysFromConfig();
void SaveHotkeysToConfig();


void HandleGlobalHotkeys(uint16 key, uint16 keycode);

#endif /* HOTKEYS_H */
