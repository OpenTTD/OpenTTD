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

#include "textbuf_type.h"
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
	int ok_button;      ///< Widget button of parent window to simulate when pressing OK in OSK.
	int cancel_button;  ///< Widget button of parent window to simulate when pressing CANCEL in OSK.
	Textbuf text;
	const char *orig;
	CharSetFilter afilter;
	bool handled;

	/**
	 * Initialize string.
	 * @param size Maximum size in bytes.
	 * @param chars Maximum size in chars.
	 */
	QueryString(uint16 size, uint16 chars = UINT16_MAX) : ok_button(-1), cancel_button(-1), text(size, chars), orig(NULL)
	{
	}

	/**
	 * Make sure everything gets freed.
	 */
	~QueryString()
	{
		free(this->orig);
	}

private:
	bool HasEditBoxFocus(const Window *w, int wid) const;
public:
	void DrawEditBox(const Window *w, int wid) const;
	void HandleEditBox(Window *w, int wid);
	HandleEditBoxResult HandleEditBoxKey(Window *w, int wid, uint16 key, uint16 keycode, EventState &state);
};

struct QueryStringBaseWindow : public Window, public QueryString {
	QueryStringBaseWindow(uint16 size, uint16 chars = UINT16_MAX) : Window(), QueryString(size, chars)
	{
	}

};

void ShowOnScreenKeyboard(QueryStringBaseWindow *parent, int button);
void UpdateOSKOriginalText(const QueryStringBaseWindow *parent, int button);

#endif /* QUERYSTRING_GUI_H */
