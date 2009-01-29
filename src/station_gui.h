/* $Id$ */

/** @file station_gui.h Contains enums and function declarations connected with stations GUI */

#ifndef STATION_GUI_H
#define STATION_GUI_H

#include "command_type.h"

/** Enum for StationView, referring to _station_view_widgets and _station_view_expanded_widgets */
enum StationViewWidgets {
	SVW_CLOSEBOX   =  0, ///< Close 'X' button
	SVW_CAPTION    =  1, ///< Caption of the window
	SVW_WAITING    =  3, ///< List of waiting cargo
	SVW_ACCEPTLIST =  5, ///< List of accepted cargos
	SVW_RATINGLIST =  5, ///< Ratings of cargos
	SVW_LOCATION   =  6, ///< 'Location' button
	SVW_RATINGS    =  7, ///< 'Ratings' button
	SVW_ACCEPTS    =  7, ///< 'Accepts' button
	SVW_RENAME     =  8, ///< 'Rename' button
	SVW_TRAINS     =  9, ///< List of scheduled trains button
	SVW_ROADVEHS,        ///< List of scheduled road vehs button
	SVW_PLANES,          ///< List of scheduled planes button
	SVW_SHIPS,           ///< List of scheduled ships button
	SVW_RESIZE,          ///< Resize button
};

enum StationCoverageType {
	SCT_PASSENGERS_ONLY,
	SCT_NON_PASSENGERS_ONLY,
	SCT_ALL
};

int DrawStationCoverageAreaText(int sx, int sy, StationCoverageType sct, int rad, bool supplies);
void CheckRedrawStationCoverage(const Window *w);

void ShowSelectStationIfNeeded(CommandContainer cmd, int w, int h);

#endif /* STATION_GUI_H */
