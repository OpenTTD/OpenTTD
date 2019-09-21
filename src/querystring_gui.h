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
 * Data stored about a string that can be modified in the GUI
 */
struct QueryString {
	/* Special actions when hitting ENTER or ESC. (only keyboard, not OSK) */
	static const int ACTION_NOTHING  = -1; ///< Nothing.
	static const int ACTION_DESELECT = -2; ///< Deselect editbox.
	static const int ACTION_CLEAR    = -3; ///< Clear editbox.

	StringID caption;
	int ok_button;      ///< Widget button of parent window to simulate when pressing OK in OSK.
	int cancel_button;  ///< Widget button of parent window to simulate when pressing CANCEL in OSK.
	Textbuf text;
	const char *orig;
	bool handled;

	/**
	 * Initialize string.
	 * @param size Maximum size in bytes.
	 * @param chars Maximum size in chars.
	 */
	QueryString(uint16 size, uint16 chars = UINT16_MAX) : ok_button(ACTION_NOTHING), cancel_button(ACTION_DESELECT), text(size, chars), orig(nullptr)
	{
	}

	/**
	 * Make sure everything gets freed.
	 */
	~QueryString()
	{
		free(this->orig);
	}

public:
	void DrawEditBox(const Window *w, int wid) const;
	void ClickEditBox(Window *w, Point pt, int wid, int click_count, bool focus_changed);
	void HandleEditBox(Window *w, int wid);

	Point GetCaretPosition(const Window *w, int wid) const;
	Rect GetBoundingRect(const Window *w, int wid, const char *from, const char *to) const;
	const char *GetCharAtPosition(const Window *w, int wid, const Point &pt) const;

	/**
	 * Get the current text.
	 * @return Current text.
	 */
	const char *GetText() const
	{
		return this->text.buf;
	}

	/**
	 * Get the position of the caret in the text buffer.
	 * @return Pointer to the caret in the text buffer.
	 */
	const char *GetCaret() const
	{
		return this->text.buf + this->text.caretpos;
	}

	/**
	 * Get the currently marked text.
	 * @param[out] length Length of the marked text.
	 * @return Beginning of the marked area or nullptr if no text is marked.
	 */
	const char *GetMarkedText(size_t *length) const
	{
		if (this->text.markend == 0) return nullptr;

		*length = this->text.markend - this->text.markpos;
		return this->text.buf + this->text.markpos;
	}
};

void ShowOnScreenKeyboard(Window *parent, int button);
void UpdateOSKOriginalText(const Window *parent, int button);
bool IsOSKOpenedFor(const Window *w, int button);

#endif /* QUERYSTRING_GUI_H */
