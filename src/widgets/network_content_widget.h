/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_content_widget.h Types related to the network content widgets. */

#ifndef WIDGETS_NETWORK_CONTENT_WIDGET_H
#define WIDGETS_NETWORK_CONTENT_WIDGET_H

/** Widgets of the WC_NETWORK_STATUS_WINDOW (WC_NETWORK_STATUS_WINDOW is also used in NetworkJoinStatusWidgets). */
enum NetworkContentDownloadStatusWindowWidgets {
	NCDSWW_BACKGROUND, ///< Background
	NCDSWW_CANCELOK,   ///< (Optional) Cancel/OK button
};

/** Widgets of the WC_NETWORK_WINDOW (WC_NETWORK_WINDOW is also used in NetworkGameWindowWidgets, NetworkStartServerWidgets, and NetworkLobbyWindowWidgets). */
enum NetworkContentListWindowWidgets {
	NCLWW_BACKGROUND,    ///< Resize button

	NCLWW_FILTER_CAPT,   ///< Caption for the filter editbox
	NCLWW_FILTER,        ///< Filter editbox

	NCLWW_CHECKBOX,      ///< Button above checkboxes
	NCLWW_TYPE,          ///< 'Type' button
	NCLWW_NAME,          ///< 'Name' button

	NCLWW_MATRIX,        ///< Panel with list of content
	NCLWW_SCROLLBAR,     ///< Scrollbar of matrix

	NCLWW_DETAILS,       ///< Panel with content details

	NCLWW_SELECT_ALL,    ///< 'Select all' button
	NCLWW_SELECT_UPDATE, ///< 'Select updates' button
	NCLWW_UNSELECT,      ///< 'Unselect all' button
	NCLWW_OPEN_URL,      ///< 'Open url' button
	NCLWW_CANCEL,        ///< 'Cancel' button
	NCLWW_DOWNLOAD,      ///< 'Download' button

	NCLWW_SEL_ALL_UPDATE, ///< #NWID_SELECTION widget for select all/update buttons.
};

#endif /* WIDGETS_NETWORK_CONTENT_WIDGET_H */
