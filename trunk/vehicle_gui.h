#ifndef VEHICLE_GUI_H
#define VEHICLE_GUI_H

void DrawVehicleProfitButton(Vehicle *v, int x, int y);
void InitializeVehiclesGuiList();

/* sorter stuff */
typedef struct SortStruct { // store owner through sorting process
	uint32	index;
	byte		owner;
} SortStruct;

int CDECL GeneralOwnerSorter(const void *a, const void *b);
void VehicleSorter(SortStruct *firstelement, uint32 n, uint16 size);
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

static VehicleSortListingTypeFunctions * const _vehicle_sorter[] = {
	&VehicleUnsortedSorter,
	&VehicleNumberSorter,
	&VehicleNameSorter,
	&VehicleAgeSorter,
	&VehicleProfitThisYearSorter,
	&VehicleProfitLastYearSorter,
	&VehicleCargoSorter,
	&VehicleReliabilitySorter,
	&VehicleMaxSpeedSorter
};

#define SORT_BY_UNSORTED 0

static const uint16 _vehicle_sort_listing[] = {
	STR_SORT_BY_UNSORTED,
	STR_SORT_BY_NUMBER,
	STR_SORT_BY_DROPDOWN_NAME,
	STR_SORT_BY_AGE,
	STR_SORT_BY_PROFIT_THIS_YEAR,
	STR_SORT_BY_PROFIT_LAST_YEAR,
	STR_SORT_BY_TOTAL_CAPACITY_PER_CARGOTYPE,
	STR_SORT_BY_RELIABILITY,
	STR_SORT_BY_MAX_SPEED,
	INVALID_STRING_ID
};

enum VehicleSortTypes {
	VEHTRAIN		= 0,
	VEHROAD			= 1,
	VEHSHIP			= 2,
	VEHAIRCRAFT	= 3
};

VARDEF bool _vehicle_sort_dirty[4];	// global sort, vehicles added/removed (4 types of vehicles)

VARDEF bool _train_sort_dirty[MAX_PLAYERS];			// vehicles for a given player needs to be resorted (new criteria)
VARDEF byte _train_sort_type[MAX_PLAYERS];			// different criteria for sorting
VARDEF bool _train_sort_order[MAX_PLAYERS];			// sort descending/ascending

VARDEF bool _aircraft_sort_dirty[MAX_PLAYERS];	// vehicles for a given player needs to be resorted (new criteria)
VARDEF byte _aircraft_sort_type[MAX_PLAYERS];		// different criteria for sorting
VARDEF bool _aircraft_sort_order[MAX_PLAYERS];	// sort descending/ascending

VARDEF bool _ship_sort_dirty[MAX_PLAYERS];			// vehicles for a given player needs to be resorted (new criteria)
VARDEF byte _ship_sort_type[MAX_PLAYERS];				// different criteria for sorting
VARDEF bool _ship_sort_order[MAX_PLAYERS];			// sort descending/ascending

VARDEF bool _road_sort_dirty[MAX_PLAYERS];			// vehicles for a given player needs to be resorted (new criteria)
VARDEF byte _road_sort_type[MAX_PLAYERS];				// different criteria for sorting
VARDEF bool _road_sort_order[MAX_PLAYERS];			// sort descending/ascending

enum {
  PLY_WND_PRC__OFFSET_TOP_WIDGET	= 26,
	PLY_WND_PRC__SIZE_OF_ROW_SMALL	= 26,
  PLY_WND_PRC__SIZE_OF_ROW_BIG		= 36,
};

#endif /* VEHICLE_GUI_H */
