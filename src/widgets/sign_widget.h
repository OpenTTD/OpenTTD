/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sign_widget.h Types related to the sign widgets. */

#ifndef WIDGETS_SIGN_WIDGET_H
#define WIDGETS_SIGN_WIDGET_H

/** Widgets of the #SignListWindow class. */
enum SignListWidgets : WidgetID {
	/* Name starts with SI instead of S, because of collision with SaveLoadWidgets */
	WID_SIL_CAPTION,               ///< Caption of the window.
	WID_SIL_LIST,                  ///< List of signs.
	WID_SIL_SCROLLBAR,             ///< Scrollbar of list.
	WID_SIL_FILTER_TEXT,           ///< Text box for typing a filter string.
	WID_SIL_FILTER_MATCH_CASE_BTN, ///< Button to toggle if case sensitive filtering should be used.
	WID_SIL_FILTER_ENTER_BTN,      ///< Scroll to first sign.
};

/** Widgets of the #SignWindow class. */
enum QueryEditSignWidgets : WidgetID {
	WID_QES_CAPTION,  ///< Caption of the window.
	WID_QES_LOCATION, ///< Scroll to sign location.
	WID_QES_TEXT,     ///< Text of the query.
	WID_QES_OK,       ///< OK button.
	WID_QES_CANCEL,   ///< Cancel button.
	WID_QES_DELETE,   ///< Delete button.
	WID_QES_PREVIOUS, ///< Previous button.
	WID_QES_NEXT,     ///< Next button.
};

#endif /* SIGN_WIDGET_H */
