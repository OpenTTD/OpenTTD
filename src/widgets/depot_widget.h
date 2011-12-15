/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file depot_widget.h Types related to the depot widgets. */

#ifndef WIDGETS_DEPOT_WIDGET_H
#define WIDGETS_DEPOT_WIDGET_H

/** Widgets of the WC_VEHICLE_DEPOT. */
enum DepotWindowWidgets {
	DEPOT_WIDGET_CAPTION,
	DEPOT_WIDGET_SELL,
	DEPOT_WIDGET_SHOW_SELL_CHAIN,
	DEPOT_WIDGET_SELL_CHAIN,
	DEPOT_WIDGET_SELL_ALL,
	DEPOT_WIDGET_AUTOREPLACE,
	DEPOT_WIDGET_MATRIX,
	DEPOT_WIDGET_V_SCROLL, ///< Vertical scrollbar
	DEPOT_WIDGET_SHOW_H_SCROLL,
	DEPOT_WIDGET_H_SCROLL, ///< Horizontal scrollbar
	DEPOT_WIDGET_BUILD,
	DEPOT_WIDGET_CLONE,
	DEPOT_WIDGET_LOCATION,
	DEPOT_WIDGET_SHOW_RENAME,
	DEPOT_WIDGET_RENAME,
	DEPOT_WIDGET_VEHICLE_LIST,
	DEPOT_WIDGET_STOP_ALL,
	DEPOT_WIDGET_START_ALL,
};

#endif /* WIDGETS_DEPOT_WIDGET_H */
