/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timetable_widget.h Types related to the timetable widgets. */

#ifndef WIDGETS_TIMETABLE_WIDGET_H
#define WIDGETS_TIMETABLE_WIDGET_H

/** Widgets of the WC_VEHICLE_TIMETABLE. */
enum TimetableViewWindowWidgets {
	TTV_CAPTION,
	TTV_ORDER_VIEW,
	TTV_TIMETABLE_PANEL,
	TTV_ARRIVAL_DEPARTURE_PANEL,      ///< Panel with the expected/scheduled arrivals
	TTV_SCROLLBAR,
	TTV_SUMMARY_PANEL,
	TTV_START_DATE,
	TTV_CHANGE_TIME,
	TTV_CLEAR_TIME,
	TTV_RESET_LATENESS,
	TTV_AUTOFILL,
	TTV_EXPECTED,                    ///< Toggle between expected and scheduled arrivals
	TTV_SHARED_ORDER_LIST,           ///< Show the shared order list
	TTV_ARRIVAL_DEPARTURE_SELECTION, ///< Disable/hide the arrival departure panel
	TTV_EXPECTED_SELECTION,          ///< Disable/hide the expected selection button
};

#endif /* WIDGETS_TIMETABLE_WIDGET_H */
