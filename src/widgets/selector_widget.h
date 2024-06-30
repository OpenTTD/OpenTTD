/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file selector_widget.h Types related to the selector widget widgets. */

#ifndef WIDGETS_SELECTOR_WIDGET_H
#define WIDGETS_SELECTOR_WIDGET_H

/** Widgets of the #SelectorWidget class. */
enum SelectorClassWindowWidgets : WidgetID {
	WID_SELECTOR_START = (1 << 16) + 1000, ///< Dummy to ensure widgets don't overlap.

	WID_SELECTOR_MATRIX, ///< Item list.
	WID_SELECTOR_SCROLLBAR, ///< Vertical scrollbar.
	WID_SELECTOR_EDITBOX, ///< Editbox filter.
	WID_SELECTOR_HIDEALL, ///< Hide all button.
	WID_SELECTOR_SHOWALL, ///< Show all button.
	WID_SELECTOR_RESIZE, ///< Resize handle
};

#endif /* WIDGETS_SELECTOR_WIDGET_H */
