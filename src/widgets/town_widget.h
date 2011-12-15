/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file town_widget.h Types related to the town widgets. */

#ifndef WIDGETS_TOWN_WIDGET_H
#define WIDGETS_TOWN_WIDGET_H

/** Widgets of the WC_TOWN_DIRECTORY. */
enum TownDirectoryWidgets {
	TDW_SORTNAME,
	TDW_SORTPOPULATION,
	TDW_CENTERTOWN,
	TDW_SCROLLBAR,
	TDW_BOTTOM_PANEL,
	TDW_BOTTOM_TEXT,
};

/** Widgets of the WC_TOWN_AUTHORITY. */
enum TownAuthorityWidgets {
	TWA_CAPTION,
	TWA_RATING_INFO,  ///< Overview with ratings for each company.
	TWA_COMMAND_LIST, ///< List of commands for the player.
	TWA_SCROLLBAR,
	TWA_ACTION_INFO,  ///< Additional information about the action.
	TWA_EXECUTE,      ///< Do-it button.
};

/** Widgets of the WC_TOWN_VIEW. */
enum TownViewWidgets {
	TVW_CAPTION,
	TVW_VIEWPORT,
	TVW_INFOPANEL,
	TVW_CENTERVIEW,
	TVW_SHOWAUTHORITY,
	TVW_CHANGENAME,
	TVW_EXPAND,
	TVW_DELETE,
};

/** Widgets of the WC_FOUND_TOWN. */
enum TownScenarioEditorWidgets {
	TSEW_BACKGROUND,
	TSEW_NEWTOWN,
	TSEW_RANDOMTOWN,
	TSEW_MANYRANDOMTOWNS,
	TSEW_TOWNNAME_TEXT,
	TSEW_TOWNNAME_EDITBOX,
	TSEW_TOWNNAME_RANDOM,
	TSEW_TOWNSIZE,
	TSEW_SIZE_SMALL,
	TSEW_SIZE_MEDIUM,
	TSEW_SIZE_LARGE,
	TSEW_SIZE_RANDOM,
	TSEW_CITY,
	TSEW_TOWNLAYOUT,
	TSEW_LAYOUT_ORIGINAL,
	TSEW_LAYOUT_BETTER,
	TSEW_LAYOUT_GRID2,
	TSEW_LAYOUT_GRID3,
	TSEW_LAYOUT_RANDOM,
};

#endif /* WIDGETS_TOWN_WIDGET_H */
