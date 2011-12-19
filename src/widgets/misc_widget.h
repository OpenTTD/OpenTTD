/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file misc_widget.h Types related to the misc widgets. */

#ifndef WIDGETS_MISC_WIDGET_H
#define WIDGETS_MISC_WIDGET_H

/** Widgets of the WC_LAND_INFO. */
enum LandInfoWidgets {
	WID_LI_BACKGROUND, ///< Background of the window.
};

/** Widgets of the WC_TOOLTIPS. */
enum ToolTipsWidgets {
	WID_TT_BACKGROUND, ///< Background of the window.
};

/** Widgets of the WC_GAME_OPTIONS (WC_GAME_OPTIONS is also used in others). */
enum AboutWidgets {
	WID_A_SCROLLING_TEXT, ///< The actually scrolling text.
	WID_A_WEBSITE,        ///< URL of OpenTTD website.
};

/** Widgets of the WC_QUERY_STRING (WC_QUERY_STRING is also used in QueryEditSignWidgets). */
enum QueryStringWidgets {
	WID_QS_CAPTION, ///< Caption of the window.
	WID_QS_TEXT,    ///< Text of the query.
	WID_QS_DEFAULT, ///< Default button.
	WID_QS_CANCEL,  ///< Cancel button.
	WID_QS_OK,      ///< OK button.
};

/** Widgets of the WC_CONFIRM_POPUP_QUERY (WC_CONFIRM_POPUP_QUERY is also used in BootstrapAskForDownloadWidgets). */
enum QueryWidgets {
	WID_Q_CAPTION, ///< Caption of the window.
	WID_Q_TEXT,    ///< Text of the query.
	WID_Q_NO,      ///< Yes button.
	WID_Q_YES,     ///< No button.
};

#endif /* WIDGETS_MISC_WIDGET_H */
