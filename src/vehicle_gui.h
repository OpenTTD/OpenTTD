/* $Id$ */

/** @file vehicle_gui.h */

#ifndef VEHICLE_GUI_H
#define VEHICLE_GUI_H

#include "window.h"
#include "vehicle.h"

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

/* Start of functions regarding vehicle list windows */
enum {
	PLY_WND_PRC__OFFSET_TOP_WIDGET = 26,
	PLY_WND_PRC__SIZE_OF_ROW_TINY  = 13,
	PLY_WND_PRC__SIZE_OF_ROW_SMALL = 26,
	PLY_WND_PRC__SIZE_OF_ROW_BIG   = 36,
	PLY_WND_PRC__SIZE_OF_ROW_BIG2  = 39,
};

/* Vehicle List Window type flags */
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

void DrawTrainImage(const Vehicle *v, int x, int y, int count, int skip, VehicleID selection);
void DrawRoadVehImage(const Vehicle *v, int x, int y, VehicleID selection);
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

void ShowReplaceVehicleWindow(VehicleType vehicletype);
void DrawSmallOrderList(const Vehicle *v, int x, int y);
void ShowReplaceGroupVehicleWindow(GroupID group, VehicleType veh);

static inline void DrawVehicleImage(const Vehicle *v, int x, int y, int count, int skip, VehicleID selection)
{
	switch (v->type) {
		case VEH_TRAIN:    DrawTrainImage(v, x, y, count, skip, selection); break;
		case VEH_ROAD:     DrawRoadVehImage(v, x, y, selection);            break;
		case VEH_SHIP:     DrawShipImage(v, x, y, selection);               break;
		case VEH_AIRCRAFT: DrawAircraftImage(v, x, y, selection);           break;
		default: NOT_REACHED();
	}
}

static inline uint GetVehicleListHeight(VehicleType type)
{
	return (type == VEH_TRAIN || type == VEH_ROAD) ? 14 : 24;
}

#endif /* VEHICLE_GUI_H */
