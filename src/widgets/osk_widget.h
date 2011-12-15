/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file osk_widget.h Types related to the osk widgets. */

#ifndef WIDGETS_OSK_WIDGET_H
#define WIDGETS_OSK_WIDGET_H

/** Widgets of the WC_OSK. */
enum OskWidgets {
	OSK_WIDGET_CAPTION,         ///< Title bar.
	OSK_WIDGET_TEXT,            ///< Edit box.
	OSK_WIDGET_CANCEL,          ///< Cancel key.
	OSK_WIDGET_OK,              ///< Ok key.
	OSK_WIDGET_BACKSPACE,       ///< Backspace key.
	OSK_WIDGET_SPECIAL,         ///< Special key (at keyborads often used for tab key).
	OSK_WIDGET_CAPS,            ///< Capslock key.
	OSK_WIDGET_SHIFT,           ///< Shift(lock) key.
	OSK_WIDGET_SPACE,           ///< Space bar.
	OSK_WIDGET_LEFT,            ///< Cursor left key.
	OSK_WIDGET_RIGHT,           ///< Cursor right key.
	OSK_WIDGET_LETTERS,         ///< First widget of the 'normal' keys.

	OSK_WIDGET_NUMBERS_FIRST = OSK_WIDGET_LETTERS,           ///< First widget of the numbers row.
	OSK_WIDGET_NUMBERS_LAST = OSK_WIDGET_NUMBERS_FIRST + 13, ///< Last widget of the numbers row.

	OSK_WIDGET_QWERTY_FIRST,                                 ///< First widget of the qwerty row.
	OSK_WIDGET_QWERTY_LAST = OSK_WIDGET_QWERTY_FIRST + 11,   ///< Last widget of the qwerty row.

	OSK_WIDGET_ASDFG_FIRST,                                  ///< First widget of the asdfg row.
	OSK_WIDGET_ASDFG_LAST = OSK_WIDGET_ASDFG_FIRST + 11,     ///< Last widget of the asdfg row.

	OSK_WIDGET_ZXCVB_FIRST,                                  ///< First widget of the zxcvb row.
	OSK_WIDGET_ZXCVB_LAST = OSK_WIDGET_ZXCVB_FIRST + 11,     ///< Last widget of the zxcvb row.
};

#endif /* WIDGETS_OSK_WIDGET_H */
