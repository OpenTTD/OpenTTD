/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file road_widget.h Types related to the road widgets. */

#ifndef WIDGETS_ROAD_WIDGET_H
#define WIDGETS_ROAD_WIDGET_H

/** Widgets of the #BuildRoadToolbarWindow class. */
enum RoadToolbarWidgets : WidgetID {
	/* Name starts with RO instead of R, because of collision with RailToolbarWidgets */
	WID_ROT_CAPTION,        ///< Caption of the window
	WID_ROT_ROAD_X,         ///< Build road in x-direction.
	WID_ROT_ROAD_Y,         ///< Build road in y-direction.
	WID_ROT_AUTOROAD,       ///< Autorail.
	WID_ROT_DEMOLISH,       ///< Demolish.
	WID_ROT_DEPOT,          ///< Build depot.
	WID_ROT_EXTENDED_DEPOT, ///< Build extended depot.
	WID_ROT_BUILD_WAYPOINT, ///< Build waypoint.
	WID_ROT_BUS_STATION,    ///< Build bus station.
	WID_ROT_TRUCK_STATION,  ///< Build truck station.
	WID_ROT_ONE_WAY,        ///< Build one-way road.
	WID_ROT_BUILD_BRIDGE,   ///< Build bridge.
	WID_ROT_BUILD_TUNNEL,   ///< Build tunnel.
	WID_ROT_REMOVE,         ///< Remove road.
	WID_ROT_CONVERT_ROAD,   ///< Convert road.

	INVALID_WID_ROT = -1,
};

/** Widgets of the #BuildRoadDepotWindow class. */
enum BuildRoadDepotWidgets : WidgetID {
	/* Name starts with BRO instead of BR, because of collision with BuildRailDepotWidgets */
	WID_BROD_CAPTION,   ///< Caption of the window.
	WID_BROD_DEPOT_NE,  ///< Depot with NE entry.
	WID_BROD_DEPOT_SE,  ///< Depot with SE entry.
	WID_BROD_DEPOT_SW,  ///< Depot with SW entry.
	WID_BROD_DEPOT_NW,  ///< Depot with NW entry.
};

/** Widgets of the #BuildRoadStationWindow class. */
enum BuildRoadStationWidgets : WidgetID {
	/* Name starts with BRO instead of BR, because of collision with BuildRailStationWidgets */
	WID_BROS_CAPTION,                ///< Caption of the window.
	WID_BROS_STATION_NE,             ///< Terminal station with NE entry.
	WID_BROS_STATION_SE,             ///< Terminal station with SE entry.
	WID_BROS_STATION_SW,             ///< Terminal station with SW entry.
	WID_BROS_STATION_NW,             ///< Terminal station with NW entry.
	WID_BROS_STATION_X,              ///< Drive-through station in x-direction.
	WID_BROS_STATION_Y,              ///< Drive-through station in y-direction.
	WID_BROS_LT_OFF,                 ///< Turn off area highlight.
	WID_BROS_LT_ON,                  ///< Turn on area highlight.
	WID_BROS_ACCEPTANCE,             ///< Station acceptance info.
	WID_BROS_AVAILABLE_ORIENTATIONS, ///< Selection for selecting 6 or 2 orientations.
};

#endif /* WIDGETS_ROAD_WIDGET_H */
