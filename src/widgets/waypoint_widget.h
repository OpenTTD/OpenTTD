/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file waypoint_widget.h Types related to the waypoint widgets. */

#ifndef WIDGETS_WAYPOINT_WIDGET_H
#define WIDGETS_WAYPOINT_WIDGET_H

/** Widgets of the #WaypointWindow class. */
enum WaypointWidgets : WidgetID {
	WID_W_CAPTION,       ///< Caption of window.
	WID_W_VIEWPORT,      ///< The viewport on this waypoint.
	WID_W_CENTER_VIEW,   ///< Center the main view on this waypoint.
	WID_W_RENAME,        ///< Rename this waypoint.
	WID_W_SHOW_VEHICLES, ///< Show the vehicles visiting this waypoint.
	WID_W_CATCHMENT,     ///< Coverage button.
};

#endif /* WIDGETS_WAYPOINT_WIDGET_H */
