#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "vehicle.h"
#include "window.h"

VehicleSortListingTypeFunctions * const _vehicle_sorter[] = {
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

const StringID _vehicle_sort_listing[] = {
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

void RebuildVehicleLists(void)
{
	Window *w;

	for (w = _windows; w != _last_window; ++w)
		switch (w->window_class) {
			case WC_TRAINS_LIST:
			case WC_ROADVEH_LIST:
			case WC_SHIPS_LIST:
			case WC_AIRCRAFT_LIST:
				WP(w, vehiclelist_d).flags |= VL_REBUILD;
				SetWindowDirty(w);
				break;

			default:
				break;
		}
}

void ResortVehicleLists(void)
{
	Window *w;

	for (w = _windows; w != _last_window; ++w)
		switch (w->window_class) {
			case WC_TRAINS_LIST:
			case WC_ROADVEH_LIST:
			case WC_SHIPS_LIST:
			case WC_AIRCRAFT_LIST:
				WP(w, vehiclelist_d).flags |= VL_RESORT;
				SetWindowDirty(w);
				break;

			default:
				break;
		}
}

void BuildVehicleList(vehiclelist_d *vl, int type, int owner, int station)
{
	SortStruct sort_list[NUM_NORMAL_VEHICLES];
	int subtype = (type != VEH_Aircraft) ? 0 : 2;
	int n = 0;
	int i;

	if (!(vl->flags & VL_REBUILD)) return;

	DEBUG(misc, 1) ("Building vehicle list for player %d station %d...",
		owner, station);	

	if (station != -1) {
		const Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == type && v->subtype <= subtype) {
				const Order *ord;
				for (ord = v->schedule_ptr; ord->type != OT_NOTHING; ++ord)
					if (ord->type == OT_GOTO_STATION && ord->station == station) {
						sort_list[n].index = v - _vehicles;
						sort_list[n].owner = v->owner;
						++n;
						break;
					}
			}
		}
	} else {
		const Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == type && v->subtype <= subtype && v->owner == owner) {
				sort_list[n].index = v - _vehicles;
				sort_list[n].owner = v->owner;
				++n;
			}
		}
	}

	vl->sort_list = realloc(vl->sort_list, n * sizeof(vl->sort_list[0])); /* XXX unchecked malloc */
	vl->list_length = n;

	for (i = 0; i < n; ++i)
		vl->sort_list[i] = sort_list[i];

	vl->flags &= ~VL_REBUILD;
	vl->flags |= VL_RESORT;
}

void SortVehicleList(vehiclelist_d *vl)
{
	if (!(vl->flags & VL_RESORT)) return;

	_internal_sort_order = vl->flags & VL_DESC;
	_internal_name_sorter_id = STR_SV_TRAIN_NAME;
	_last_vehicle_idx = 0; // used for "cache" in namesorting
	qsort(vl->sort_list, vl->list_length, sizeof(vl->sort_list[0]),
		_vehicle_sorter[vl->sort_type]);

	vl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
	vl->flags &= ~VL_RESORT;
}


/* General Vehicle GUI based procedures that are independent of vehicle types */
void InitializeVehiclesGuiList()
{
}

// draw the vehicle profit button in the vehicle list window.
void DrawVehicleProfitButton(Vehicle *v, int x, int y)
{
	uint32 ormod;

	// draw profit-based colored icons
	if(v->age <= 365 * 2)
		ormod = 0x3158000; // grey
	else if(v->profit_last_year < 0)
		ormod = 0x30b8000; //red
	else if(v->profit_last_year < 10000)
		ormod = 0x30a8000; // yellow
	else
		ormod = 0x30d8000; // green
	DrawSprite((SPR_BLOT) | ormod, x, y);
}

/************ Sorter functions *****************/
int CDECL GeneralOwnerSorter(const void *a, const void *b)
{
	return (*(const SortStruct*)a).owner - (*(const SortStruct*)b).owner;
}

/* Variables you need to set before calling this function!
* 1. (byte)_internal_sort_type:					sorting criteria to sort on
* 2. (bool)_internal_sort_order:				sorting order, descending/ascending
* 3. (uint32)_internal_name_sorter_id:	default StringID of the vehicle when no name is set. eg
*    STR_SV_TRAIN_NAME for trains or STR_SV_AIRCRAFT_NAME for aircraft
*/
int CDECL VehicleUnsortedSorter(const void *a, const void *b)
{
	return DEREF_VEHICLE((*(const SortStruct*)a).index)->index - DEREF_VEHICLE((*(const SortStruct*)b).index)->index;
}

