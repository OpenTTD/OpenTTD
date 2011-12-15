/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehicle_widget.h Types related to the vehicle widgets. */

#ifndef WIDGETS_VEHICLE_WIDGET_H
#define WIDGETS_VEHICLE_WIDGET_H

/** Widgets of the WC_VEHICLE_VIEW. */
enum VehicleViewWindowWidgets {
	VVW_WIDGET_CAPTION,
	VVW_WIDGET_VIEWPORT,           ///< Viewport widget.
	VVW_WIDGET_START_STOP_VEH,
	VVW_WIDGET_CENTER_MAIN_VIEH,
	VVW_WIDGET_GOTO_DEPOT,
	VVW_WIDGET_REFIT_VEH,
	VVW_WIDGET_SHOW_ORDERS,
	VVW_WIDGET_SHOW_DETAILS,
	VVW_WIDGET_CLONE_VEH,
	VVW_WIDGET_SELECT_DEPOT_CLONE, ///< Selection widget between 'goto depot', and 'clone vehicle' buttons.
	VVW_WIDGET_SELECT_REFIT_TURN,  ///< Selection widget between 'refit' and 'turn around' buttons.
	VVW_WIDGET_TURN_AROUND,
	VVW_WIDGET_FORCE_PROCEED,
};

/** Widgets of the WC_VEHICLE_REFIT. */
enum VehicleRefitWidgets {
	VRW_CAPTION,
	VRW_VEHICLE_PANEL_DISPLAY,
	VRW_SHOW_HSCROLLBAR,
	VRW_HSCROLLBAR,
	VRW_SELECTHEADER,
	VRW_MATRIX,
	VRW_SCROLLBAR,
	VRW_INFOPANEL,
	VRW_REFITBUTTON,
};

/** Widgets of the WC_VEHICLE_DETAILS. */
enum VehicleDetailsWindowWidgets {
	VLD_WIDGET_CAPTION,
	VLD_WIDGET_RENAME_VEHICLE,
	VLD_WIDGET_TOP_DETAILS,
	VLD_WIDGET_INCREASE_SERVICING_INTERVAL,
	VLD_WIDGET_DECREASE_SERVICING_INTERVAL,
	VLD_WIDGET_SERVICING_INTERVAL,
	VLD_WIDGET_MIDDLE_DETAILS,
	VLD_WIDGET_MATRIX,
	VLD_WIDGET_SCROLLBAR,
	VLD_WIDGET_DETAILS_CARGO_CARRIED,
	VLD_WIDGET_DETAILS_TRAIN_VEHICLES,
	VLD_WIDGET_DETAILS_CAPACITY_OF_EACH,
	VLD_WIDGET_DETAILS_TOTAL_CARGO,
};

/** Widgets of the vehicle lists (WC_INVALID). */
enum VehicleListWindowWidgets {
	VLW_WIDGET_CAPTION,
	VLW_WIDGET_SORT_ORDER,
	VLW_WIDGET_SORT_BY_PULLDOWN,
	VLW_WIDGET_LIST,
	VLW_WIDGET_SCROLLBAR,
	VLW_WIDGET_HIDE_BUTTONS,
	VLW_WIDGET_AVAILABLE_VEHICLES,
	VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN,
	VLW_WIDGET_STOP_ALL,
	VLW_WIDGET_START_ALL,
};


#endif /* WIDGETS_VEHICLE_WIDGET_H */
