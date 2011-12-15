/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dock_widget.h Types related to the dock widgets. */

#ifndef WIDGETS_DOCK_WIDGET_H
#define WIDGETS_DOCK_WIDGET_H

/** Widgets of the WC_SCEN_BUILD_TOOLBAR / WC_BUILD_TOOLBAR (WC_SCEN_BUILD_TOOLBAR / WC_BUILD_TOOLBAR is also used in others). */
enum DockToolbarWidgets {
	DTW_BUTTONS_BEGIN,             ///< Begin of clickable buttons (except seperating panel)
	DTW_CANAL = DTW_BUTTONS_BEGIN, ///< Build canal button
	DTW_LOCK,                      ///< Build lock button
	DTW_DEMOLISH,                  ///< Demolish aka dynamite button
	DTW_DEPOT,                     ///< Build depot button
	DTW_STATION,                   ///< Build station button
	DTW_BUOY,                      ///< Build buoy button
	DTW_RIVER,                     ///< Build river button (in scenario editor)
	DTW_BUILD_AQUEDUCT,            ///< Build aqueduct button
	DTW_END,                       ///< End of toolbar widgets
};

#endif /* WIDGETS_DOCK_WIDGET_H */
