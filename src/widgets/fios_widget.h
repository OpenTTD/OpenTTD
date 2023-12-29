/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fios_widget.h Types related to the fios widgets. */

#ifndef WIDGETS_FIOS_WIDGET_H
#define WIDGETS_FIOS_WIDGET_H

/** Widgets of the #SaveLoadWindow class. */
enum SaveLoadWidgets : WidgetID {
	WID_SL_CAPTION,                 ///< Caption of the window.
	WID_SL_SORT_BYNAME,             ///< Sort by name button.
	WID_SL_SORT_BYDATE,             ///< Sort by date button.
	WID_SL_FILTER,                  ///< Filter list of files
	WID_SL_BACKGROUND,              ///< Background of window.
	WID_SL_FILE_BACKGROUND,         ///< Background of file selection.
	WID_SL_HOME_BUTTON,             ///< Home button.
	WID_SL_DRIVES_DIRECTORIES_LIST, ///< Drives list.
	WID_SL_SCROLLBAR,               ///< Scrollbar of the file list.
	WID_SL_CONTENT_DOWNLOAD,        ///< Content download button, only available for play scenario/heightmap.
	WID_SL_SAVE_OSK_TITLE,          ///< Title textbox, only available for save operations.
	WID_SL_DELETE_SELECTION,        ///< Delete button, only available for save operations.
	WID_SL_SAVE_GAME,               ///< Save button, only available for save operations.
	WID_SL_CONTENT_DOWNLOAD_SEL,    ///< Selection 'stack' to 'hide' the content download.
	WID_SL_DETAILS,                 ///< Panel with game details.
	WID_SL_NEWGRF_INFO,             ///< Button to open NewGgrf configuration.
	WID_SL_LOAD_BUTTON,             ///< Button to load game/scenario.
	WID_SL_MISSING_NEWGRFS,         ///< Button to find missing NewGRFs online.
};

#endif /* WIDGETS_FIOS_WIDGET_H */
