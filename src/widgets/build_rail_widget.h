/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file build_rail_widget.h Types related to the build_rail widgets. */

#ifndef WIDGETS_BUILD_RAIL_WIDGET_H
#define WIDGETS_BUILD_RAIL_WIDGET_H

/** Widgets of the #BuildVehicleWindow class. */
enum BuildRailWidgets : WidgetID {
	WID_BR_CAPTION, ///< Caption of window.
	WID_BR_SORT_ASCENDING_DESCENDING, ///< Sort direction.
	WID_BR_SORT_DROPDOWN, ///< Criteria of sorting dropdown.
	WID_BR_FILTER, ///< Filter by name.
	WID_BR_SHOW_HIDDEN_TRACKS, ///< Toggle whether to display the hidden vehicles.
	WID_BR_LIST, ///< List of vehicles.
	WID_BR_SCROLLBAR, ///< Scrollbar of list.
	WID_BR_PANEL, ///< Button panel.
	WID_BR_SHOW_HIDE, ///< Button to hide or show the selected engine.
	WID_BR_RENAME, ///< Rename button.
	WID_BR_CONFIGURE_BADGES, ///< Button to configure badges.
	WID_BR_BADGE_FILTER, ///< Container for dropdown badge filters.
};

#endif /* WIDGETS_BUILD_RAIL_WIDGET_H */
