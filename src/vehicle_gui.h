/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehicle_gui.h Functions related to the vehicle's GUIs. */

#ifndef VEHICLE_GUI_H
#define VEHICLE_GUI_H

#include "window_type.h"
#include "vehicle_type.h"
#include "order_type.h"
#include "station_type.h"
#include "engine_type.h"
#include "tile_type.h"

void ShowVehicleRefitWindow(const Vehicle *v, VehicleOrderID order, Window *parent);

/** Constants of vehicle view widget indices */
enum VehicleViewWindowWidgets {
	VVW_WIDGET_CLOSEBOX = 0,
	VVW_WIDGET_CAPTION,
	VVW_WIDGET_STICKY,
	VVW_WIDGET_PANEL,
	VVW_WIDGET_INSET,
	VVW_WIDGET_VIEWPORT,           ///< Viewport widget.
	VVW_WIDGET_START_STOP_VEH,
	VVW_WIDGET_CENTER_MAIN_VIEH,
	VVW_WIDGET_GOTO_DEPOT,
	VVW_WIDGET_REFIT_VEH,
	VVW_WIDGET_SHOW_ORDERS,
	VVW_WIDGET_SHOW_DETAILS,
	VVW_WIDGET_CLONE_VEH,
	VVW_WIDGET_EMPTY_BOTTOM_RIGHT,
	VVW_WIDGET_SELECT_DEPOT_CLONE, ///< Selection widget between 'goto depot', and 'clone vehicle' buttons.
	VVW_WIDGET_SELECT_REFIT_TURN,  ///< Selection widget between 'refit' and 'turn around' buttons.
	VVW_WIDGET_RESIZE,
	VVW_WIDGET_TURN_AROUND,
	VVW_WIDGET_FORCE_PROCEED,
};

/** The tabs in the train details window */
enum TrainDetailsWindowTabs {
	TDW_TAB_CARGO = 0, ///< Tab with cargo carried by the vehicles
	TDW_TAB_INFO,      ///< Tab with name and value of the vehicles
	TDW_TAB_CAPACITY,  ///< Tab with cargo capacity of the vehicles
	TDW_TAB_TOTALS,    ///< Tab with sum of total cargo transported
};

/** Vehicle List Window type flags */
enum {
	VLW_STANDARD      = 0 << 8,
	VLW_SHARED_ORDERS = 1 << 8,
	VLW_STATION_LIST  = 2 << 8,
	VLW_DEPOT_LIST    = 3 << 8,
	VLW_GROUP_LIST    = 4 << 8,
	VLW_WAYPOINT_LIST = 5 << 8,
	VLW_MASK          = 0x700,
};

static inline bool ValidVLWFlags(uint16 flags)
{
	return (flags == VLW_STANDARD || flags == VLW_SHARED_ORDERS || flags == VLW_STATION_LIST || flags == VLW_DEPOT_LIST || flags == VLW_GROUP_LIST);
}

int DrawVehiclePurchaseInfo(int left, int right, int y, EngineID engine_number);

void DrawTrainImage(const Train *v, int x, int y, VehicleID selection, int max_width, int skip);
void DrawRoadVehImage(const Vehicle *v, int x, int y, VehicleID selection, int max_width);
void DrawShipImage(const Vehicle *v, int x, int y, VehicleID selection);
void DrawAircraftImage(const Vehicle *v, int x, int y, VehicleID selection);

void ShowBuildVehicleWindow(TileIndex tile, VehicleType type);

uint ShowRefitOptionsList(int left, int right, int y, EngineID engine);
StringID GetCargoSubtypeText(const Vehicle *v);

void ShowVehicleListWindow(const Vehicle *v);
void ShowVehicleListWindow(CompanyID company, VehicleType vehicle_type, const Waypoint *wp);
void ShowVehicleListWindow(CompanyID company, VehicleType vehicle_type);
void ShowVehicleListWindow(CompanyID company, VehicleType vehicle_type, StationID station);
void ShowVehicleListWindow(CompanyID company, VehicleType vehicle_type, TileIndex depot_tile);


static inline uint GetVehicleListHeight(VehicleType type)
{
	return (type == VEH_TRAIN || type == VEH_ROAD) ? 14 : 24;
}

/** Get WindowClass for vehicle list of given vehicle type
 * @param vt vehicle type to check
 * @return corresponding window class
 * @note works only for company buildable vehicle types
 */
static inline WindowClass GetWindowClassForVehicleType(VehicleType vt)
{
	switch (vt) {
		default: NOT_REACHED();
		case VEH_TRAIN:    return WC_TRAINS_LIST;
		case VEH_ROAD:     return WC_ROADVEH_LIST;
		case VEH_SHIP:     return WC_SHIPS_LIST;
		case VEH_AIRCRAFT: return WC_AIRCRAFT_LIST;
	}
}

/* Unified window procedure */
void ShowVehicleViewWindow(const Vehicle *v);

Vehicle *CheckClickOnVehicle(const struct ViewPort *vp, int x, int y);

#endif /* VEHICLE_GUI_H */
