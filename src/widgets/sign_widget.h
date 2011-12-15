/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sign_widget.h Types related to the sign widgets. */

#ifndef WIDGETS_SIGN_WIDGET_H
#define WIDGETS_SIGN_WIDGET_H

/** Widgets of the WC_SIGN_LIST. */
enum SignListWidgets {
	SLW_CAPTION,
	SLW_LIST,
	SLW_SCROLLBAR,
	SLW_FILTER_TEXT,           ///< Text box for typing a filter string
	SLW_FILTER_MATCH_CASE_BTN, ///< Button to toggle if case sensitive filtering should be used
	SLW_FILTER_CLEAR_BTN,      ///< Button to clear the filter
};

/** Widgets of the WC_QUERY_STRING (WC_QUERY_STRING is also used in QueryStringWidgets). */
enum QueryEditSignWidgets {
	QUERY_EDIT_SIGN_WIDGET_CAPTION,
	QUERY_EDIT_SIGN_WIDGET_TEXT,
	QUERY_EDIT_SIGN_WIDGET_OK,
	QUERY_EDIT_SIGN_WIDGET_CANCEL,
	QUERY_EDIT_SIGN_WIDGET_DELETE,
	QUERY_EDIT_SIGN_WIDGET_PREVIOUS,
	QUERY_EDIT_SIGN_WIDGET_NEXT,
};

#endif /* */
