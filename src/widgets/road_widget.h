/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file road_widget.h Types related to the road widgets. */

#ifndef WIDGETS_ROAD_WIDGET_H
#define WIDGETS_ROAD_WIDGET_H

/** Widgets of the WC_SCEN_BUILD_TOOLBAR / WC_BUILD_TOOLBAR (WC_SCEN_BUILD_TOOLBAR / WC_BUILD_TOOLBAR is also used in others). */
enum RoadToolbarWidgets {
	RTW_ROAD_X,
	RTW_ROAD_Y,
	RTW_AUTOROAD,
	RTW_DEMOLISH,
	RTW_DEPOT,
	RTW_BUS_STATION,
	RTW_TRUCK_STATION,
	RTW_ONE_WAY,
	RTW_BUILD_BRIDGE,
	RTW_BUILD_TUNNEL,
	RTW_REMOVE,
};

/** Widgets of the WC_BUILD_DEPOT (WC_BUILD_DEPOT is also used in others). */
enum BuildRoadDepotWidgets {
	BRDW_CAPTION,
	BRDW_DEPOT_NE,
	BRDW_DEPOT_SE,
	BRDW_DEPOT_SW,
	BRDW_DEPOT_NW,
};

/** Widgets of the WC_BUS_STATION / WC_TRUCK_STATION. */
enum BuildRoadStationWidgets {
	BRSW_CAPTION,
	BRSW_BACKGROUND,
	BRSW_STATION_NE,
	BRSW_STATION_SE,
	BRSW_STATION_SW,
	BRSW_STATION_NW,
	BRSW_STATION_X,
	BRSW_STATION_Y,
	BRSW_LT_OFF,
	BRSW_LT_ON,
	BRSW_INFO,
};

#endif /* WIDGETS_ROAD_WIDGET_H */
