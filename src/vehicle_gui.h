/* $Id$ */

#ifndef VEHICLE_GUI_H
#define VEHICLE_GUI_H

#include "window.h"
#include "vehicle.h"

void DrawVehicleProfitButton(const Vehicle *v, int x, int y);
void ShowVehicleRefitWindow(const Vehicle *v, VehicleOrderID order);
void InitializeVehiclesGuiList(void);

/* sorter stuff */
void RebuildVehicleLists(void);
void ResortVehicleLists(void);

#define PERIODIC_RESORT_DAYS 10

/* Vehicle List Window type flags */
enum {
	VLW_STANDARD      = 0 << 8,
	VLW_SHARED_ORDERS = 1 << 8,
	VLW_STATION_LIST  = 2 << 8,
	VLW_DEPOT_LIST    = 3 << 8,
	VLW_MASK          = 0x700,
};

static inline bool ValidVLWFlags(uint16 flags)
{
	return (flags == VLW_STANDARD || flags == VLW_SHARED_ORDERS || flags == VLW_STATION_LIST || flags == VLW_DEPOT_LIST);
}

void PlayerVehWndProc(Window *w, WindowEvent *e);

void DrawTrainEnginePurchaseInfo(int x, int y, uint w, EngineID engine_number);
void DrawTrainWagonPurchaseInfo(int x, int y, uint w, EngineID engine_number);
void DrawRoadVehPurchaseInfo(int x, int y, uint w, EngineID engine_number);
void DrawAircraftPurchaseInfo(int x, int y, uint w, EngineID engine_number);
void DrawShipPurchaseInfo(int x, int y, uint w, EngineID engine_number);

void DrawTrainImage(const Vehicle *v, int x, int y, int count, int skip, VehicleID selection);
void DrawRoadVehImage(const Vehicle *v, int x, int y, VehicleID selection);
void DrawShipImage(const Vehicle *v, int x, int y, VehicleID selection);
void DrawAircraftImage(const Vehicle *v, int x, int y, VehicleID selection);

void ShowBuildTrainWindow(TileIndex tile);
void ShowBuildRoadVehWindow(TileIndex tile);
void ShowBuildShipWindow(TileIndex tile);
void ShowBuildVehicleWindow(TileIndex tile, byte type);

void ChangeVehicleViewWindow(const Vehicle *from_v, const Vehicle *to_v);

uint ShowAdditionalText(int x, int y, uint w, EngineID engine);
uint ShowRefitOptionsList(int x, int y, uint w, EngineID engine);

void ShowVehicleListWindow(const Vehicle *v);
void ShowVehicleListWindow(PlayerID player, byte vehicle_type);
void ShowVehicleListWindow(PlayerID player, byte vehicle_type, StationID station);
void ShowVehicleListWindow(PlayerID player, byte vehicle_type, TileIndex depot_tile);


static inline void DrawVehicleImage(const Vehicle *v, int x, int y, int count, int skip, VehicleID selection)
{
	switch (v->type) {
		case VEH_Train:    DrawTrainImage(v, x, y, count, skip, selection); break;
		case VEH_Road:     DrawRoadVehImage(v, x, y, selection);            break;
		case VEH_Ship:     DrawShipImage(v, x, y, selection);               break;
		case VEH_Aircraft: DrawAircraftImage(v, x, y, selection);           break;
		default: NOT_REACHED();
	}
}

static inline byte GetVehicleListHeight(byte type)
{
	return (type == VEH_Train || type == VEH_Road) ? 14 : 24;
}

#endif /* VEHICLE_GUI_H */
