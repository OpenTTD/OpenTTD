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
VARDEF uint32	_internal_name_sorter_id;	// internal StringID for default vehicle-names
VARDEF uint32	_last_vehicle_idx;				// cached index to hopefully speed up name-sorting
VARDEF bool		_internal_sort_order;			// descending/ascending

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

VARDEF Sorting _sorting;

enum {
  PLY_WND_PRC__OFFSET_TOP_WIDGET	= 26,
	PLY_WND_PRC__SIZE_OF_ROW_SMALL	= 26,
  PLY_WND_PRC__SIZE_OF_ROW_BIG		= 36,
};

void ShowReplaceVehicleWindow(byte vehicletype);

void Set_DPARAM_Train_Engine_Build_Window(uint16 engine_number);
void Set_DPARAM_Train_Car_Build_Window(Window *w, uint16 engine_number);
void Set_DPARAM_Road_Veh_Build_Window(uint16 engine_number);
void Set_DPARAM_Aircraft_Build_Window(uint16 engine_number);
void Set_DPARAM_Ship_Build_Window(uint16 engine_number);


#endif /* VEHICLE_GUI_H */
