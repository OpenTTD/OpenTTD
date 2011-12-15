/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file rail_widget.h Types related to the rail widgets. */

#ifndef WIDGETS_RAIL_WIDGET_H
#define WIDGETS_RAIL_WIDGET_H

/** Widgets of the WC_BUILD_TOOLBAR (WC_BUILD_TOOLBAR is also used in others). */
enum RailToolbarWidgets {
	RTW_CAPTION,
	RTW_BUILD_NS,
	RTW_BUILD_X,
	RTW_BUILD_EW,
	RTW_BUILD_Y,
	RTW_AUTORAIL,
	RTW_DEMOLISH,
	RTW_BUILD_DEPOT,
	RTW_BUILD_WAYPOINT,
	RTW_BUILD_STATION,
	RTW_BUILD_SIGNALS,
	RTW_BUILD_BRIDGE,
	RTW_BUILD_TUNNEL,
	RTW_REMOVE,
	RTW_CONVERT_RAIL,
};

/** Widgets of the WC_BUILD_STATION (WC_BUILD_STATION is also used in others). */
enum BuildRailStationWidgets {
	BRSW_PLATFORM_DIR_X,       ///< Button to select '/' view.
	BRSW_PLATFORM_DIR_Y,       ///< Button to select '\' view.

	BRSW_PLATFORM_NUM_1,       ///< Button to select stations with a single platform.
	BRSW_PLATFORM_NUM_2,       ///< Button to select stations with 2 platforms.
	BRSW_PLATFORM_NUM_3,       ///< Button to select stations with 3 platforms.
	BRSW_PLATFORM_NUM_4,       ///< Button to select stations with 4 platforms.
	BRSW_PLATFORM_NUM_5,       ///< Button to select stations with 5 platforms.
	BRSW_PLATFORM_NUM_6,       ///< Button to select stations with 6 platforms.
	BRSW_PLATFORM_NUM_7,       ///< Button to select stations with 7 platforms.

	BRSW_PLATFORM_LEN_1,       ///< Button to select single tile length station platforms.
	BRSW_PLATFORM_LEN_2,       ///< Button to select 2 tiles length station platforms.
	BRSW_PLATFORM_LEN_3,       ///< Button to select 3 tiles length station platforms.
	BRSW_PLATFORM_LEN_4,       ///< Button to select 4 tiles length station platforms.
	BRSW_PLATFORM_LEN_5,       ///< Button to select 5 tiles length station platforms.
	BRSW_PLATFORM_LEN_6,       ///< Button to select 6 tiles length station platforms.
	BRSW_PLATFORM_LEN_7,       ///< Button to select 7 tiles length station platforms.

	BRSW_PLATFORM_DRAG_N_DROP, ///< Button to enable drag and drop type station placement.

	BRSW_HIGHLIGHT_OFF,        ///< Button for turning coverage highlighting off.
	BRSW_HIGHLIGHT_ON,         ///< Button for turning coverage highlighting on.
	BRSW_COVERAGE_TEXTS,       ///< Empty space for the coverage texts.

	BRSW_MATRIX,               ///< Matrix widget displaying the available stations.
	BRSW_IMAGE,                ///< Panel used at each cell of the matrix.
	BRSW_MATRIX_SCROLL,        ///< Scrollbar of the matrix widget.

	BRSW_SHOW_NEWST_ADDITIONS, ///< Selection for newstation class selection list.
	BRSW_SHOW_NEWST_MATRIX,    ///< Selection for newstation image matrix.
	BRSW_SHOW_NEWST_RESIZE,    ///< Selection for panel and resize at bottom right for newstation.
	BRSW_SHOW_NEWST_TYPE,      ///< Display of selected station type.
	BRSW_NEWST_LIST,           ///< List with available newstation classes.
	BRSW_NEWST_SCROLL,         ///< Scrollbar of the #BRSW_NEWST_LIST.

	BRSW_PLATFORM_NUM_BEGIN = BRSW_PLATFORM_NUM_1 - 1,
	BRSW_PLATFORM_LEN_BEGIN = BRSW_PLATFORM_LEN_1 - 1,
};

/** Widgets of the WC_BUILD_SIGNAL. */
enum BuildSignalWidgets {
	BSW_SEMAPHORE_NORM,
	BSW_SEMAPHORE_ENTRY,
	BSW_SEMAPHORE_EXIT,
	BSW_SEMAPHORE_COMBO,
	BSW_SEMAPHORE_PBS,
	BSW_SEMAPHORE_PBS_OWAY,
	BSW_ELECTRIC_NORM,
	BSW_ELECTRIC_ENTRY,
	BSW_ELECTRIC_EXIT,
	BSW_ELECTRIC_COMBO,
	BSW_ELECTRIC_PBS,
	BSW_ELECTRIC_PBS_OWAY,
	BSW_CONVERT,
	BSW_DRAG_SIGNALS_DENSITY,
	BSW_DRAG_SIGNALS_DENSITY_LABEL,
	BSW_DRAG_SIGNALS_DENSITY_DECREASE,
	BSW_DRAG_SIGNALS_DENSITY_INCREASE,
};

/** Widgets of the WC_BUILD_DEPOT (WC_BUILD_DEPOT is also used in others). */
enum BuildRailDepotWidgets {
	BRDW_DEPOT_NE,
	BRDW_DEPOT_SE,
	BRDW_DEPOT_SW,
	BRDW_DEPOT_NW,
};

/** Widgets of the WC_BUILD_DEPOT (WC_BUILD_DEPOT is also used in others). */
enum BuildRailWaypointWidgets {
	BRWW_WAYPOINT_MATRIX,
	BRWW_WAYPOINT,
	BRWW_SCROLL,
};

#endif /* WIDGETS_RAIL_WIDGET_H */
