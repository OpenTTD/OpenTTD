/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file toolbar_gui.h Stuff related to the (main) toolbar. */

#ifndef TOOLBAR_GUI_H
#define TOOLBAR_GUI_H

#include "newgrf.h"
#include "rail_gui.h"
#include "road_gui.h"

enum MainToolbarHotkeys : int32_t {
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

extern RailType _last_built_railtype;
extern RoadType _last_built_roadtype;
extern RoadType _last_built_tramtype;

/**
 * Get the last built type for provided feature.
 * @param feature one of GSF_RAILTYPES, GSF_ROADTYPES, GSF_TRAMTYPES.
 * @return value of _last_built_railtype, _last_built_roadtype or _last_built_tramtype
 */
inline uint8_t GetLastBuiltType(GrfSpecFeature feature)
{
	switch (feature) {
		case GSF_RAILTYPES: return _last_built_railtype;
		case GSF_ROADTYPES: return _last_built_roadtype;
		case GSF_TRAMTYPES: return _last_built_tramtype;
		default: NOT_REACHED();
	}
}

/**
 * Set the last built type for provided feature.
 * @param feature one of GSF_RAILTYPES, GSF_ROADTYPES, GSF_TRAMTYPES.
 * @param type New value for _last_built_railtype, _last_built_roadtype or _last_built_tramtype
 */
inline void GetLastBuiltType(GrfSpecFeature feature, uint8_t type)
{
	switch (feature) {
		case GSF_RAILTYPES: _last_built_railtype = (RailType)type; return;
		case GSF_ROADTYPES: _last_built_roadtype = (RoadType)type; return;
		case GSF_TRAMTYPES: _last_built_tramtype = (RoadType)type; return;
		default: NOT_REACHED();
	}
}

/**
 * Shows the build toolbar for provided feature.
 * @param feature one of GSF_RAILTYPES, GSF_ROADTYPES, GSF_TRAMTYPES.
 * @param type Depending on the feature it is a railtype, a roadtype or a tramtype.
 * @return newly opened toolbar, or nullptr if the toolbar could not be opened.
 */
inline Window *ShowBuildToolbar(GrfSpecFeature feature, uint8_t type)
{
	switch (feature) {
		case GSF_ROADTYPES:
		case GSF_TRAMTYPES:
			return ShowBuildRoadToolbar((RoadType)type);

		case GSF_RAILTYPES: return ShowBuildRailToolbar((RailType)type);
		default: NOT_REACHED();
	}
}

#endif /* TOOLBAR_GUI_H */
