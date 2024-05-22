/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file date_widget.h Types related to the date widgets. */

#ifndef WIDGETS_DATE_WIDGET_H
#define WIDGETS_DATE_WIDGET_H

/** Widgets of the #SetDateWindow class. */
enum SetDateWidgets : WidgetID {
	WID_SD_DAY,      ///< Dropdown for the day.
	WID_SD_MONTH,    ///< Dropdown for the month.
	WID_SD_YEAR,     ///< Dropdown for the year.
	WID_SD_SET_DATE, ///< Actually set the date.
};

#endif /* WIDGETS_DATE_WIDGET_H */
