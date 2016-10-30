/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file toolbar_widget.h Types related to the toolbar widgets. */

#ifndef WIDGETS_TOOLBAR_WIDGET_H
#define WIDGETS_TOOLBAR_WIDGET_H

/** Widgets of the #MainToolbarWindow class. */
enum ToolbarNormalWidgets {
	WID_TN_PAUSE,         ///< Pause the game.
	WID_TN_FAST_FORWARD,  ///< Fast forward the game.
	WID_TN_SETTINGS,      ///< Settings menu.
	WID_TN_SAVE,          ///< Save menu.
	WID_TN_SMALL_MAP,     ///< Small map menu.
	WID_TN_TOWNS,         ///< Town menu.
	WID_TN_SUBSIDIES,     ///< Subsidy menu.
	WID_TN_STATIONS,      ///< Station menu.
	WID_TN_FINANCES,      ///< Finance menu.
	WID_TN_COMPANIES,     ///< Company menu.
	WID_TN_STORY,         ///< Story menu.
	WID_TN_GOAL,          ///< Goal menu.
	WID_TN_GRAPHS,        ///< Graph menu.
	WID_TN_LEAGUE,        ///< Company league menu.
	WID_TN_INDUSTRIES,    ///< Industry menu.
	WID_TN_VEHICLE_START, ///< Helper for the offset of the vehicle menus.
	WID_TN_TRAINS        = WID_TN_VEHICLE_START, ///< Train menu.
	WID_TN_ROADVEHS,      ///< Road vehicle menu.
	WID_TN_SHIPS,         ///< Ship menu.
	WID_TN_AIRCRAFTS,     ///< Aircraft menu.
	WID_TN_ZOOM_IN,       ///< Zoom in the main viewport.
	WID_TN_ZOOM_OUT,      ///< Zoom out the main viewport.
	WID_TN_BUILDING_TOOLS_START, ///< Helper for the offset of the building tools
	WID_TN_RAILS         = WID_TN_BUILDING_TOOLS_START, ///< Rail building menu.
	WID_TN_ROADS,         ///< Road building menu.
	WID_TN_WATER,         ///< Water building toolbar.
	WID_TN_AIR,           ///< Airport building toolbar.
	WID_TN_LANDSCAPE,     ///< Landscaping toolbar.
	WID_TN_MUSIC_SOUND,   ///< Music/sound configuration menu.
	WID_TN_MESSAGES,      ///< Messages menu.
	WID_TN_HELP,          ///< Help menu.
	WID_TN_SWITCH_BAR,    ///< Only available when toolbar has been split to switch between different subsets.
	WID_TN_END,           ///< Helper for knowing the amount of widgets.
};

/** Widgets of the #ScenarioEditorToolbarWindow class. */
enum ToolbarEditorWidgets {
	WID_TE_PAUSE,         ///< Pause the game.
	WID_TE_FAST_FORWARD,  ///< Fast forward the game.
	WID_TE_SETTINGS,      ///< Settings menu.
	WID_TE_SAVE,          ///< Save menu.
	WID_TE_SPACER,        ///< Spacer with "scenario editor" text.
	WID_TE_DATE,          ///< The date of the scenario.
	WID_TE_DATE_BACKWARD, ///< Reduce the date of the scenario.
	WID_TE_DATE_FORWARD,  ///< Increase the date of the scenario.
	WID_TE_SMALL_MAP,     ///< Small map menu.
	WID_TE_ZOOM_IN,       ///< Zoom in the main viewport.
	WID_TE_ZOOM_OUT,      ///< Zoom out the main viewport.
	WID_TE_LAND_GENERATE, ///< Land generation.
	WID_TE_TOWN_GENERATE, ///< Town building window.
	WID_TE_INDUSTRY,      ///< Industry building window.
	WID_TE_ROADS,         ///< Road building menu.
	WID_TE_WATER,         ///< Water building toolbar.
	WID_TE_TREES,         ///< Tree building toolbar.
	WID_TE_SIGNS,         ///< Sign building.
	WID_TE_DATE_PANEL,    ///< Container for the date widgets.
	/* The following three need to have the same actual widget number as the normal toolbar due to shared code. */
	WID_TE_MUSIC_SOUND = WID_TN_MUSIC_SOUND, ///< Music/sound configuration menu.
	WID_TE_HELP        = WID_TN_HELP,        ///< Help menu.
	WID_TE_SWITCH_BAR  = WID_TN_SWITCH_BAR,  ///< Only available when toolbar has been split to switch between different subsets.
};

#endif /* WIDGETS_TOOLBAR_WIDGET_H */
