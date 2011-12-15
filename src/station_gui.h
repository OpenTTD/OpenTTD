/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file station_gui.h Contains enums and function declarations connected with stations GUI */

#ifndef STATION_GUI_H
#define STATION_GUI_H

#include "command_type.h"
#include "tilearea_type.h"
#include "window_type.h"

/** Enum for StationView, referring to _station_view_widgets and _station_view_expanded_widgets */
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

/** Types of cargo to display for station coverage. */
enum StationCoverageType {
	SCT_PASSENGERS_ONLY,     ///< Draw only passenger class cargoes.
	SCT_NON_PASSENGERS_ONLY, ///< Draw all non-passenger class cargoes.
	SCT_ALL,                 ///< Draw all cargoes.
};

int DrawStationCoverageAreaText(int left, int right, int top, StationCoverageType sct, int rad, bool supplies);
void CheckRedrawStationCoverage(const Window *w);

void ShowSelectStationIfNeeded(CommandContainer cmd, TileArea ta);
void ShowSelectWaypointIfNeeded(CommandContainer cmd, TileArea ta);

#endif /* STATION_GUI_H */
