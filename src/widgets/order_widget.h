/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file order_widget.h Types related to the order widgets. */

#ifndef WIDGETS_ORDER_WIDGET_H
#define WIDGETS_ORDER_WIDGET_H

/** Widgets of the WC_VEHICLE_ORDERS. */
enum OrderWindowWidgets {
	ORDER_WIDGET_CAPTION,
	ORDER_WIDGET_TIMETABLE_VIEW,
	ORDER_WIDGET_ORDER_LIST,
	ORDER_WIDGET_SCROLLBAR,
	ORDER_WIDGET_SKIP,
	ORDER_WIDGET_DELETE,
	ORDER_WIDGET_STOP_SHARING,
	ORDER_WIDGET_NON_STOP,
	ORDER_WIDGET_GOTO,
	ORDER_WIDGET_FULL_LOAD,
	ORDER_WIDGET_UNLOAD,
	ORDER_WIDGET_REFIT,
	ORDER_WIDGET_SERVICE,
	ORDER_WIDGET_EMPTY,
	ORDER_WIDGET_REFIT_DROPDOWN,
	ORDER_WIDGET_COND_VARIABLE,
	ORDER_WIDGET_COND_COMPARATOR,
	ORDER_WIDGET_COND_VALUE,
	ORDER_WIDGET_SEL_TOP_LEFT,              ///< #NWID_SELECTION widget for left part of the top row of the 'your train' order window.
	ORDER_WIDGET_SEL_TOP_MIDDLE,            ///< #NWID_SELECTION widget for middle part of the top row of the 'your train' order window.
	ORDER_WIDGET_SEL_TOP_RIGHT,             ///< #NWID_SELECTION widget for right part of the top row of the 'your train' order window.
	ORDER_WIDGET_SEL_TOP_ROW_GROUNDVEHICLE, ///< #NWID_SELECTION widget for the top row of the 'your train' order window.
	ORDER_WIDGET_SEL_TOP_ROW,               ///< #NWID_SELECTION widget for the top row of the 'your non-trains' order window.
	ORDER_WIDGET_SEL_BOTTOM_MIDDLE,         ///< #NWID_SELECTION widget for the middle part of the bottom row of the 'your train' order window.
	ORDER_WIDGET_SHARED_ORDER_LIST,
};

#endif /* WIDGETS_ORDER_WIDGET_H */
