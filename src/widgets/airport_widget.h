/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file airport_widget.h Types related to the airport widgets. */

#ifndef WIDGETS_AIRPORT_WIDGET_H
#define WIDGETS_AIRPORT_WIDGET_H

/** Widgets of the #BuildAirToolbarWindow class. */
enum AirportToolbarWidgets : WidgetID {
	WID_AT_AIRPORT,  ///< Build airport button.
	WID_AT_DEMOLISH, ///< Demolish button.

	INVALID_WID_AT = -1,
};

/** Widgets of the #BuildAirportWindow class. */
enum AirportPickerWidgets : WidgetID {
	WID_AP_CLASS_DROPDOWN,  ///< Dropdown of airport classes.
	WID_AP_AIRPORT_LIST,    ///< List of airports.
	WID_AP_SCROLLBAR,       ///< Scrollbar of the list.
	WID_AP_LAYOUT_NUM,      ///< Current number of the layout.
	WID_AP_LAYOUT_DECREASE, ///< Decrease the layout number.
	WID_AP_LAYOUT_INCREASE, ///< Increase the layout number.
	WID_AP_AIRPORT_SPRITE,  ///< A visual display of the airport currently selected.
	WID_AP_EXTRA_TEXT,      ///< Additional text about the airport.
	WID_AP_COVERAGE_LABEL,  ///< Label if you want to see the coverage.
	WID_AP_BTN_DONTHILIGHT, ///< Don't show the coverage button.
	WID_AP_BTN_DOHILIGHT,   ///< Show the coverage button.
	WID_AP_ACCEPTANCE,      ///< Acceptance info.
};

#endif /* WIDGETS_AIRPORT_WIDGET_H */
