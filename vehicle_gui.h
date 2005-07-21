#ifndef VEHICLE_GUI_H
#define VEHICLE_GUI_H

#include "vehicle.h"

struct vehiclelist_d;

void DrawVehicleProfitButton(Vehicle *v, int x, int y);
CargoID DrawVehicleRefitWindow(const Vehicle *v, int sel);
void InitializeVehiclesGuiList(void);

/* sorter stuff */
void RebuildVehicleLists(void);
void ResortVehicleLists(void);

void BuildVehicleList(struct vehiclelist_d *vl, int type, int owner, int station);
void SortVehicleList(struct vehiclelist_d *vl);

int CDECL GeneralOwnerSorter(const void *a, const void *b);

#define PERIODIC_RESORT_DAYS 10
#define DEF_SORTER(yyyy) int CDECL yyyy(const void *a, const void *b)

DEF_SORTER(VehicleUnsortedSorter);
DEF_SORTER(VehicleNumberSorter);
DEF_SORTER(VehicleNameSorter);
DEF_SORTER(VehicleAgeSorter);
DEF_SORTER(VehicleProfitThisYearSorter);
DEF_SORTER(VehicleProfitLastYearSorter);
DEF_SORTER(VehicleCargoSorter);
DEF_SORTER(VehicleReliabilitySorter);
DEF_SORTER(VehicleMaxSpeedSorter);

typedef DEF_SORTER(VehicleSortListingTypeFunctions);

#define SORT_BY_UNSORTED 0
extern VehicleSortListingTypeFunctions * const _vehicle_sorter[];
extern const StringID _vehicle_sort_listing[];
extern const StringID _rail_types_list[];

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
