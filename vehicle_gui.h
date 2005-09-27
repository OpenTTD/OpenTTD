/* $Id$ */

#ifndef VEHICLE_GUI_H
#define VEHICLE_GUI_H

#include "station.h"
#include "vehicle.h"

struct vehiclelist_d;

void DrawVehicleProfitButton(const Vehicle *v, int x, int y);
CargoID DrawVehicleRefitWindow(const Vehicle *v, int sel);
void InitializeVehiclesGuiList(void);

/* sorter stuff */
void RebuildVehicleLists(void);
void ResortVehicleLists(void);

void BuildVehicleList(struct vehiclelist_d* vl, int type, PlayerID, StationID);
void SortVehicleList(struct vehiclelist_d *vl);

int CDECL GeneralOwnerSorter(const void *a, const void *b);

#define PERIODIC_RESORT_DAYS 10
#define SORT_BY_UNSORTED 0
extern const StringID _vehicle_sort_listing[];

enum VehicleSortTypes {
	VEHTRAIN     = 0,
	VEHROAD      = 1,
	VEHSHIP      = 2,
	VEHAIRCRAFT  = 3
};

typedef struct Listing {
	bool order;	// Ascending/descending?
	byte criteria;	// Sorting criteria
} Listing;

typedef struct Sorting {
	Listing aircraft;
	Listing roadveh;
	Listing ship;
	Listing train;
} Sorting;

extern Sorting _sorting;

enum {
  PLY_WND_PRC__OFFSET_TOP_WIDGET	= 26,
	PLY_WND_PRC__SIZE_OF_ROW_SMALL	= 26,
  PLY_WND_PRC__SIZE_OF_ROW_BIG		= 36,
};

void ShowReplaceVehicleWindow(byte vehicletype);

void DrawTrainEnginePurchaseInfo(int x, int y, EngineID engine_number);
void DrawTrainWagonPurchaseInfo(int x, int y, EngineID engine_number);
void DrawRoadVehPurchaseInfo(int x, int y, EngineID engine_number);
void DrawAircraftPurchaseInfo(int x, int y, EngineID engine_number);
void DrawShipPurchaseInfo(int x, int y, EngineID engine_number);


#endif /* VEHICLE_GUI_H */
