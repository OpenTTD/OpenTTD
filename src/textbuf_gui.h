/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file textbuf_gui.h Stuff related to the text buffer GUI. */

#ifndef TEXTBUF_GUI_H
#define TEXTBUF_GUI_H

#include "window_type.h"
#include "string_type.h"
#include "strings_type.h"

/** Flags used in ShowQueryString() call */
enum QueryStringFlags {
	QSF_NONE             =    0,
	QSF_ACCEPT_UNCHANGED = 0x01, ///< return success even when the text didn't change
	QSF_ENABLE_DEFAULT   = 0x02, ///< enable the 'Default' button ("\0" is returned)
	QSF_LEN_IN_CHARS     = 0x04, ///< the length of the string is counted in characters
	QSF_PASSWORD         = 0x08, ///< password entry box, show warning about password security
};

DECLARE_ENUM_AS_BIT_SET(QueryStringFlags)

/** Callback procedure for the ShowQuery method. */
typedef void QueryCallbackProc(Window*, bool);

void ShowQueryString(StringID str, StringID caption, uint max_len, Window *parent, CharSetFilter afilter, QueryStringFlags flags);
void ShowQuery(StringID caption, StringID message, Window *w, QueryCallbackProc *callback, bool focus = false);

/** The number of 'characters' on the on-screen keyboard. */
static const uint OSK_KEYBOARD_ENTRIES = 50;

/**
 * The number of characters has to be OSK_KEYBOARD_ENTRIES. However, these
 * have to be UTF-8 encoded, which means up to 4 bytes per character.
 */
extern std::string _keyboard_opt[2];

#endif /* TEXTBUF_GUI_H */
