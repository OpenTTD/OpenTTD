/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file toolbar_gui.h Stuff related to the (main) toolbar. */

#ifndef TOOLBAR_GUI_H
#define TOOLBAR_GUI_H

#include "widgets/toolbar_widget.h"

// TODO: Replace all instances of "MTHK_blah" with "WID_blah" where we can,
// then redefine this as AdditionalMainToolbarHotkeys, or something like that.
enum MainToolbarHotkeys : int32_t {
	MTHK_PAUSE = WID_TN_PAUSE,
	MTHK_FASTFORWARD = WID_TN_FAST_FORWARD,
	MTHK_SETTINGS = WID_TN_SETTINGS,
	MTHK_SAVEGAME = WID_TN_SAVE,
	MTHK_SMALLMAP = WID_TN_SMALL_MAP,
	MTHK_TOWNDIRECTORY = WID_TN_TOWNS,
	MTHK_SUBSIDIES = WID_TN_SUBSIDIES,
	MTHK_STATIONS = WID_TN_STATIONS,
	MTHK_FINANCES = WID_TN_FINANCES,
	MTHK_COMPANIES = WID_TN_COMPANIES,
	MTHK_STORY = WID_TN_STORY,
	MTHK_GOAL = WID_TN_GOAL,
	MTHK_GRAPHS = WID_TN_GRAPHS,
	MTHK_LEAGUE = WID_TN_LEAGUE,
	MTHK_INDUSTRIES = WID_TN_INDUSTRIES,
	MTHK_TRAIN_LIST = WID_TN_TRAINS,
	MTHK_ROADVEH_LIST = WID_TN_ROADVEHS,
	MTHK_SHIP_LIST = WID_TN_SHIPS,
	MTHK_AIRCRAFT_LIST = WID_TN_AIRCRAFT,
	MTHK_ZOOM_IN = WID_TN_ZOOM_IN,
	MTHK_ZOOM_OUT = WID_TN_ZOOM_OUT,
	MTHK_BUILD_RAIL = WID_TN_RAILS,
	MTHK_BUILD_ROAD = WID_TN_ROADS,
	MTHK_BUILD_TRAM = WID_TN_TRAMS,
	MTHK_BUILD_DOCKS = WID_TN_WATER,
	MTHK_BUILD_AIRPORT = WID_TN_AIR,
	MTHK_TERRAFORM = WID_TN_LANDSCAPE,
	MTHK_MUSIC = WID_TN_MUSIC_SOUND,
	MTHK_LANDINFO = WID_TN_HELP,
	// Hotkeys without associated widgets.
	MTHK_LOADGAME = WID_TN_END,
	MTHK_BUILD_TREES,
	MTHK_SCRIPT_DEBUG,
	MTHK_SMALL_SCREENSHOT,
	MTHK_ZOOMEDIN_SCREENSHOT,
	MTHK_DEFAULTZOOM_SCREENSHOT,
	MTHK_GIANT_SCREENSHOT,
	MTHK_CHEATS,
	MTHK_EXTRA_VIEWPORT,
	MTHK_CLIENT_LIST,
	MTHK_SIGN_LIST,
};

void AllocateToolbar();
void ToggleBoundingBoxes();
void ToggleDirtyBlocks();
void ToggleWidgetOutlines();

extern uint _toolbar_width;

#endif /* TOOLBAR_GUI_H */
