/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file toolbar_gui.h Stuff related to the (main) toolbar. */

#ifndef TOOLBAR_GUI_H
#define TOOLBAR_GUI_H

enum MainToolbarHotkeys {
	MTHK_PAUSE,
	MTHK_FASTFORWARD,
	MTHK_SETTINGS,
	MTHK_SAVEGAME,
	MTHK_LOADGAME,
	MTHK_SMALLMAP,
	MTHK_TOWNDIRECTORY,
	MTHK_SUBSIDIES,
	MTHK_STATIONS,
	MTHK_FINANCES,
	MTHK_COMPANIES,
	MTHK_STORY,
	MTHK_GOAL,
	MTHK_GRAPHS,
	MTHK_LEAGUE,
	MTHK_INDUSTRIES,
	MTHK_TRAIN_LIST,
	MTHK_ROADVEH_LIST,
	MTHK_SHIP_LIST,
	MTHK_AIRCRAFT_LIST,
	MTHK_ZOOM_IN,
	MTHK_ZOOM_OUT,
	MTHK_BUILD_RAIL,
	MTHK_BUILD_ROAD,
	MTHK_BUILD_TRAM,
	MTHK_BUILD_DOCKS,
	MTHK_BUILD_AIRPORT,
	MTHK_BUILD_TREES,
	MTHK_MUSIC,
	MTHK_LANDINFO,
	MTHK_SCRIPT_DEBUG,
	MTHK_SMALL_SCREENSHOT,
	MTHK_ZOOMEDIN_SCREENSHOT,
	MTHK_DEFAULTZOOM_SCREENSHOT,
	MTHK_GIANT_SCREENSHOT,
	MTHK_CHEATS,
	MTHK_TERRAFORM,
	MTHK_EXTRA_VIEWPORT,
	MTHK_CLIENT_LIST,
	MTHK_SIGN_LIST
};

void AllocateToolbar();
void ToggleBoundingBoxes();
void ToggleDirtyBlocks();
void ToggleWidgetOutlines();

extern uint _toolbar_width;

#endif /* TOOLBAR_GUI_H */
