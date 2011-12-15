/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file station_widget.h Types related to the station widgets. */

#ifndef WIDGETS_STATION_WIDGET_H
#define WIDGETS_STATION_WIDGET_H

/** Widgets of the WC_STATION_VIEW. */
enum StationViewWidgets {
	SVW_CAPTION    =  0, ///< Caption of the window
	SVW_WAITING    =  1, ///< List of waiting cargo
	SVW_SCROLLBAR  =  2, ///< Scrollbar
	SVW_ACCEPTLIST =  3, ///< List of accepted cargoes
	SVW_RATINGLIST =  3, ///< Ratings of cargoes
	SVW_LOCATION   =  4, ///< 'Location' button
	SVW_RATINGS    =  5, ///< 'Ratings' button
	SVW_ACCEPTS    =  5, ///< 'Accepts' button
	SVW_RENAME     =  6, ///< 'Rename' button
	SVW_TRAINS     =  7, ///< List of scheduled trains button
	SVW_ROADVEHS,        ///< List of scheduled road vehs button
	SVW_SHIPS,           ///< List of scheduled ships button
	SVW_PLANES,          ///< List of scheduled planes button
};

/** Widgets of the WC_STATION_LIST. */
enum StationListWidgets {
	SLW_CAPTION,        ///< Window caption
	SLW_LIST,           ///< The main panel, list of stations
	SLW_SCROLLBAR,      ///< Scrollbar next to the main panel

	/* Vehicletypes need to be in order of StationFacility due to bit magic */
	SLW_TRAIN,          ///< 'TRAIN' button - list only facilities where is a railroad station
	SLW_TRUCK,          ///< 'TRUCK' button - list only facilities where is a truck stop
	SLW_BUS,            ///< 'BUS' button - list only facilities where is a bus stop
	SLW_AIRPLANE,       ///< 'AIRPLANE' button - list only facilities where is an airport
	SLW_SHIP,           ///< 'SHIP' button - list only facilities where is a dock
	SLW_FACILALL,       ///< 'ALL' button - list all facilities

	SLW_NOCARGOWAITING, ///< 'NO' button - list stations where no cargo is waiting
	SLW_CARGOALL,       ///< 'ALL' button - list all stations

	SLW_SORTBY,         ///< 'Sort by' button - reverse sort direction
	SLW_SORTDROPBTN,    ///< Dropdown button

	SLW_CARGOSTART,     ///< Widget numbers used for list of cargo types (not present in _company_stations_widgets)
};

/** Widgets of the WC_SELECT_STATION. */
enum JoinStationWidgets {
	JSW_WIDGET_CAPTION,
	JSW_PANEL,
	JSW_SCROLLBAR,
};

#endif /* WIDGETS_STATION_WIDGET_H */
