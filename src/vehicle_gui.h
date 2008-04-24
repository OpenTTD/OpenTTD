/* $Id$ */

/** @file vehicle_gui.h */

#ifndef VEHICLE_GUI_H
#define VEHICLE_GUI_H

#include "window_gui.h"
#include "vehicle_type.h"
#include "order_type.h"
#include "station_type.h"
#include "engine_type.h"

void DrawVehicleProfitButton(const Vehicle *v, int x, int y);
void ShowVehicleRefitWindow(const Vehicle *v, VehicleOrderID order);
void InitializeVehiclesGuiList();

/* sorter stuff */
void RebuildVehicleLists();
void ResortVehicleLists();
void SortVehicleList(vehiclelist_d *vl);
void BuildVehicleList(vehiclelist_d *vl, PlayerID owner, uint16 index, uint16 window_type);

#define PERIODIC_RESORT_DAYS 10

extern const StringID _vehicle_sort_listing[];

/** Constants of vehicle view widget indices */
enum VehicleViewWindowWidgets {
	VVW_WIDGET_CLOSEBOX = 0,
	VVW_WIDGET_CAPTION,
	VVW_WIDGET_STICKY,
	VVW_WIDGET_PANEL,
	VVW_WIDGET_VIEWPORT,
	VVW_WIDGET_START_STOP_VEH,
	VVW_WIDGET_CENTER_MAIN_VIEH,
	VVW_WIDGET_GOTO_DEPOT,
	VVW_WIDGET_REFIT_VEH,
	VVW_WIDGET_SHOW_ORDERS,
	VVW_WIDGET_SHOW_DETAILS,
	VVW_WIDGET_CLONE_VEH,
	VVW_WIDGET_EMPTY_BOTTOM_RIGHT,
	VVW_WIDGET_RESIZE,
	VVW_WIDGET_TURN_AROUND,
	VVW_WIDGET_FORCE_PROCEED,
};

/** Start of functions regarding vehicle list windows */
enum {
	PLY_WND_PRC__OFFSET_TOP_WIDGET = 26,
	PLY_WND_PRC__SIZE_OF_ROW_TINY  = 13,
	PLY_WND_PRC__SIZE_OF_ROW_SMALL = 26,
	PLY_WND_PRC__SIZE_OF_ROW_BIG   = 36,
	PLY_WND_PRC__SIZE_OF_ROW_BIG2  = 39,
};

/** Vehicle List Window type flags */
enum {
	VLW_STANDARD      = 0 << 8,
	VLW_SHARED_ORDERS = 1 << 8,
	VLW_STATION_LIST  = 2 << 8,
	VLW_DEPOT_LIST    = 3 << 8,
	VLW_GROUP_LIST    = 4 << 8,
	VLW_MASK          = 0x700,
};

static inline bool ValidVLWFlags(uint16 flags)
{
	return (flags == VLW_STANDARD || flags == VLW_SHARED_ORDERS || flags == VLW_STATION_LIST || flags == VLW_DEPOT_LIST || flags == VLW_GROUP_LIST);
}

void PlayerVehWndProc(Window *w, WindowEvent *e);

int DrawVehiclePurchaseInfo(int x, int y, uint w, EngineID engine_number);

void DrawTrainImage(const Vehicle *v, int x, int y, VehicleID selection, int count, int skip);
void DrawRoadVehImage(const Vehicle *v, int x, int y, VehicleID selection, int count);
void DrawShipImage(const Vehicle *v, int x, int y, VehicleID selection);
void DrawAircraftImage(const Vehicle *v, int x, int y, VehicleID selection);

void ShowBuildVehicleWindow(TileIndex tile, VehicleType type);

void ChangeVehicleViewWindow(const Vehicle *from_v, const Vehicle *to_v);

uint ShowAdditionalText(int x, int y, uint w, EngineID engine);
uint ShowRefitOptionsList(int x, int y, uint w, EngineID engine);

void ShowVehicleListWindow(const Vehicle *v);
void ShowVehicleListWindow(PlayerID player, VehicleType vehicle_type);
void ShowVehicleListWindow(PlayerID player, VehicleType vehicle_type, StationID station);
void ShowVehicleListWindow(PlayerID player, VehicleType vehicle_type, TileIndex depot_tile);

void DrawSmallOrderList(const Vehicle *v, int x, int y);

void DrawVehicleImage(const Vehicle *v, int x, int y, VehicleID selection, int count, int skip);

static inline uint GetVehicleListHeight(VehicleType type)
{
	return (type == VEH_TRAIN || type == VEH_ROAD) ? 14 : 24;
}

/** Get WindowClass for vehicle list of given vehicle type
 * @param vt vehicle type to check
 * @return corresponding window class
 * @note works only for player buildable vehicle types
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

Vehicle *CheckClickOnVehicle(const ViewPort *vp, int x, int y);

#endif /* VEHICLE_GUI_H */
