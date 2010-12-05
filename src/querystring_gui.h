/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file querystring_gui.h Base for the GUIs that have an edit box in them. */

#ifndef QUERYSTRING_GUI_H
#define QUERYSTRING_GUI_H

#include "textbuf_gui.h"
#include "window_gui.h"

/**
 * Return values for HandleEditBoxKey
 */
enum HandleEditBoxResult
{
	HEBR_EDITING = 0, // Other key pressed.
	HEBR_CONFIRM,     // Return or enter key pressed.
	HEBR_CANCEL,      // Escape key pressed.
	HEBR_NOT_FOCUSED, // Edit box widget not focused.
};

/**
 * Data stored about a string that can be modified in the GUI
 */
struct QueryString {
	StringID caption;
	Textbuf text;
	const char *orig;
	CharSetFilter afilter;
	bool handled;

	/**
	 * Make sure everything gets initialized properly.
	 */
	QueryString() : orig(NULL)
	{
	}

	/**
	 * Make sure everything gets freed.
	 */
	~QueryString()
	{
		free((void*)this->orig);
	}

private:
	bool HasEditBoxFocus(const Window *w, int wid) const;
public:
	void DrawEditBox(Window *w, int wid);
	void HandleEditBox(Window *w, int wid);
	HandleEditBoxResult HandleEditBoxKey(Window *w, int wid, uint16 key, uint16 keycode, EventState &state);
};

struct QueryStringBaseWindow : public Window, public QueryString {
	char *edit_str_buf;         ///< Buffer for string.
	const uint16 edit_str_size; ///< Maximum length of string (in bytes), including terminating '\0'.
	const uint16 max_chars;     ///< Maximum length of string (in characters), including terminating '\0'.

	QueryStringBaseWindow(uint16 size, uint16 chars = UINT16_MAX) : Window(), edit_str_size(size), max_chars(chars == UINT16_MAX ? size : chars)
	{
		assert(size != 0);
		this->edit_str_buf = CallocT<char>(size);
	}

	~QueryStringBaseWindow()
	{
		free(this->edit_str_buf);
	}

	void DrawEditBox(int wid);
	void HandleEditBox(int wid);
	HandleEditBoxResult HandleEditBoxKey(int wid, uint16 key, uint16 keycode, EventState &state);
	virtual void OnOpenOSKWindow(int wid);
	virtual void OnOSKInput(int wid) {}
};

void ShowOnScreenKeyboard(QueryStringBaseWindow *parent, int button, int cancel, int ok);
void UpdateOSKOriginalText(const QueryStringBaseWindow *parent, int button);

#endif /* QUERYSTRING_GUI_H */
