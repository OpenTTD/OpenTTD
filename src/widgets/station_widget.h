/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file station_widget.h Types related to the station widgets. */

#ifndef WIDGETS_STATION_WIDGET_H
#define WIDGETS_STATION_WIDGET_H

/** Widgets of the #StationViewWindow class. */
enum StationViewWidgets : WidgetID {
	WID_SV_CAPTION,            ///< Caption of the window.
	WID_SV_GROUP,              ///< label for "group by"
	WID_SV_GROUP_BY,           ///< 'Group by' button
	WID_SV_SORT_ORDER,         ///< 'Sort order' button
	WID_SV_SORT_BY,            ///< 'Sort by' button
	WID_SV_WAITING,            ///< List of waiting cargo.
	WID_SV_SCROLLBAR,          ///< Scrollbar.
	WID_SV_ACCEPT_RATING_LIST, ///< List of accepted cargoes / rating of cargoes.
	WID_SV_LOCATION,           ///< 'Location' button.
	WID_SV_ACCEPTS_RATINGS,    ///< 'Accepts' / 'Ratings' button.
	WID_SV_RENAME,             ///< 'Rename' button.
	WID_SV_CLOSE_AIRPORT,      ///< 'Close airport' button.
	WID_SV_TRAINS,             ///< List of scheduled trains button.
	WID_SV_ROADVEHS,           ///< List of scheduled road vehs button.
	WID_SV_SHIPS,              ///< List of scheduled ships button.
	WID_SV_PLANES,             ///< List of scheduled planes button.
	WID_SV_CATCHMENT,          ///< Toggle catchment area highlight.
};

/** Widgets of the #CompanyStationsWindow class. */
enum StationListWidgets : WidgetID {
	/* Name starts with ST instead of S, because of collision with SaveLoadWidgets */
	WID_STL_CAPTION,        ///< Caption of the window.
	WID_STL_LIST,           ///< The main panel, list of stations.
	WID_STL_SCROLLBAR,      ///< Scrollbar next to the main panel.

	/* Vehicletypes need to be in order of StationFacility due to bit magic */
	WID_STL_TRAIN,          ///< 'TRAIN' button - list only facilities where is a railroad station.
	WID_STL_TRUCK,          ///< 'TRUCK' button - list only facilities where is a truck stop.
	WID_STL_BUS,            ///< 'BUS' button - list only facilities where is a bus stop.
	WID_STL_AIRPLANE,       ///< 'AIRPLANE' button - list only facilities where is an airport.
	WID_STL_SHIP,           ///< 'SHIP' button - list only facilities where is a dock.
	WID_STL_FACILALL,       ///< 'ALL' button - list all facilities.

	WID_STL_NOCARGOWAITING, ///< 'NO' button - list stations where no cargo is waiting.
	WID_STL_CARGOALL,       ///< 'ALL' button - list all stations.

	WID_STL_SORTBY,         ///< 'Sort by' button - reverse sort direction.
	WID_STL_SORTDROPBTN,    ///< Dropdown button.

	WID_STL_CARGOSTART,     ///< Widget numbers used for list of cargo types (not present in _company_stations_widgets).
};

/** Widgets of the #SelectStationWindow class. */
enum JoinStationWidgets : WidgetID {
	WID_JS_CAPTION,   // Caption of the window.
	WID_JS_PANEL,     // Main panel.
	WID_JS_SCROLLBAR, // Scrollbar of the panel.
};

#endif /* WIDGETS_STATION_WIDGET_H */
