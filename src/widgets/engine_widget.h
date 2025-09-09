/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file engine_widget.h Types related to the engine widgets. */

#ifndef WIDGETS_ENGINE_WIDGET_H
#define WIDGETS_ENGINE_WIDGET_H

#include "build_vehicle_widget.h"
#include "../vehicle_type.h"

static const uint8_t WID_EP_TOGGLES_OFFSET = 100;

/** Widgets of the #EnginePreviewWindow class. */
enum EnginePreviewWidgets : WidgetID {
	WID_EP_SORT_ASCENDING_DESCENDING = WID_BV_SORT_ASCENDING_DESCENDING, ///< Sort direction.
	WID_EP_CONFIGURE_BADGES = WID_BV_CONFIGURE_BADGES, ///< Button to configure badges.
	WID_EP_SORT_DROPDOWN = WID_BV_SORT_DROPDOWN, ///< Criteria of sorting dropdown.
	WID_EP_SCROLLBAR = WID_BV_SCROLLBAR, ///< Scrollbar of list.
	WID_EP_FILTER = WID_BV_FILTER, ///< Filter by name.
	WID_EP_LIST = WID_BV_LIST, ///< List of vehicles.

	/* Make sure that following ids do not collide with BV ones. */
	WID_EP_TRAIN_TOGGLE = WID_EP_TOGGLES_OFFSET + VEH_TRAIN, ///< Toggles visibility of trains.
	WID_EP_ROAD_VEHICLE_TOGGLE = WID_EP_TOGGLES_OFFSET + VEH_ROAD, ///< Toggles visibility of road vehicles.
	WID_EP_SHIP_TOGGLE = WID_EP_TOGGLES_OFFSET + VEH_SHIP, ///< Toggles visibility of ships.
	WID_EP_AIRCRAFT_TOGGLE = WID_EP_TOGGLES_OFFSET + VEH_AIRCRAFT, ///< Toggles visibility of aircrafts.

	WID_EP_SHOW_ALL_VEH_TYPES = WID_EP_TOGGLES_OFFSET + VEH_END, ///< Toggles visibility of all vehicles.
	WID_EP_LIST_CONTAINER, ///< Container for WID_EP_LIST and related widgets.
	WID_EP_TOGGLE_LIST, ///< Toggles visibility of WID_EP_LIST and related widgets.
	WID_EP_ACCEPT_ALL, ///< Accept all visible offers.
	WID_EP_QUESTION, ///< The container for the question.
	WID_EP_NO,       ///< No button.
	WID_EP_YES,      ///< Yes button.

	/* WID_EP_BADGE_FILTER has to be at the end */
	WID_EP_BADGE_FILTER, ///< Container for dropdown badge filters.
};

#endif /* WIDGETS_ENGINE_WIDGET_H */
