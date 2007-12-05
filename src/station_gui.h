/* $Id$ */

/** @file station_gui.h Contains enums and function declarations connected with stations GUI */

#ifndef STATION_GUI_H
#define STATION_GUI_H

/** Enum for PlayerStations, referring to _player_stations_widgets */
enum StationListWidgets {
	SLW_CLOSEBOX       =  0, ///< Close 'X' button

	SLW_LIST           =  3, ///< The main panel, list of stations

	SLW_TRAIN          =  6, ///< 'TRAIN' button - list only facilities where is a railroad station
	SLW_TRUCK,
	SLW_BUS,
	SLW_AIRPLANE,
	SLW_SHIP,
	SLW_FACILALL,            ///< 'ALL' button - list all facilities

	SLW_PAN_BETWEEN    = 12, ///< Small panel between list of types of ficilities and list of cargo types
	SLW_NOCARGOWAITING = 13, ///< 'NO' button - list stations where no cargo is waiting
	SLW_CARGOALL       = 14, ///< 'ALL' button - list all stations
	SLW_PAN_RIGHT      = 15, ///< Panel right of list of cargo types

	SLW_SORTBY         = 16, ///< 'Sort by' button - reverse sort direction
	SLW_SORTCRITERIA   = 17, ///< Button - list of criteria
	SLW_SORTDROPBTN    = 18, ///< Dropdown button
	SLW_PAN_SORT_RIGHT = 19, ///< Panel right of sorting options

	SLW_CARGOSTART     = 20, ///< Widget numbers used for list of cargo types (not present in _player_stations_widgets)
};

/** Enum for StationView, referring to _station_view_widgets and _station_view_expanded_widgets */
enum StationViewWidgets {
	SVW_CLOSEBOX   =  0, ///< Close 'X' button
	SVW_CAPTION    =  1, ///< Caption of the window
	SVW_WAITING    =  3, ///< List of waiting cargo
	SVW_ACCEPTLIST =  5, ///< List of accepted cargos
	SVW_RATINGLIST =  6, ///< Ratings of cargos
	SVW_LOCATION   =  7, ///< 'Location' button
	SVW_RATINGS    =  8, ///< 'Ratings' button
	SVW_ACCEPTS    =  8, ///< 'Accepts' button
	SVW_RENAME     =  9, ///< 'Rename' button
	SVW_TRAINS     = 10, ///< List of scheduled trains button
	SVW_ROADVEHS,        ///< List of scheduled road vehs button
	SVW_PLANES,          ///< List of scheduled planes button
	SVW_SHIPS,           ///< List of scheduled ships button
};

/* sorter stuff */
void RebuildStationLists();
void ResortStationLists();

#endif /* STATION_GUI_H */
