/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file airport_widget.h Types related to the airport widgets. */

#ifndef WIDGETS_AIRPORT_WIDGET_H
#define WIDGETS_AIRPORT_WIDGET_H

/** Widgets of the WC_BUILD_TOOLBAR (WC_BUILD_TOOLBAR is also used in others). */
enum AirportToolbarWidgets {
	ATW_AIRPORT,
	ATW_DEMOLISH,
};

/** Widgets of the WC_BUILD_STATION (WC_BUILD_STATION is also used in others). */
enum AirportPickerWidgets {
	BAIRW_CLASS_DROPDOWN,
	BAIRW_AIRPORT_LIST,
	BAIRW_SCROLLBAR,
	BAIRW_LAYOUT_NUM,
	BAIRW_LAYOUT_DECREASE,
	BAIRW_LAYOUT_INCREASE,
	BAIRW_AIRPORT_SPRITE,
	BAIRW_EXTRA_TEXT,
	BAIRW_BOTTOMPANEL,
	BAIRW_COVERAGE_LABEL,
	BAIRW_BTN_DONTHILIGHT,
	BAIRW_BTN_DOHILIGHT,
};

#endif /* WIDGETS_AIRPORT_WIDGET_H */
