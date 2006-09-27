/* $Id$ */

#ifndef VEHICLE_GUI_H
#define VEHICLE_GUI_H

#include "window.h"

void DrawVehicleProfitButton(const Vehicle *v, int x, int y);
void ShowVehicleRefitWindow(const Vehicle *v);
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
	VLW_MASK          = 0x700,
};

static inline bool ValidVLWFlags(uint16 flags)
{
	return (flags == VLW_STANDARD || flags == VLW_SHARED_ORDERS || flags == VLW_STATION_LIST);
}

void PlayerVehWndProc(Window *w, WindowEvent *e);

void DrawTrainEnginePurchaseInfo(int x, int y, EngineID engine_number);
void DrawTrainWagonPurchaseInfo(int x, int y, EngineID engine_number);
void DrawRoadVehPurchaseInfo(int x, int y, EngineID engine_number);
void DrawAircraftPurchaseInfo(int x, int y, EngineID engine_number);
void DrawShipPurchaseInfo(int x, int y, EngineID engine_number);

void DrawTrainImage(const Vehicle *v, int x, int y, int count, int skip, VehicleID selection);
void DrawRoadVehImage(const Vehicle *v, int x, int y, VehicleID selection);
void DrawShipImage(const Vehicle *v, int x, int y, VehicleID selection);
void DrawSmallOrderListShip(const Vehicle *v, int x, int y);
void DrawAircraftImage(const Vehicle *v, int x, int y, VehicleID selection);
void DrawSmallOrderListAircraft(const Vehicle *v, int x, int y);

void ShowBuildTrainWindow(TileIndex tile);
void ShowBuildRoadVehWindow(TileIndex tile);
void ShowBuildShipWindow(TileIndex tile);
void ShowBuildAircraftWindow(TileIndex tile);

void ChangeVehicleViewWindow(const Vehicle *from_v, const Vehicle *to_v);

int ShowAdditionalText(int x, int y, int w, EngineID engine);

#endif /* VEHICLE_GUI_H */