// if the sorting criteria had the same value, sort vehicle by unitnumber
#define VEHICLEUNITNUMBERSORTER(r, a, b) {if (r == 0) {r = a->unitnumber - b->unitnumber;}}

int CDECL VehicleNumberSorter(const void *a, const void *b)
{
	const Vehicle *va = DEREF_VEHICLE((*(const SortStruct*)a).index);
	const Vehicle *vb = DEREF_VEHICLE((*(const SortStruct*)b).index);
	int r = va->unitnumber - vb->unitnumber;

	return (_internal_sort_order & 1) ? -r : r;
}

static char _bufcache[64];	// used together with _last_vehicle_idx to hopefully speed up stringsorting
int CDECL VehicleNameSorter(const void *a, const void *b)
{
	const SortStruct *cmp1 = (const SortStruct*)a;
	const SortStruct *cmp2 = (const SortStruct*)b;
	const Vehicle *va = DEREF_VEHICLE(cmp1->index);
	const Vehicle *vb = DEREF_VEHICLE(cmp2->index);
	char buf1[64] = "\0";
	int r;

	if (va->string_id != _internal_name_sorter_id) {
		SetDParam(0, va->string_id);
		GetString(buf1, STR_0315);
	}

	if ( cmp2->index != _last_vehicle_idx) {
		_last_vehicle_idx = cmp2->index;
		_bufcache[0] = '\0';
		if (vb->string_id != _internal_name_sorter_id) {
			SetDParam(0, vb->string_id);
			GetString(_bufcache, STR_0315);
		}
	}

	r =  strcmp(buf1, _bufcache);	// sort by name

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleAgeSorter(const void *a, const void *b)
{
	const Vehicle *va = DEREF_VEHICLE((*(const SortStruct*)a).index);
	const Vehicle *vb = DEREF_VEHICLE((*(const SortStruct*)b).index);
	int r = va->age - vb->age;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleProfitThisYearSorter(const void *a, const void *b)
{
	const Vehicle *va = DEREF_VEHICLE((*(const SortStruct*)a).index);
	const Vehicle *vb = DEREF_VEHICLE((*(const SortStruct*)b).index);
	int r = va->profit_this_year - vb->profit_this_year;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleProfitLastYearSorter(const void *a, const void *b)
{
	const Vehicle *va = DEREF_VEHICLE((*(const SortStruct*)a).index);
	const Vehicle *vb = DEREF_VEHICLE((*(const SortStruct*)b).index);
	int r = va->profit_last_year - vb->profit_last_year;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleCargoSorter(const void *a, const void *b)
{
	const Vehicle *va = DEREF_VEHICLE((*(const SortStruct*)a).index);
	const Vehicle *vb = DEREF_VEHICLE((*(const SortStruct*)b).index);
	const Vehicle *v;
	int r = 0;
	int i;
	uint _cargo_counta[NUM_CARGO];
	uint _cargo_countb[NUM_CARGO];
	memset(_cargo_counta, 0, sizeof(_cargo_counta));
	memset(_cargo_countb, 0, sizeof(_cargo_countb));

	for (v = va; v != NULL; v = v->next)
		_cargo_counta[v->cargo_type] += v->cargo_cap;

	for (v = vb; v != NULL; v = v->next)
		_cargo_countb[v->cargo_type] += v->cargo_cap;

	for (i = 0; i < NUM_CARGO; i++) {
		r = _cargo_counta[i] - _cargo_countb[i];
		if (r != 0)
			break;
	}

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleReliabilitySorter(const void *a, const void *b)
{
	const Vehicle *va = DEREF_VEHICLE((*(const SortStruct*)a).index);
	const Vehicle *vb = DEREF_VEHICLE((*(const SortStruct*)b).index);
	int r = va->reliability - vb->reliability;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleMaxSpeedSorter(const void *a, const void *b)
{
	const Vehicle *va = DEREF_VEHICLE((*(const SortStruct*)a).index);
	const Vehicle *vb = DEREF_VEHICLE((*(const SortStruct*)b).index);
	int r = va->max_speed - vb->max_speed;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}
