/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fios_widget.h Types related to the fios widgets. */

#ifndef WIDGETS_FIOS_WIDGET_H
#define WIDGETS_FIOS_WIDGET_H

/** Widgets of the WC_SAVELOAD. */
enum SaveLoadWindowWidgets {
	SLWW_WINDOWTITLE,
	SLWW_SORT_BYNAME,
	SLWW_SORT_BYDATE,
	SLWW_BACKGROUND,
	SLWW_FILE_BACKGROUND,
	SLWW_HOME_BUTTON,
	SLWW_DRIVES_DIRECTORIES_LIST,
	SLWW_SCROLLBAR,
	SLWW_CONTENT_DOWNLOAD,     ///< only available for play scenario/heightmap (content download)
	SLWW_SAVE_OSK_TITLE,       ///< only available for save operations
	SLWW_DELETE_SELECTION,     ///< same in here
	SLWW_SAVE_GAME,            ///< not to mention in here too
	SLWW_CONTENT_DOWNLOAD_SEL, ///< Selection 'stack' to 'hide' the content download
	SLWW_DETAILS,              ///< Panel with game details
	SLWW_NEWGRF_INFO,          ///< Button to open NewGgrf configuration
	SLWW_LOAD_BUTTON,          ///< Button to load game/scenario
	SLWW_MISSING_NEWGRFS,      ///< Button to find missing NewGRFs online
};

#endif /* WIDGETS_FIOS_WIDGET_H */
