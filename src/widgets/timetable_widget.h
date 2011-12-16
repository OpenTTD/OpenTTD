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
enum TimetableViewWidgets {
	WID_TV_CAPTION,                     ///< Caption of the window.
	WID_TV_ORDER_VIEW,                  ///< Order view.
	WID_TV_TIMETABLE_PANEL,             ///< Timetable panel.
	WID_TV_ARRIVAL_DEPARTURE_PANEL,     ///< Panel with the expected/scheduled arrivals.
	WID_TV_SCROLLBAR,                   ///< Scrollbar for the panel.
	WID_TV_SUMMARY_PANEL,               ///< Summary panel.
	WID_TV_START_DATE,                  ///< Start date button.
	WID_TV_CHANGE_TIME,                 ///< Change time button.
	WID_TV_CLEAR_TIME,                  ///< Clear time button.
	WID_TV_RESET_LATENESS,              ///< Reset lateness button.
	WID_TV_AUTOFILL,                    ///< Autofill button.
	WID_TV_EXPECTED,                    ///< Toggle between expected and scheduled arrivals.
	WID_TV_SHARED_ORDER_LIST,           ///< Show the shared order list.
	WID_TV_ARRIVAL_DEPARTURE_SELECTION, ///< Disable/hide the arrival departure panel.
	WID_TV_EXPECTED_SELECTION,          ///< Disable/hide the expected selection button.
};

#endif /* WIDGETS_TIMETABLE_WIDGET_H */
