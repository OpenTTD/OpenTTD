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
enum VehicleDepotWidgets {
	WID_VD_CAPTION,         ///< Caption of window.
	WID_VD_SELL,            ///< Sell button.
	WID_VD_SHOW_SELL_CHAIN, ///< Show sell chain panel.
	WID_VD_SELL_CHAIN,      ///< Sell chain button.
	WID_VD_SELL_ALL,        ///< Sell all button.
	WID_VD_AUTOREPLACE,     ///< Autoreplace button.
	WID_VD_MATRIX,          ///< Matrix of vehicles.
	WID_VD_V_SCROLL,        ///< Vertical scrollbar.
	WID_VD_SHOW_H_SCROLL,   ///< Show horizontal scrollbar panel.
	WID_VD_H_SCROLL,        ///< Horizontal scrollbar.
	WID_VD_BUILD,           ///< Build button.
	WID_VD_CLONE,           ///< Clone button.
	WID_VD_LOCATION,        ///< Location button.
	WID_VD_SHOW_RENAME,     ///< Show rename panel.
	WID_VD_RENAME,          ///< Rename button.
	WID_VD_VEHICLE_LIST,    ///< List of vehicles.
	WID_VD_STOP_ALL,        ///< Stop all button.
	WID_VD_START_ALL,       ///< Start all button.
};

#endif /* WIDGETS_DEPOT_WIDGET_H */
