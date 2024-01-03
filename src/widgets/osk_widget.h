/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file osk_widget.h Types related to the osk widgets. */

#ifndef WIDGETS_OSK_WIDGET_H
#define WIDGETS_OSK_WIDGET_H

/** Widgets of the #OskWindow class. */
enum OnScreenKeyboardWidgets : WidgetID {
	WID_OSK_CAPTION,   ///< Caption of window.
	WID_OSK_TEXT,      ///< Edit box.
	WID_OSK_CANCEL,    ///< Cancel key.
	WID_OSK_OK,        ///< Ok key.
	WID_OSK_BACKSPACE, ///< Backspace key.
	WID_OSK_SPECIAL,   ///< Special key (at keyboards often used for tab key).
	WID_OSK_CAPS,      ///< Capslock key.
	WID_OSK_SHIFT,     ///< Shift(lock) key.
	WID_OSK_SPACE,     ///< Space bar.
	WID_OSK_LEFT,      ///< Cursor left key.
	WID_OSK_RIGHT,     ///< Cursor right key.
	WID_OSK_LETTERS,   ///< First widget of the 'normal' keys.

	WID_OSK_NUMBERS_FIRST = WID_OSK_LETTERS,            ///< First widget of the numbers row.
	WID_OSK_NUMBERS_LAST  = WID_OSK_NUMBERS_FIRST + 13, ///< Last widget of the numbers row.

	WID_OSK_QWERTY_FIRST,                               ///< First widget of the qwerty row.
	WID_OSK_QWERTY_LAST   = WID_OSK_QWERTY_FIRST  + 11, ///< Last widget of the qwerty row.

	WID_OSK_ASDFG_FIRST,                                ///< First widget of the asdfg row.
	WID_OSK_ASDFG_LAST    = WID_OSK_ASDFG_FIRST   + 11, ///< Last widget of the asdfg row.

	WID_OSK_ZXCVB_FIRST,                                ///< First widget of the zxcvb row.
	WID_OSK_ZXCVB_LAST    = WID_OSK_ZXCVB_FIRST   + 11, ///< Last widget of the zxcvb row.
};

#endif /* WIDGETS_OSK_WIDGET_H */
