#include "stdafx.h"
#include "ttd.h"

#include "vehicle.h"

/* General Vehicle GUI based procedures that are independent of vehicle types */
void InitializeVehiclesGuiList()
{
	bool *i;
	for (i = _train_sort_dirty; i != endof(_train_sort_dirty); i++)
		*i = true;

	for (i = _aircraft_sort_dirty; i != endof(_aircraft_sort_dirty); i++)
		*i = true;

	for (i = _ship_sort_dirty; i != endof(_ship_sort_dirty); i++)
		*i = true;

	for (i = _road_sort_dirty; i != endof(_road_sort_dirty); i++)
		*i = true;

	for (i = _vehicle_sort_dirty; i != endof(_vehicle_sort_dirty); i++)
		*i = true;

	//memset(_train_sort_dirty, true, sizeof(_train_sort_dirty));
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
	DrawSprite((SPR_OPENTTD_BASE + 10) | ormod, x, y);
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

int CDECL VehicleNumberSorter(const void *a, const void *b)
{
	const Vehicle *va = DEREF_VEHICLE((*(const SortStruct*)a).index);
	const Vehicle *vb = DEREF_VEHICLE((*(const SortStruct*)b).index);
	int r = va->unitnumber - vb->unitnumber;

	if (r == 0) // if the sorting criteria had the same value, sort by unitnumber
		r = va->unitnumber - vb->unitnumber;

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
		SET_DPARAM16(0, va->string_id);
		GetString(buf1, STR_0315);
	}

	if ( cmp2->index != _last_vehicle_idx) {
		_last_vehicle_idx = cmp2->index;
		_bufcache[0] = '\0';
		if (vb->string_id != _internal_name_sorter_id) {
			SET_DPARAM16(0, vb->string_id);
			GetString(_bufcache, STR_0315);
		}
	}

	r =  strcmp(buf1, _bufcache);	// sort by name

	if (r == 0) // if the sorting criteria had the same value, sort by unitnumber
		r = va->unitnumber - vb->unitnumber;

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleAgeSorter(const void *a, const void *b)
{
	const Vehicle *va = DEREF_VEHICLE((*(const SortStruct*)a).index);
	const Vehicle *vb = DEREF_VEHICLE((*(const SortStruct*)b).index);
	int r = va->age - vb->age;

	if (r == 0) // if the sorting criteria had the same value, sort by unitnumber
		r = va->unitnumber - vb->unitnumber;

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleProfitThisYearSorter(const void *a, const void *b)
{
	const Vehicle *va = DEREF_VEHICLE((*(const SortStruct*)a).index);
	const Vehicle *vb = DEREF_VEHICLE((*(const SortStruct*)b).index);
	int r = va->profit_this_year - vb->profit_this_year;

	if (r == 0) // if the sorting criteria had the same value, sort by unitnumber
		r = va->unitnumber - vb->unitnumber;

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleProfitLastYearSorter(const void *a, const void *b)
{
	const Vehicle *va = DEREF_VEHICLE((*(const SortStruct*)a).index);
	const Vehicle *vb = DEREF_VEHICLE((*(const SortStruct*)b).index);
	int r = va->profit_last_year - vb->profit_last_year;

	if (r == 0) // if the sorting criteria had the same value, sort by unitnumber
		r = va->unitnumber - vb->unitnumber;

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleCargoSorter(const void *a, const void *b)
{
	// FIXME - someone write a normal cargo sorter that also works by cargo_cap,
	// FIXME - since I seem to be unable to do so :S
	const Vehicle *va = DEREF_VEHICLE((*(const SortStruct*)a).index);
	const Vehicle *vb = DEREF_VEHICLE((*(const SortStruct*)b).index);
	int r = 0;
	int i;
	byte _cargo_counta[NUM_CARGO];
	byte _cargo_countb[NUM_CARGO];
	memset(_cargo_counta, 0, sizeof(_cargo_counta));
	memset(_cargo_countb, 0, sizeof(_cargo_countb));

	do {
		_cargo_counta[va->cargo_type]++;
	} while ( (va = va->next) != NULL);

	do {
		_cargo_countb[vb->cargo_type]++;
	} while ( (vb = vb->next) != NULL);

	for (i = 0; i < NUM_CARGO; i++) {
		r = _cargo_counta[i] - _cargo_countb[i];
		if (r != 0)
			break;
	}

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleReliabilitySorter(const void *a, const void *b)
{
	const Vehicle *va = DEREF_VEHICLE((*(const SortStruct*)a).index);
	const Vehicle *vb = DEREF_VEHICLE((*(const SortStruct*)b).index);
	int r = va->reliability - vb->reliability;

	if (r == 0) // if the sorting criteria had the same value, sort by unitnumber
		r = va->unitnumber - vb->unitnumber;

	return (_internal_sort_order & 1) ? -r : r;
}

int CDECL VehicleMaxSpeedSorter(const void *a, const void *b)
{
	const Vehicle *va = DEREF_VEHICLE((*(const SortStruct*)a).index);
	const Vehicle *vb = DEREF_VEHICLE((*(const SortStruct*)b).index);
	int r = va->max_speed - vb->max_speed;

	if (r == 0) // if the sorting criteria had the same value, sort by unitnumber
		r = va->unitnumber - vb->unitnumber;

	return (_internal_sort_order & 1) ? -r : r;
}
