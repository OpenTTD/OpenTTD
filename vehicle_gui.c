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
	return (*(SortStruct*)a).owner - (*(SortStruct*)b).owner;
}

static char _bufcache[64];	// used together with _last_vehicle_idx to hopefully speed up stringsorting

/* Variables you need to set before calling this function!
* 1. (uint32)_internal_name_sorter_id:	default StringID of the vehicle when no name is set. eg
*    STR_SV_TRAIN_NAME for trains or STR_SV_AIRCRAFT_NAME for aircraft
* 2. (bool)_internal_sort_order:				sorting order, descending/ascending
*/
int CDECL VehicleNameSorter(const void *a, const void *b)
{
	SortStruct *cmp1 = (SortStruct*)a;
	SortStruct *cmp2 = (SortStruct*)b;
	Vehicle *v;
	char buf1[64] = "\0";
	int r;

	v = DEREF_VEHICLE(cmp1->index);
	if (v->string_id != _internal_name_sorter_id) {
		SET_DPARAM16(0, v->string_id);
		GetString(buf1, STR_0315);
	}

	if ( cmp2->index != _last_vehicle_idx) {
		_last_vehicle_idx = cmp2->index;
		v = DEREF_VEHICLE(cmp2->index);
		_bufcache[0] = '\0';
		if (v->string_id != _internal_name_sorter_id) {
			SET_DPARAM16(0, v->string_id);
			GetString(_bufcache, STR_0315);
		}
	}

	r =  strcmp(buf1, _bufcache);	// sort by name
	if (_internal_sort_order & 1) r = -r;
	return r;
}

/* Variables you need to set before calling this function!
* 1. (byte)_internal_sort_type:		the criteria by which to sort these vehicles (number, age, etc)
* 2. (bool)_internal_sort_order:	sorting order, descending/ascending
*/
int CDECL GeneralVehicleSorter(const void *a, const void *b)
{
	SortStruct *cmp1 = (SortStruct*)a;
	SortStruct *cmp2 = (SortStruct*)b;
	Vehicle *va = DEREF_VEHICLE(cmp1->index);
	Vehicle *vb = DEREF_VEHICLE(cmp2->index);
	int r;

	switch (_internal_sort_type) {
		case SORT_BY_UNSORTED: /* Sort unsorted */
			return va->index - vb->index;
		case SORT_BY_NUMBER: /* Sort by Number */
			r = va->unitnumber - vb->unitnumber;
			break;
		/* case SORT_BY_NAME: Sort by Name (VehicleNameSorter)*/
		case SORT_BY_AGE: /* Sort by Age */
			r = va->age - vb->age;
			break;
		case SORT_BY_PROFIT_THIS_YEAR: /* Sort by Profit this year */
			r = va->profit_this_year - vb->profit_this_year;
			break;
		case SORT_BY_PROFIT_LAST_YEAR: /* Sort by Profit last year */
			r = va->profit_last_year - vb->profit_last_year;
			break;
		case SORT_BY_TOTAL_CAPACITY_PER_CARGOTYPE: { /* Sort by Total capacity per cargotype */
			// FIXME - someone write a normal cargo sorter that also works by cargo_cap,
			// FIXME - since I seem to be unable to do so :S
			Vehicle *ua = va;
			Vehicle *ub = vb;
			int i;
			byte _cargo_counta[NUM_CARGO];
			byte _cargo_countb[NUM_CARGO];
			do {
				_cargo_counta[ua->cargo_type]++;
			} while ( (ua = ua->next) != NULL);
			do {
				_cargo_countb[ub->cargo_type]++;
			} while ( (ub = ub->next) != NULL);

			for (i = 0; i < NUM_CARGO; i++) {
				r = _cargo_counta[i] - _cargo_countb[i];
				if (r != 0)
					break;
			}
		} break;
		case SORT_BY_RELIABILITY: /* Sort by Reliability */
			r = va->reliability - vb->reliability;
			break;
		case SORT_BY_MAX_SPEED: /* Sort by Max speed */
			r = va->max_speed - vb->max_speed;
			break;
		default: NOT_REACHED();
	}
	
	if (_internal_sort_order & 1) r = -r;
	return r;
}
