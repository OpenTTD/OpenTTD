/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file build_vehicle_widget.h Types related to the build_vehicle widgets. */

#ifndef WIDGETS_BUILD_VEHICLE_WIDGET_H
#define WIDGETS_BUILD_VEHICLE_WIDGET_H

/** Widgets of the #BuildVehicleWindow class. */
enum BuildVehicleWidgets {
	WID_BV_CAPTION,                   ///< Caption of window.
	WID_BV_SORT_ASCENDING_DESCENDING, ///< Sort direction.
	WID_BV_SORT_DROPDOWN,             ///< Criteria of sorting dropdown.
	WID_BV_CARGO_FILTER_DROPDOWN,     ///< Cargo filter dropdown.
	WID_BV_SHOW_HIDDEN_ENGINES,       ///< Toggle whether to display the hidden vehicles.
	WID_BV_LIST,                      ///< List of vehicles.
	WID_BV_SCROLLBAR,                 ///< Scrollbar of list.
	WID_BV_PANEL,                     ///< Button panel.
	WID_BV_BUILD,                     ///< Build panel.
	WID_BV_SHOW_HIDE,                 ///< Button to hide or show the selected engine.
	WID_BV_BUILD_SEL,                 ///< Build button.
	WID_BV_RENAME,                    ///< Rename button.
	/* build vehicle smart: */
	WID_BV_FILTER_BUTTON,             ///< Show filters panel.
	WID_BV_FILTER_CONTAINER,          ///< Filters container.
	WID_BV_RAIL_WAGON,                ///< Show/hide wagons.
	WID_BV_RAIL_MOTOR_WAGON,          ///< Show/hide motor-wagons.
	WID_BV_RAIL_LOCOMOTIVE,           ///< Show/hide locomotive.
	WID_BV_RAIL_WEIGHT_INCREASE,      ///< Increase train weight.
	WID_BV_RAIL_WEIGHT_DECREASE,      ///< Decrease train weight.
	WID_BV_RAIL_WEIGHT_EDIT,          ///< Edit train weight directly.
	WID_BV_RAIL_SPEED_INCREASE,       ///< Increase train speed.
	WID_BV_RAIL_SPEED_DECREASE,       ///< Decrease train speed.
	WID_BV_RAIL_SPEED_EDIT,           ///< Edit train speed directly.
	WID_BV_TRAIN_LENGTH_INCREASE,     ///< Increase train length.
	WID_BV_TRAIN_LENGTH_DECREASE,     ///< Decrease train length.
	WID_BV_TRAIN_LENGTH_EDIT,         ///< Edit train length directly.
	WID_BV_SLOPE_LENGTH_INCREASE,     ///< Increase slope length.
	WID_BV_SLOPE_LENGTH_DECREASE,     ///< Decrease slope length.
	WID_BV_SLOPE_LENGTH_EDIT,         ///< Edit slope length directly.
	WID_BV_SLOPE,                     ///< Calculate the effect of the slopes.
	WID_BV_TRAIN_FROM_DEPOT,          ///< Take settings from train in depot.
};

#endif /* WIDGETS_BUILD_VEHICLE_WIDGET_H */
