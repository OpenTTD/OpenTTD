/* $Id$ */

/** @file vehicle_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "player.h"
#include "station.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "engine.h"
#include "gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "command_func.h"
#include "variables.h"
#include "vehicle_gui.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "train.h"
#include "newgrf_callbacks.h"
#include "newgrf_engine.h"
#include "newgrf_text.h"
#include "ship.h"
#include "aircraft.h"
#include "roadveh.h"
#include "depot.h"
#include "cargotype.h"
#include "group.h"
#include "group_gui.h"
#include "strings_func.h"
#include "functions.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "autoreplace_gui.h"
#include "core/alloc_func.hpp"
#include "string_func.h"

struct Sorting {
	Listing aircraft;
	Listing roadveh;
	Listing ship;
	Listing train;
};

static Sorting _sorting;

static bool   _internal_sort_order;     // descending/ascending

typedef int CDECL VehicleSortListingTypeFunction(const void*, const void*);

static VehicleSortListingTypeFunction VehicleNumberSorter;
static VehicleSortListingTypeFunction VehicleNameSorter;
static VehicleSortListingTypeFunction VehicleAgeSorter;
static VehicleSortListingTypeFunction VehicleProfitThisYearSorter;
static VehicleSortListingTypeFunction VehicleProfitLastYearSorter;
static VehicleSortListingTypeFunction VehicleCargoSorter;
static VehicleSortListingTypeFunction VehicleReliabilitySorter;
static VehicleSortListingTypeFunction VehicleMaxSpeedSorter;
static VehicleSortListingTypeFunction VehicleModelSorter;
static VehicleSortListingTypeFunction VehicleValueSorter;

static VehicleSortListingTypeFunction* const _vehicle_sorter[] = {
	&VehicleNumberSorter,
	&VehicleNameSorter,
	&VehicleAgeSorter,
	&VehicleProfitThisYearSorter,
	&VehicleProfitLastYearSorter,
	&VehicleCargoSorter,
	&VehicleReliabilitySorter,
	&VehicleMaxSpeedSorter,
	&VehicleModelSorter,
	&VehicleValueSorter,
};

const StringID _vehicle_sort_listing[] = {
	STR_SORT_BY_NUMBER,
	STR_SORT_BY_DROPDOWN_NAME,
	STR_SORT_BY_AGE,
	STR_SORT_BY_PROFIT_THIS_YEAR,
	STR_SORT_BY_PROFIT_LAST_YEAR,
	STR_SORT_BY_TOTAL_CAPACITY_PER_CARGOTYPE,
	STR_SORT_BY_RELIABILITY,
	STR_SORT_BY_MAX_SPEED,
	STR_SORT_BY_MODEL,
	STR_SORT_BY_VALUE,
	INVALID_STRING_ID
};

void RebuildVehicleLists()
{
	Window* const *wz;

	FOR_ALL_WINDOWS(wz) {
		Window *w = *wz;

		switch (w->window_class) {
			case WC_TRAINS_LIST:
			case WC_ROADVEH_LIST:
			case WC_SHIPS_LIST:
			case WC_AIRCRAFT_LIST:
				WP(w, vehiclelist_d).l.flags |= VL_REBUILD;
				SetWindowDirty(w);
				break;

			default: break;
		}
	}
}

void ResortVehicleLists()
{
	Window* const *wz;

	FOR_ALL_WINDOWS(wz) {
		Window *w = *wz;

		switch (w->window_class) {
			case WC_TRAINS_LIST:
			case WC_ROADVEH_LIST:
			case WC_SHIPS_LIST:
			case WC_AIRCRAFT_LIST:
				WP(w, vehiclelist_d).l.flags |= VL_RESORT;
				SetWindowDirty(w);
				break;

			default: break;
		}
	}
}

void BuildVehicleList(vehiclelist_d *vl, PlayerID owner, uint16 index, uint16 window_type)
{
	if (!(vl->l.flags & VL_REBUILD)) return;

	DEBUG(misc, 3, "Building vehicle list for player %d at station %d", owner, index);

	vl->l.list_length = GenerateVehicleSortList(&vl->sort_list, &vl->length_of_sort_list, vl->vehicle_type, owner, index, window_type);

	vl->l.flags &= ~VL_REBUILD;
	vl->l.flags |= VL_RESORT;
}

void SortVehicleList(vehiclelist_d *vl)
{
	if (!(vl->l.flags & VL_RESORT)) return;

	_internal_sort_order = (vl->l.flags & VL_DESC) != 0;
	qsort((void*)vl->sort_list, vl->l.list_length, sizeof(vl->sort_list[0]),
		_vehicle_sorter[vl->l.sort_type]);

	vl->l.resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
	vl->l.flags &= ~VL_RESORT;
}

void DepotSortList(Vehicle **v, uint16 length)
{
	_internal_sort_order = 0;
	qsort((void*)v, length, sizeof(v[0]), _vehicle_sorter[0]);
}

/** draw the vehicle profit button in the vehicle list window. */
void DrawVehicleProfitButton(const Vehicle *v, int x, int y)
{
	SpriteID pal;

	/* draw profit-based colored icons */
	if (v->age <= 365 * 2) {
		pal = PALETTE_TO_GREY;
	} else if (v->profit_last_year < 0) {
		pal = PALETTE_TO_RED;
	} else if (v->profit_last_year < 10000) {
		pal = PALETTE_TO_YELLOW;
	} else {
		pal = PALETTE_TO_GREEN;
	}
	DrawSprite(SPR_BLOT, pal, x, y);
}

struct RefitOption {
	CargoID cargo;
	byte subtype;
	uint16 value;
	EngineID engine;
};

struct RefitList {
	uint num_lines;
	RefitOption *items;
};

static RefitList *BuildRefitList(const Vehicle *v)
{
	uint max_lines = 256;
	RefitOption *refit = CallocT<RefitOption>(max_lines);
	RefitList *list = CallocT<RefitList>(1);
	Vehicle *u = (Vehicle*)v;
	uint num_lines = 0;
	uint i;

	do {
		uint32 cmask = EngInfo(u->engine_type)->refit_mask;
		byte callbackmask = EngInfo(u->engine_type)->callbackmask;

		/* Skip this engine if it has no capacity */
		if (u->cargo_cap == 0) continue;

		/* Loop through all cargos in the refit mask */
		for (CargoID cid = 0; cid < NUM_CARGO && num_lines < max_lines; cid++) {
			/* Skip cargo type if it's not listed */
			if (!HasBit(cmask, cid)) continue;

			/* Check the vehicle's callback mask for cargo suffixes */
			if (HasBit(callbackmask, CBM_VEHICLE_CARGO_SUFFIX)) {
				/* Make a note of the original cargo type. It has to be
				 * changed to test the cargo & subtype... */
				CargoID temp_cargo = u->cargo_type;
				byte temp_subtype  = u->cargo_subtype;
				byte refit_cyc;

				u->cargo_type = cid;

				for (refit_cyc = 0; refit_cyc < 16 && num_lines < max_lines; refit_cyc++) {
					bool duplicate = false;
					uint16 callback;

					u->cargo_subtype = refit_cyc;
					callback = GetVehicleCallback(CBID_VEHICLE_CARGO_SUFFIX, 0, 0, u->engine_type, u);

					if (callback == 0xFF) callback = CALLBACK_FAILED;
					if (refit_cyc != 0 && callback == CALLBACK_FAILED) break;

					/* Check if this cargo and subtype combination are listed */
					for (i = 0; i < num_lines && !duplicate; i++) {
						if (refit[i].cargo == cid && refit[i].value == callback) duplicate = true;
					}

					if (duplicate) continue;

					refit[num_lines].cargo   = cid;
					refit[num_lines].subtype = refit_cyc;
					refit[num_lines].value   = callback;
					refit[num_lines].engine  = u->engine_type;
					num_lines++;
				}

				/* Reset the vehicle's cargo type */
				u->cargo_type    = temp_cargo;
				u->cargo_subtype = temp_subtype;
			} else {
				/* No cargo suffix callback -- use no subtype */
				bool duplicate = false;

				for (i = 0; i < num_lines && !duplicate; i++) {
					if (refit[i].cargo == cid && refit[i].value == CALLBACK_FAILED) duplicate = true;
				}

				if (!duplicate) {
					refit[num_lines].cargo   = cid;
					refit[num_lines].subtype = 0;
					refit[num_lines].value   = CALLBACK_FAILED;
					refit[num_lines].engine  = INVALID_ENGINE;
					num_lines++;
				}
			}
		}
	} while (v->type == VEH_TRAIN && (u = u->Next()) != NULL && num_lines < max_lines);

	list->num_lines = num_lines;
	list->items = refit;

	return list;
}

/** Draw the list of available refit options for a consist.
 * Draw the list and highlight the selected refit option (if any)
 * @param *list first vehicle in consist to get the refit-options of
 * @param sel selected refit cargo-type in the window
 * @param pos position of the selected item in caller widow
 * @param rows number of rows(capacity) in caller window
 * @param delta step height in caller window
 * @return the refit option that is hightlighted, NULL if none
 */
static RefitOption *DrawVehicleRefitWindow(const RefitList *list, int sel, uint pos, uint rows, uint delta)
{
	RefitOption *refit = list->items;
	RefitOption *selected = NULL;
	uint num_lines = list->num_lines;
	uint y = 31;
	uint i;

	/* Draw the list, and find the selected cargo (by its position in list) */
	for (i = 0; i < num_lines; i++) {
		TextColour colour = TC_BLACK;
		if (sel == 0) {
			selected = &refit[i];
			colour = TC_WHITE;
		}

		if (i >= pos && i < pos + rows) {
			/* Draw the cargo name */
			int last_x = DrawString(2, y, GetCargo(refit[i].cargo)->name, colour);

			/* If the callback succeeded, draw the cargo suffix */
			if (refit[i].value != CALLBACK_FAILED) {
				DrawString(last_x + 1, y, GetGRFStringID(GetEngineGRFID(refit[i].engine), 0xD000 + refit[i].value), colour);
			}
			y += delta;
		}

		sel--;
	}

	return selected;
}

static void VehicleRefitWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			Vehicle *v = GetVehicle(w->window_number);

			if (v->type == VEH_TRAIN) {
				uint length = CountVehiclesInChain(v);

				if (length != WP(w, refit_d).length) {
					/* Consist length has changed, so rebuild the refit list */
					free(WP(w, refit_d).list->items);
					free(WP(w, refit_d).list);
					WP(w, refit_d).list = BuildRefitList(v);
					WP(w, refit_d).length = length;
				}
			}

			SetVScrollCount(w, WP(w, refit_d).list->num_lines);

			SetDParam(0, v->index);
			DrawWindowWidgets(w);

			WP(w, refit_d).cargo = DrawVehicleRefitWindow(WP(w, refit_d).list, WP(w, refit_d).sel, w->vscroll.pos, w->vscroll.cap, w->resize.step_height);

			if (WP(w, refit_d).cargo != NULL) {
				CommandCost cost;

				cost = DoCommand(v->tile, v->index, WP(w, refit_d).cargo->cargo | WP(w, refit_d).cargo->subtype << 8,
								 DC_QUERY_COST, GetCmdRefitVeh(GetVehicle(w->window_number)->type));

				if (CmdSucceeded(cost)) {
					SetDParam(0, WP(w, refit_d).cargo->cargo);
					SetDParam(1, _returned_refit_capacity);
					SetDParam(2, cost.GetCost());
					DrawString(2, w->widget[5].top + 1, STR_9840_NEW_CAPACITY_COST_OF_REFIT, TC_FROMSTRING);
				}
			}
		} break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case 3: { // listbox
					int y = e->we.click.pt.y - w->widget[3].top;
					if (y >= 0) {
						WP(w, refit_d).sel = (y / (int)w->resize.step_height) + w->vscroll.pos;
						SetWindowDirty(w);
					}
				} break;
				case 6: // refit button
					if (WP(w, refit_d).cargo != NULL) {
						const Vehicle *v = GetVehicle(w->window_number);

						if (WP(w, refit_d).order == INVALID_VEH_ORDER_ID) {
							int command = 0;

							switch (v->type) {
								default: NOT_REACHED();
								case VEH_TRAIN:    command = CMD_REFIT_RAIL_VEHICLE | CMD_MSG(STR_RAIL_CAN_T_REFIT_VEHICLE);  break;
								case VEH_ROAD:     command = CMD_REFIT_ROAD_VEH     | CMD_MSG(STR_REFIT_ROAD_VEHICLE_CAN_T);  break;
								case VEH_SHIP:     command = CMD_REFIT_SHIP         | CMD_MSG(STR_9841_CAN_T_REFIT_SHIP);     break;
								case VEH_AIRCRAFT: command = CMD_REFIT_AIRCRAFT     | CMD_MSG(STR_A042_CAN_T_REFIT_AIRCRAFT); break;
							}
							if (DoCommandP(v->tile, v->index, WP(w, refit_d).cargo->cargo | WP(w, refit_d).cargo->subtype << 8, NULL, command)) DeleteWindow(w);
						} else {
							if (DoCommandP(v->tile, v->index, WP(w, refit_d).cargo->cargo | WP(w, refit_d).cargo->subtype << 8 | WP(w, refit_d).order << 16, NULL, CMD_ORDER_REFIT)) DeleteWindow(w);
						}
					}
					break;
			}
			break;

		case WE_RESIZE:
			w->vscroll.cap += e->we.sizing.diff.y / (int)w->resize.step_height;
			w->widget[3].data = (w->vscroll.cap << 8) + 1;
			break;

		case WE_DESTROY:
			free(WP(w, refit_d).list->items);
			free(WP(w, refit_d).list);
			break;
	}
}


static const Widget _vehicle_refit_widgets[] = {
	{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                            STR_018B_CLOSE_WINDOW},
	{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   239,     0,    13, STR_983B_REFIT,                      STR_018C_WINDOW_TITLE_DRAG_THIS},
	{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,   239,    14,    27, STR_983F_SELECT_CARGO_TYPE_TO_CARRY, STR_983D_SELECT_TYPE_OF_CARGO_FOR},
	{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   227,    28,   139, 0x801,                               STR_EMPTY},
	{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   228,   239,    28,   139, 0x0,                                 STR_0190_SCROLL_BAR_SCROLLS_LIST},
	{      WWT_PANEL,     RESIZE_TB,    14,     0,   239,   140,   161, 0x0,                                 STR_NULL},
	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   227,   162,   173, 0x0,                                 STR_NULL},
	{  WWT_RESIZEBOX,     RESIZE_TB,    14,   228,   239,   162,   173, 0x0,                                 STR_RESIZE_BUTTON},
	{   WIDGETS_END},
};

static const WindowDesc _vehicle_refit_desc = {
	WDP_AUTO, WDP_AUTO, 240, 174, 240, 174,
	WC_VEHICLE_REFIT, WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_vehicle_refit_widgets,
	VehicleRefitWndProc,
};

/** Show the refit window for a vehicle
* @param *v The vehicle to show the refit window for
* @param order of the vehicle ( ? )
*/
void ShowVehicleRefitWindow(const Vehicle *v, VehicleOrderID order)
{
	Window *w;

	DeleteWindowById(WC_VEHICLE_REFIT, v->index);

	w = AllocateWindowDescFront(&_vehicle_refit_desc, v->index);
	WP(w, refit_d).order = order;

	if (w != NULL) {
		w->caption_color = v->owner;
		w->vscroll.cap = 8;
		w->resize.step_height = 14;
		WP(w, refit_d).sel  = -1;
		WP(w, refit_d).list = BuildRefitList(v);
		if (v->type == VEH_TRAIN) WP(w, refit_d).length = CountVehiclesInChain(v);
		SetVScrollCount(w, WP(w, refit_d).list->num_lines);

		switch (v->type) {
			case VEH_TRAIN:
				w->widget[3].tooltips = STR_RAIL_SELECT_TYPE_OF_CARGO_FOR;
				w->widget[6].data     = STR_RAIL_REFIT_VEHICLE;
				w->widget[6].tooltips = STR_RAIL_REFIT_TO_CARRY_HIGHLIGHTED;
				break;
			case VEH_ROAD:
				w->widget[3].tooltips = STR_ROAD_SELECT_TYPE_OF_CARGO_FOR;
				w->widget[6].data     = STR_REFIT_ROAD_VEHICLE;
				w->widget[6].tooltips = STR_REFIT_ROAD_VEHICLE_TO_CARRY_HIGHLIGHTED;
				break;
			case VEH_SHIP:
				w->widget[3].tooltips = STR_983D_SELECT_TYPE_OF_CARGO_FOR;
				w->widget[6].data     = STR_983C_REFIT_SHIP;
				w->widget[6].tooltips = STR_983E_REFIT_SHIP_TO_CARRY_HIGHLIGHTED;
				break;
			case VEH_AIRCRAFT:
				w->widget[3].tooltips = STR_A03E_SELECT_TYPE_OF_CARGO_FOR;
				w->widget[6].data     = STR_A03D_REFIT_AIRCRAFT;
				w->widget[6].tooltips = STR_A03F_REFIT_AIRCRAFT_TO_CARRY;
				break;
			default: NOT_REACHED();
		}
	}
}

/** Display additional text from NewGRF in the purchase information window */
uint ShowAdditionalText(int x, int y, uint w, EngineID engine)
{
	uint16 callback = GetVehicleCallback(CBID_VEHICLE_ADDITIONAL_TEXT, 0, 0, engine, NULL);
	if (callback == CALLBACK_FAILED) return 0;

	/* STR_02BD is used to start the string with {BLACK} */
	SetDParam(0, GetGRFStringID(GetEngineGRFID(engine), 0xD000 + callback));
	return DrawStringMultiLine(x, y, STR_02BD, w);
}

/** Display list of cargo types of the engine, for the purchase information window */
uint ShowRefitOptionsList(int x, int y, uint w, EngineID engine)
{
	/* List of cargo types of this engine */
	uint32 cmask = EngInfo(engine)->refit_mask;
	/* List of cargo types available in this climate */
	uint32 lmask = _cargo_mask;
	char *b = _userstring;

	/* Draw nothing if the engine is not refittable */
	if (CountBits(cmask) <= 1) return 0;

	b = InlineString(b, STR_PURCHASE_INFO_REFITTABLE_TO);

	if (cmask == lmask) {
		/* Engine can be refitted to all types in this climate */
		b = InlineString(b, STR_PURCHASE_INFO_ALL_TYPES);
	} else {
		/* Check if we are able to refit to more cargo types and unable to. If
		 * so, invert the cargo types to list those that we can't refit to. */
		if (CountBits(cmask ^ lmask) < CountBits(cmask)) {
			cmask ^= lmask;
			b = InlineString(b, STR_PURCHASE_INFO_ALL_BUT);
		}

		bool first = true;

		/* Add each cargo type to the list */
		for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
			if (!HasBit(cmask, cid)) continue;

			if (b >= lastof(_userstring) - (2 + 2 * 4)) break; // ", " and two calls to Utf8Encode()

			if (!first) b = strecpy(b, ", ", lastof(_userstring));
			first = false;

			b = InlineString(b, GetCargo(cid)->name);
		}
	}

	/* Terminate and display the completed string */
	*b = '\0';

	/* Make sure we detect any buffer overflow */
	assert(b < endof(_userstring));

	return DrawStringMultiLine(x, y, STR_SPEC_USERSTRING, w);
}


/* if the sorting criteria had the same value, sort vehicle by unitnumber */
#define VEHICLEUNITNUMBERSORTER(r, a, b) {if (r == 0) {r = a->unitnumber - b->unitnumber;}}

static int CDECL VehicleNumberSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	int r = va->unitnumber - vb->unitnumber;

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleNameSorter(const void *a, const void *b)
{
	static const Vehicle *last_vehicle[2] = { NULL, NULL };
	static char           last_name[2][64] = { "", "" };

	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	int r;

	if (va != last_vehicle[0]) {
		last_vehicle[0] = va;
		if (IsCustomName(va->string_id)) {
			GetString(last_name[0], va->string_id, lastof(last_name[0]));
		} else {
			last_name[0][0] = '\0';
		}
	}

	if (vb != last_vehicle[1]) {
		last_vehicle[1] = vb;
		if (IsCustomName(vb->string_id)) {
			GetString(last_name[1], vb->string_id, lastof(last_name[1]));
		} else {
			last_name[1][0] = '\0';
		}
	}

	r = strcmp(last_name[0], last_name[1]); // sort by name

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleAgeSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	int r = va->age - vb->age;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleProfitThisYearSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	int r = ClampToI32(va->profit_this_year - vb->profit_this_year);

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleProfitLastYearSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	int r = ClampToI32(va->profit_last_year - vb->profit_last_year);

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleCargoSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	const Vehicle* v;
	AcceptedCargo cargoa;
	AcceptedCargo cargob;
	int r = 0;

	memset(cargoa, 0, sizeof(cargoa));
	memset(cargob, 0, sizeof(cargob));
	for (v = va; v != NULL; v = v->Next()) cargoa[v->cargo_type] += v->cargo_cap;
	for (v = vb; v != NULL; v = v->Next()) cargob[v->cargo_type] += v->cargo_cap;

	for (CargoID i = 0; i < NUM_CARGO; i++) {
		r = cargoa[i] - cargob[i];
		if (r != 0) break;
	}

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleReliabilitySorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	int r = va->reliability - vb->reliability;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleMaxSpeedSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	int max_speed_a = 0xFFFF, max_speed_b = 0xFFFF;
	int r;
	const Vehicle *ua = va, *ub = vb;

	if (va->type == VEH_TRAIN && vb->type == VEH_TRAIN) {
		do {
			if (RailVehInfo(ua->engine_type)->max_speed != 0)
				max_speed_a = min(max_speed_a, RailVehInfo(ua->engine_type)->max_speed);
		} while ((ua = ua->Next()) != NULL);

		do {
			if (RailVehInfo(ub->engine_type)->max_speed != 0)
				max_speed_b = min(max_speed_b, RailVehInfo(ub->engine_type)->max_speed);
		} while ((ub = ub->Next()) != NULL);

		r = max_speed_a - max_speed_b;
	} else {
		r = va->max_speed - vb->max_speed;
	}

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleModelSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	int r = va->engine_type - vb->engine_type;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleValueSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	const Vehicle *u;
	Money valuea = 0, valueb = 0;

	for (u = va; u != NULL; u = u->Next()) valuea += u->value;
	for (u = vb; u != NULL; u = u->Next()) valueb += u->value;

	int r = ClampToI32(valuea - valueb);

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

void InitializeGUI()
{
	memset(&_sorting, 0, sizeof(_sorting));
}

/** Assigns an already open vehicle window to a new vehicle.
 * Assigns an already open vehicle window to a new vehicle. If the vehicle got
 * any sub window open (orders and so on) it will change owner too.
 * @param *from_v the current owner of the window
 * @param *to_v the new owner of the window
 */
void ChangeVehicleViewWindow(const Vehicle *from_v, const Vehicle *to_v)
{
	Window *w;

	w = FindWindowById(WC_VEHICLE_VIEW, from_v->index);
	if (w != NULL) {
		w->window_number = to_v->index;
		WP(w, vp_d).follow_vehicle = to_v->index;
		SetWindowDirty(w);

		w = FindWindowById(WC_VEHICLE_ORDERS, from_v->index);
		if (w != NULL) {
			w->window_number = to_v->index;
			SetWindowDirty(w);
		}

		w = FindWindowById(WC_VEHICLE_REFIT, from_v->index);
		if (w != NULL) {
			w->window_number = to_v->index;
			SetWindowDirty(w);
		}

		w = FindWindowById(WC_VEHICLE_DETAILS, from_v->index);
		if (w != NULL) {
			w->window_number = to_v->index;
			SetWindowDirty(w);
		}
	}
}

enum VehicleListWindowWidgets {
	VLW_WIDGET_CLOSEBOX = 0,
	VLW_WIDGET_CAPTION,
	VLW_WIDGET_STICKY,
	VLW_WIDGET_SORT_ORDER,
	VLW_WIDGET_SORT_BY_TEXT,
	VLW_WIDGET_SORT_BY_PULLDOWN,
	VLW_WIDGET_EMPTY_TOP_RIGHT,
	VLW_WIDGET_LIST,
	VLW_WIDGET_SCROLLBAR,
	VLW_WIDGET_OTHER_PLAYER_FILLER,
	VLW_WIDGET_AVAILABLE_VEHICLES,
	VLW_WIDGET_MANAGE_VEHICLES,
	VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN,
	VLW_WIDGET_STOP_ALL,
	VLW_WIDGET_START_ALL,
	VLW_WIDGET_EMPTY_BOTTOM_RIGHT,
	VLW_WIDGET_RESIZE,
};

static const Widget _vehicle_list_widgets[] = {
	{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,             STR_018B_CLOSE_WINDOW},
	{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   247,     0,    13, 0x0,                  STR_018C_WINDOW_TITLE_DRAG_THIS},
	{  WWT_STICKYBOX,     RESIZE_LR,    14,   248,   259,     0,    13, 0x0,                  STR_STICKY_BUTTON},
	{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    80,    14,    25, STR_SORT_BY,          STR_SORT_ORDER_TIP},
	{      WWT_PANEL,   RESIZE_NONE,    14,    81,   235,    14,    25, 0x0,                  STR_SORT_CRITERIA_TIP},
	{    WWT_TEXTBTN,   RESIZE_NONE,    14,   236,   247,    14,    25, STR_0225,             STR_SORT_CRITERIA_TIP},
	{      WWT_PANEL,  RESIZE_RIGHT,    14,   248,   259,    14,    25, 0x0,                  STR_NULL},
	{     WWT_MATRIX,     RESIZE_RB,    14,     0,   247,    26,   169, 0x0,                  STR_NULL},
	{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   248,   259,    26,   169, 0x0,                  STR_0190_SCROLL_BAR_SCROLLS_LIST},
	/* Widget to be shown for other players hiding the following 6 widgets */
	{      WWT_PANEL,    RESIZE_RTB,    14,     0,   247,   170,   181, 0x0,                  STR_NULL},

	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   105,   170,   181, 0x0,                  STR_AVAILABLE_ENGINES_TIP},
	{    WWT_TEXTBTN,     RESIZE_TB,    14,   106,   211,   170,   181, STR_MANAGE_LIST,      STR_MANAGE_LIST_TIP},
	{    WWT_TEXTBTN,     RESIZE_TB,    14,   212,   223,   170,   181, STR_0225,             STR_MANAGE_LIST_TIP},

	{ WWT_PUSHIMGBTN,     RESIZE_TB,    14,   224,   235,   170,   181, SPR_FLAG_VEH_STOPPED, STR_MASS_STOP_LIST_TIP},
	{ WWT_PUSHIMGBTN,     RESIZE_TB,    14,   236,   247,   170,   181, SPR_FLAG_VEH_RUNNING, STR_MASS_START_LIST_TIP},
	{      WWT_PANEL,    RESIZE_RTB,    14,   248,   247,   170,   181, 0x0,                  STR_NULL},
	{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   248,   259,   170,   181, 0x0,                  STR_RESIZE_BUTTON},
	{   WIDGETS_END},
};

static void CreateVehicleListWindow(Window *w)
{
	vehiclelist_d *vl = &WP(w, vehiclelist_d);
	uint16 window_type = w->window_number & VLW_MASK;
	PlayerID player = (PlayerID)GB(w->window_number, 0, 8);

	vl->vehicle_type = (VehicleType)GB(w->window_number, 11, 5);
	vl->length_of_sort_list = 0;
	vl->sort_list = NULL;
	w->caption_color = player;

	/* Hide the widgets that we will not use in this window
	 * Some windows contains actions only fit for the owner */
	if (player == _local_player) {
		w->HideWidget(VLW_WIDGET_OTHER_PLAYER_FILLER);
		w->SetWidgetDisabledState(VLW_WIDGET_AVAILABLE_VEHICLES, window_type != VLW_STANDARD);
	} else {
		w->SetWidgetsHiddenState(true,
			VLW_WIDGET_AVAILABLE_VEHICLES,
			VLW_WIDGET_MANAGE_VEHICLES,
			VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN,
			VLW_WIDGET_STOP_ALL,
			VLW_WIDGET_START_ALL,
			VLW_WIDGET_EMPTY_BOTTOM_RIGHT,
			WIDGET_LIST_END);
	}

	/* Set up the window widgets */
	switch (vl->vehicle_type) {
		case VEH_TRAIN:
			w->widget[VLW_WIDGET_LIST].tooltips          = STR_883D_TRAINS_CLICK_ON_TRAIN_FOR;
			w->widget[VLW_WIDGET_AVAILABLE_VEHICLES].data = STR_AVAILABLE_TRAINS;
			break;

		case VEH_ROAD:
			w->widget[VLW_WIDGET_LIST].tooltips          = STR_901A_ROAD_VEHICLES_CLICK_ON;
			w->widget[VLW_WIDGET_AVAILABLE_VEHICLES].data = STR_AVAILABLE_ROAD_VEHICLES;
			break;

		case VEH_SHIP:
			w->widget[VLW_WIDGET_LIST].tooltips          = STR_9823_SHIPS_CLICK_ON_SHIP_FOR;
			w->widget[VLW_WIDGET_AVAILABLE_VEHICLES].data = STR_AVAILABLE_SHIPS;
			break;

		case VEH_AIRCRAFT:
			w->widget[VLW_WIDGET_LIST].tooltips          = STR_A01F_AIRCRAFT_CLICK_ON_AIRCRAFT;
			w->widget[VLW_WIDGET_AVAILABLE_VEHICLES].data = STR_AVAILABLE_AIRCRAFT;
			break;

		default: NOT_REACHED();
	}

	switch (window_type) {
		case VLW_SHARED_ORDERS:
			w->widget[VLW_WIDGET_CAPTION].data  = STR_VEH_WITH_SHARED_ORDERS_LIST;
			break;
		case VLW_STANDARD: /* Company Name - standard widget setup */
			switch (vl->vehicle_type) {
				case VEH_TRAIN:    w->widget[VLW_WIDGET_CAPTION].data = STR_881B_TRAINS;        break;
				case VEH_ROAD:     w->widget[VLW_WIDGET_CAPTION].data = STR_9001_ROAD_VEHICLES; break;
				case VEH_SHIP:     w->widget[VLW_WIDGET_CAPTION].data = STR_9805_SHIPS;         break;
				case VEH_AIRCRAFT: w->widget[VLW_WIDGET_CAPTION].data = STR_A009_AIRCRAFT;      break;
				default: NOT_REACHED(); break;
			}
			break;
		case VLW_STATION_LIST: /* Station Name */
			switch (vl->vehicle_type) {
				case VEH_TRAIN:    w->widget[VLW_WIDGET_CAPTION].data = STR_SCHEDULED_TRAINS;        break;
				case VEH_ROAD:     w->widget[VLW_WIDGET_CAPTION].data = STR_SCHEDULED_ROAD_VEHICLES; break;
				case VEH_SHIP:     w->widget[VLW_WIDGET_CAPTION].data = STR_SCHEDULED_SHIPS;         break;
				case VEH_AIRCRAFT: w->widget[VLW_WIDGET_CAPTION].data = STR_SCHEDULED_AIRCRAFT;      break;
				default: NOT_REACHED(); break;
			}
			break;

		case VLW_DEPOT_LIST:
			switch (vl->vehicle_type) {
				case VEH_TRAIN:    w->widget[VLW_WIDGET_CAPTION].data = STR_VEHICLE_LIST_TRAIN_DEPOT;    break;
				case VEH_ROAD:     w->widget[VLW_WIDGET_CAPTION].data = STR_VEHICLE_LIST_ROADVEH_DEPOT;  break;
				case VEH_SHIP:     w->widget[VLW_WIDGET_CAPTION].data = STR_VEHICLE_LIST_SHIP_DEPOT;     break;
				case VEH_AIRCRAFT: w->widget[VLW_WIDGET_CAPTION].data = STR_VEHICLE_LIST_AIRCRAFT_DEPOT; break;
				default: NOT_REACHED(); break;
			}
			break;
		default: NOT_REACHED(); break;
	}

	switch (vl->vehicle_type) {
		case VEH_TRAIN:
			w->resize.step_width = 1;
			/* Fallthrough */
		case VEH_ROAD:
			w->vscroll.cap = 7;
			w->resize.step_height = PLY_WND_PRC__SIZE_OF_ROW_SMALL;
			w->resize.height = 220 - (PLY_WND_PRC__SIZE_OF_ROW_SMALL * 3); // Minimum of 4 vehicles
			break;
		case VEH_SHIP:
		case VEH_AIRCRAFT:
			w->vscroll.cap = 4;
			w->resize.step_height = PLY_WND_PRC__SIZE_OF_ROW_BIG;
			break;
		default: NOT_REACHED();
	}

	w->widget[VLW_WIDGET_LIST].data = (w->vscroll.cap << 8) + 1;

	/* Set up sorting. Make the window-specific _sorting variable
		* point to the correct global _sorting struct so we are freed
		* from having conditionals during window operation */
	switch (vl->vehicle_type) {
		case VEH_TRAIN:    vl->_sorting = &_sorting.train; break;
		case VEH_ROAD:     vl->_sorting = &_sorting.roadveh; break;
		case VEH_SHIP:     vl->_sorting = &_sorting.ship; break;
		case VEH_AIRCRAFT: vl->_sorting = &_sorting.aircraft; break;
		default: NOT_REACHED(); break;
	}

	vl->l.flags = VL_REBUILD | (vl->_sorting->order ? VL_DESC : VL_NONE);
	vl->l.sort_type = vl->_sorting->criteria;
	vl->sort_list = NULL;
	vl->l.resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS; // Set up resort timer
}

void DrawSmallOrderList(const Vehicle *v, int x, int y)
{
	const Order *order;
	int sel, i = 0;

	sel = v->cur_order_index;

	FOR_VEHICLE_ORDERS(v, order) {
		if (sel == 0) DrawString(x - 6, y, STR_SMALL_RIGHT_ARROW, TC_BLACK);
		sel--;

		if (order->type == OT_GOTO_STATION) {
			if (v->type == VEH_SHIP && GetStation(order->dest)->IsBuoy()) continue;

			SetDParam(0, order->dest);
			DrawString(x, y, STR_A036, TC_FROMSTRING);

			y += 6;
			if (++i == 4) break;
		}
	}
}

static void DrawVehicleListWindow(Window *w)
{
	vehiclelist_d *vl = &WP(w, vehiclelist_d);
	int x = 2;
	int y = PLY_WND_PRC__OFFSET_TOP_WIDGET;
	int max;
	int i;
	const PlayerID owner = (PlayerID)w->caption_color;
	const Player *p = GetPlayer(owner);
	const uint16 window_type = w->window_number & VLW_MASK;
	const uint16 index = GB(w->window_number, 16, 16);

	BuildVehicleList(vl, owner, index, window_type);
	SortVehicleList(vl);
	SetVScrollCount(w, vl->l.list_length);

	/* draw the widgets */
	switch (window_type) {
		case VLW_SHARED_ORDERS: /* Shared Orders */
			if (vl->l.list_length == 0) {
				/* We can't open this window without vehicles using this order
				 * and we should close the window when deleting the order      */
				NOT_REACHED();
			}
			SetDParam(0, w->vscroll.count);
			break;

		case VLW_STANDARD: /* Company Name */
			SetDParam(0, p->index);
			SetDParam(1, w->vscroll.count);
			break;

		case VLW_STATION_LIST: /* Station Name */
			SetDParam(0, index);
			SetDParam(1, w->vscroll.count);
			break;

		case VLW_DEPOT_LIST:
			switch (vl->vehicle_type) {
				case VEH_TRAIN:    SetDParam(0, STR_8800_TRAIN_DEPOT);        break;
				case VEH_ROAD:     SetDParam(0, STR_9003_ROAD_VEHICLE_DEPOT); break;
				case VEH_SHIP:     SetDParam(0, STR_9803_SHIP_DEPOT);         break;
				case VEH_AIRCRAFT: SetDParam(0, STR_A002_AIRCRAFT_HANGAR);    break;
				default: NOT_REACHED(); break;
			}
			if (vl->vehicle_type == VEH_AIRCRAFT) {
				SetDParam(1, index); // Airport name
			} else {
				SetDParam(1, GetDepot(index)->town_index);
			}
			SetDParam(2, w->vscroll.count);
			break;
		default: NOT_REACHED(); break;
	}

	w->SetWidgetsDisabledState(vl->l.list_length == 0,
		VLW_WIDGET_MANAGE_VEHICLES,
		VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN,
		VLW_WIDGET_STOP_ALL,
		VLW_WIDGET_START_ALL,
		WIDGET_LIST_END);

	DrawWindowWidgets(w);

	/* draw sorting criteria string */
	DrawString(85, 15, _vehicle_sort_listing[vl->l.sort_type], TC_BLACK);
	/* draw arrow pointing up/down for ascending/descending sorting */
	DoDrawString(vl->l.flags & VL_DESC ? DOWNARROW : UPARROW, 69, 15, TC_BLACK);

	max = min(w->vscroll.pos + w->vscroll.cap, vl->l.list_length);
	for (i = w->vscroll.pos; i < max; ++i) {
		const Vehicle *v = vl->sort_list[i];
		StringID str;

		SetDParam(0, v->profit_this_year);
		SetDParam(1, v->profit_last_year);

		DrawVehicleImage(v, x + 19, y + 6, INVALID_VEHICLE, w->widget[VLW_WIDGET_LIST].right - w->widget[VLW_WIDGET_LIST].left - 20, 0);
		DrawString(x + 19, y + w->resize.step_height - 8, STR_0198_PROFIT_THIS_YEAR_LAST_YEAR, TC_FROMSTRING);

		if ((v->type == VEH_TRAIN    && v->string_id != STR_SV_TRAIN_NAME)   ||
			(v->type == VEH_ROAD     && v->string_id != STR_SV_ROADVEH_NAME) ||
			(v->type == VEH_SHIP     && v->string_id != STR_SV_SHIP_NAME)    ||
			(v->type == VEH_AIRCRAFT && v->string_id != STR_SV_AIRCRAFT_NAME)) {

			/* The vehicle got a name so we will print it */
			SetDParam(0, v->index);
			DrawString(x + 19, y, STR_01AB, TC_FROMSTRING);
		}

		if (w->resize.step_height == PLY_WND_PRC__SIZE_OF_ROW_BIG) DrawSmallOrderList(v, x + 138, y);

		if (v->IsInDepot()) {
			str = STR_021F;
		} else {
			str = (v->age > v->max_age - 366) ? STR_00E3 : STR_00E2;
		}

		SetDParam(0, v->unitnumber);
		DrawString(x, y + 2, str, TC_FROMSTRING);

		DrawVehicleProfitButton(v, x, y + 13);

		y += w->resize.step_height;
	}
}

/*
 * bitmask for w->window_number
 * 0-7 PlayerID (owner)
 * 8-10 window type (use flags in vehicle_gui.h)
 * 11-15 vehicle type (using VEH_, but can be compressed to fewer bytes if needed)
 * 16-31 StationID or OrderID depending on window type (bit 8-10)
 **/
void PlayerVehWndProc(Window *w, WindowEvent *e)
{
	vehiclelist_d *vl = &WP(w, vehiclelist_d);

	switch (e->event) {
		case WE_CREATE:
			CreateVehicleListWindow(w);
			break;

		case WE_PAINT:
			DrawVehicleListWindow(w);
			break;

		case WE_CLICK: {
			switch (e->we.click.widget) {
				case VLW_WIDGET_SORT_ORDER: /* Flip sorting method ascending/descending */
					vl->l.flags ^= VL_DESC;
					vl->l.flags |= VL_RESORT;

					vl->_sorting->order = !!(vl->l.flags & VL_DESC);
					SetWindowDirty(w);
					break;
				case VLW_WIDGET_SORT_BY_TEXT: case VLW_WIDGET_SORT_BY_PULLDOWN:/* Select sorting criteria dropdown menu */
					ShowDropDownMenu(w, _vehicle_sort_listing, vl->l.sort_type, VLW_WIDGET_SORT_BY_PULLDOWN, 0, 0);
					return;
				case VLW_WIDGET_LIST: { /* Matrix to show vehicles */
					uint32 id_v = (e->we.click.pt.y - PLY_WND_PRC__OFFSET_TOP_WIDGET) / w->resize.step_height;
					const Vehicle *v;

					if (id_v >= w->vscroll.cap) return; // click out of bounds

					id_v += w->vscroll.pos;

					if (id_v >= vl->l.list_length) return; // click out of list bound

					v = vl->sort_list[id_v];

					ShowVehicleViewWindow(v);
				} break;

				case VLW_WIDGET_AVAILABLE_VEHICLES:
					ShowBuildVehicleWindow(0, vl->vehicle_type);
					break;

				case VLW_WIDGET_MANAGE_VEHICLES:
				case VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN: {
					static StringID action_str[] = {
						STR_REPLACE_VEHICLES,
						STR_SEND_FOR_SERVICING,
						STR_NULL,
						INVALID_STRING_ID
					};

					static const StringID depot_name[] = {
						STR_SEND_TRAIN_TO_DEPOT,
						STR_SEND_ROAD_VEHICLE_TO_DEPOT,
						STR_SEND_SHIP_TO_DEPOT,
						STR_SEND_AIRCRAFT_TO_HANGAR
					};

					/* XXX - Substite string since the dropdown cannot handle dynamic strings */
					action_str[2] = depot_name[vl->vehicle_type];
					ShowDropDownMenu(w, action_str, 0, VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN, 0, (w->window_number & VLW_MASK) == VLW_STANDARD ? 0 : 1);
					break;
				}

				case VLW_WIDGET_STOP_ALL:
				case VLW_WIDGET_START_ALL:
					DoCommandP(0, GB(w->window_number, 16, 16), (w->window_number & VLW_MASK) | (1 << 6) | (e->we.click.widget == VLW_WIDGET_START_ALL ? (1 << 5) : 0) | vl->vehicle_type, NULL, CMD_MASS_START_STOP);
					break;
			}
		} break;

		case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
			switch (e->we.dropdown.button) {
				case VLW_WIDGET_SORT_BY_PULLDOWN:
					if (vl->l.sort_type != e->we.dropdown.index) {
						/* value has changed -> resort */
						vl->l.flags |= VL_RESORT;
						vl->l.sort_type = e->we.dropdown.index;
						vl->_sorting->criteria = vl->l.sort_type;
					}
					break;
				case VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN:
					assert(vl->l.list_length != 0);

					switch (e->we.dropdown.index) {
						case 0: /* Replace window */
							ShowReplaceVehicleWindow(vl->vehicle_type);
							break;
						case 1: /* Send for servicing */
							DoCommandP(0, GB(w->window_number, 16, 16) /* StationID or OrderID (depending on VLW) */,
								(w->window_number & VLW_MASK) | DEPOT_MASS_SEND | DEPOT_SERVICE,
								NULL,
								GetCmdSendToDepot(vl->vehicle_type));
							break;
						case 2: /* Send to Depots */
							DoCommandP(0, GB(w->window_number, 16, 16) /* StationID or OrderID (depending on VLW) */,
								(w->window_number & VLW_MASK) | DEPOT_MASS_SEND,
								NULL,
								GetCmdSendToDepot(vl->vehicle_type));
							break;

						default: NOT_REACHED();
					}
					break;
				default: NOT_REACHED();
			}
			SetWindowDirty(w);
			break;

		case WE_DESTROY:
			free((void*)vl->sort_list);
			break;

		case WE_TICK: /* resort the list every 20 seconds orso (10 days) */
			if (_pause_game != 0) break;
			if (--vl->l.resort_timer == 0) {
				StationID station = ((w->window_number & VLW_MASK) == VLW_STATION_LIST) ? GB(w->window_number, 16, 16) : INVALID_STATION;
				PlayerID owner = (PlayerID)w->caption_color;

				DEBUG(misc, 3, "Periodic resort %d list player %d at station %d", vl->vehicle_type, owner, station);
				vl->l.resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
				vl->l.flags |= VL_RESORT;
				SetWindowDirty(w);
			}
			break;

		case WE_RESIZE: /* Update the scroll + matrix */
			w->vscroll.cap += e->we.sizing.diff.y / (int)w->resize.step_height;
			w->widget[VLW_WIDGET_LIST].data = (w->vscroll.cap << 8) + 1;
			break;
	}
}

static const WindowDesc _player_vehicle_list_train_desc = {
	WDP_AUTO, WDP_AUTO, 260, 182, 260, 182,
	WC_TRAINS_LIST, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_vehicle_list_widgets,
	PlayerVehWndProc
};

static const WindowDesc _player_vehicle_list_road_veh_desc = {
	WDP_AUTO, WDP_AUTO, 260, 182, 260, 182,
	WC_ROADVEH_LIST, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_vehicle_list_widgets,
	PlayerVehWndProc
};

static const WindowDesc _player_vehicle_list_ship_desc = {
	WDP_AUTO, WDP_AUTO, 260, 182, 260, 182,
	WC_SHIPS_LIST, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_vehicle_list_widgets,
	PlayerVehWndProc
};

static const WindowDesc _player_vehicle_list_aircraft_desc = {
	WDP_AUTO, WDP_AUTO, 260, 182, 260, 182,
	WC_AIRCRAFT_LIST, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_vehicle_list_widgets,
	PlayerVehWndProc
};

static void ShowVehicleListWindowLocal(PlayerID player, uint16 VLW_flag, VehicleType vehicle_type, uint16 unique_number)
{
	Window *w;
	WindowNumber num;

	if (!IsValidPlayer(player)) return;

	num = (unique_number << 16) | (vehicle_type << 11) | VLW_flag | player;

	/* The vehicle list windows have been unified. Just some strings need
	 * to be changed which happens in the WE_CREATE event and resizing
	 * some of the windows to the correct size */
	switch (vehicle_type) {
		default: NOT_REACHED();
		case VEH_TRAIN:
			w = AllocateWindowDescFront(&_player_vehicle_list_train_desc, num);
			if (w != NULL) ResizeWindow(w, 65, 38);
			break;
		case VEH_ROAD:
			w = AllocateWindowDescFront(&_player_vehicle_list_road_veh_desc, num);
			if (w != NULL) ResizeWindow(w, 0, 38);
			break;
		case VEH_SHIP:
			w = AllocateWindowDescFront(&_player_vehicle_list_ship_desc, num);
			break;
		case VEH_AIRCRAFT:
			w = AllocateWindowDescFront(&_player_vehicle_list_aircraft_desc, num);
			break;
	}

	if (w != NULL) {
		/* Set the minimum window size to the current window size */
		w->resize.width = w->width;
		w->resize.height = w->height;
	}
}

void ShowVehicleListWindow(PlayerID player, VehicleType vehicle_type)
{
	/* If _patches.advanced_vehicle_list > 1, display the Advanced list
	 * if _patches.advanced_vehicle_list == 1, display Advanced list only for local player
	 * if _ctrl_pressed, do the opposite action (Advanced list x Normal list)
	 */

	if ((_patches.advanced_vehicle_list > (uint)(player != _local_player)) != _ctrl_pressed) {
		ShowPlayerGroup(player, vehicle_type);
	} else {
		ShowVehicleListWindowLocal(player, VLW_STANDARD, vehicle_type, 0);
	}
}

void ShowVehicleListWindow(const Vehicle *v)
{
	if (v->orders == NULL) return; // no shared list to show
	ShowVehicleListWindowLocal(v->owner, VLW_SHARED_ORDERS, v->type, v->orders->index);
}

void ShowVehicleListWindow(PlayerID player, VehicleType vehicle_type, StationID station)
{
	ShowVehicleListWindowLocal(player, VLW_STATION_LIST, vehicle_type, station);
}

void ShowVehicleListWindow(PlayerID player, VehicleType vehicle_type, TileIndex depot_tile)
{
	uint16 depot_airport_index;

	if (vehicle_type == VEH_AIRCRAFT) {
		depot_airport_index = GetStationIndex(depot_tile);
	} else {
		Depot *depot = GetDepotByTile(depot_tile);
		if (depot == NULL) return; // no depot to show
		depot_airport_index = depot->index;
	}
	ShowVehicleListWindowLocal(player, VLW_DEPOT_LIST, vehicle_type, depot_airport_index);
}


/* Unified vehicle GUI - Vehicle Details Window */

/** Constants of vehicle details widget indices */
enum VehicleDetailsWindowWidgets {
	VLD_WIDGET_CLOSEBOX = 0,
	VLD_WIDGET_CAPTION,
	VLD_WIDGET_RENAME_VEHICLE,
	VLD_WIDGET_TOP_DETAILS,
	VLD_WIDGET_INCREASE_SERVICING_INTERVAL,
	VLD_WIDGET_DECREASE_SERVICING_INTERVAL,
	VLD_WIDGET_BOTTOM_RIGHT,
	VLD_WIDGET_MIDDLE_DETAILS,
	VLD_WIDGET_SCROLLBAR,
	VLD_WIDGET_DETAILS_CARGO_CARRIED,
	VLD_WIDGET_DETAILS_TRAIN_VEHICLES,
	VLD_WIDGET_DETAILS_CAPACITY_OF_EACH,
	VLD_WIDGET_DETAILS_TOTAL_CARGO,
	VLD_WIDGET_RESIZE,
};

/** Vehicle details widgets. */
static const Widget _vehicle_details_widgets[] = {
	{   WWT_CLOSEBOX,   RESIZE_NONE, 14,   0,  10,   0,  13, STR_00C5,             STR_018B_CLOSE_WINDOW},                  // VLD_WIDGET_CLOSEBOX
	{    WWT_CAPTION,  RESIZE_RIGHT, 14,  11, 364,   0,  13, 0x0,                  STR_018C_WINDOW_TITLE_DRAG_THIS},        // VLD_WIDGET_CAPTION
	{ WWT_PUSHTXTBTN,     RESIZE_LR, 14, 365, 404,   0,  13, STR_01AA_NAME,        STR_NULL /* filled in later */},         // VLD_WIDGET_RENAME_VEHICLE
	{      WWT_PANEL,  RESIZE_RIGHT, 14,   0, 404,  14,  55, 0x0,                  STR_NULL},                               // VLD_WIDGET_TOP_DETAILS
	{ WWT_PUSHTXTBTN,     RESIZE_TB, 14,   0,  10, 101, 106, STR_0188,             STR_884D_INCREASE_SERVICING_INTERVAL},   // VLD_WIDGET_INCREASE_SERVICING_INTERVAL
	{ WWT_PUSHTXTBTN,     RESIZE_TB, 14,   0,  10, 107, 112, STR_0189,             STR_884E_DECREASE_SERVICING_INTERVAL},   // VLD_WIDGET_DECREASE_SERVICING_INTERVAL
	{      WWT_PANEL,    RESIZE_RTB, 14,  11, 404, 101, 112, 0x0,                  STR_NULL},                               // VLD_WIDGET_BOTTOM_RIGHT
	{     WWT_MATRIX,     RESIZE_RB, 14,   0, 392,  56, 100, 0x701,                STR_NULL},                               // VLD_WIDGET_MIDDLE_DETAILS
	{  WWT_SCROLLBAR,    RESIZE_LRB, 14, 393, 404,  56, 100, 0x0,                  STR_0190_SCROLL_BAR_SCROLLS_LIST},       // VLD_WIDGET_SCROLLBAR
	{ WWT_PUSHTXTBTN,     RESIZE_TB, 14,   0,  95, 113, 124, STR_013C_CARGO,       STR_884F_SHOW_DETAILS_OF_CARGO_CARRIED}, // VLD_WIDGET_DETAILS_CARGO_CARRIED
	{ WWT_PUSHTXTBTN,     RESIZE_TB, 14,  96, 194, 113, 124, STR_013D_INFORMATION, STR_8850_SHOW_DETAILS_OF_TRAIN_VEHICLES},// VLD_WIDGET_DETAILS_TRAIN_VEHICLES
	{ WWT_PUSHTXTBTN,     RESIZE_TB, 14, 195, 293, 113, 124, STR_013E_CAPACITIES,  STR_8851_SHOW_CAPACITIES_OF_EACH},       // VLD_WIDGET_DETAILS_CAPACITY_OF_EACH
	{ WWT_PUSHTXTBTN,    RESIZE_RTB, 14, 294, 392, 113, 124, STR_013E_TOTAL_CARGO, STR_8852_SHOW_TOTAL_CARGO},              // VLD_WIDGET_DETAILS_TOTAL_CARGO
	{  WWT_RESIZEBOX,   RESIZE_LRTB, 14, 393, 404, 113, 124, 0x0,                  STR_RESIZE_BUTTON},                      // VLD_RESIZE
	{   WIDGETS_END},
};


/** Command indices for the _vehicle_command_translation_table. */
enum VehicleStringTranslation {
	VST_VEHICLE_AGE_RUNNING_COST_YR,
	VST_VEHICLE_MAX_SPEED,
	VST_VEHICLE_PROFIT_THIS_YEAR_LAST_YEAR,
	VST_VEHICLE_RELIABILITY_BREAKDOWNS,
};

/** Command codes for the shared buttons indexed by VehicleCommandTranslation and vehicle type. */
static const StringID _vehicle_translation_table[][4] = {
	{ // VST_VEHICLE_AGE_RUNNING_COST_YR
		STR_885D_AGE_RUNNING_COST_YR,
		STR_900D_AGE_RUNNING_COST_YR,
		STR_9812_AGE_RUNNING_COST_YR,
		STR_A00D_AGE_RUNNING_COST_YR,
	},
	{ // VST_VEHICLE_MAX_SPEED
		STR_NULL,
		STR_900E_MAX_SPEED,
		STR_9813_MAX_SPEED,
		STR_A00E_MAX_SPEED,
	},
	{ // VST_VEHICLE_PROFIT_THIS_YEAR_LAST_YEAR
		STR_885F_PROFIT_THIS_YEAR_LAST_YEAR,
		STR_900F_PROFIT_THIS_YEAR_LAST_YEAR,
		STR_9814_PROFIT_THIS_YEAR_LAST_YEAR,
		STR_A00F_PROFIT_THIS_YEAR_LAST_YEAR,
	},
	{ // VST_VEHICLE_RELIABILITY_BREAKDOWNS
		STR_8860_RELIABILITY_BREAKDOWNS,
		STR_9010_RELIABILITY_BREAKDOWNS,
		STR_9815_RELIABILITY_BREAKDOWNS,
		STR_A010_RELIABILITY_BREAKDOWNS,
	},
};

/** Initialize a newly created vehicle details window */
void CreateVehicleDetailsWindow(Window *w)
{
	const Vehicle *v = GetVehicle(w->window_number);

	switch (v->type) {
		case VEH_TRAIN:
			ResizeWindow(w, 0, 39);

			w->vscroll.cap = 6;
			w->height += 12;
			w->resize.step_height = 14;
			w->resize.height = w->height - 14 * 2; // Minimum of 4 wagons in the display

			w->widget[VLD_WIDGET_RENAME_VEHICLE].tooltips = STR_8867_NAME_TRAIN;
			w->widget[VLD_WIDGET_CAPTION].data = STR_8802_DETAILS;
			break;

		case VEH_ROAD: {
			w->widget[VLD_WIDGET_CAPTION].data = STR_900C_DETAILS;
			w->widget[VLD_WIDGET_RENAME_VEHICLE].tooltips = STR_902E_NAME_ROAD_VEHICLE;

			if (!RoadVehHasArticPart(v)) break;

			/* Draw the text under the vehicle instead of next to it, minus the
			* height already allocated for the cargo of the first vehicle. */
			uint height_extension = 15 - 11;

			/* Add space for the cargo amount for each part. */
			for (const Vehicle *u = v; u != NULL; u = u->Next()) {
				height_extension += 11;
			}

			ResizeWindow(w, 0, height_extension);
		} break;

		case VEH_SHIP:
			w->widget[VLD_WIDGET_RENAME_VEHICLE].tooltips = STR_982F_NAME_SHIP;
			w->widget[VLD_WIDGET_CAPTION].data = STR_9811_DETAILS;
			break;

		case VEH_AIRCRAFT:
			ResizeWindow(w, 0, 11);
			w->widget[VLD_WIDGET_RENAME_VEHICLE].tooltips = STR_A032_NAME_AIRCRAFT;
			w->widget[VLD_WIDGET_CAPTION].data = STR_A00C_DETAILS;
			break;
		default: NOT_REACHED();
	}

	if (v->type != VEH_TRAIN) {
		w->vscroll.cap = 1;
		w->widget[VLD_WIDGET_MIDDLE_DETAILS].right += 12;
	}

	w->widget[VLD_WIDGET_MIDDLE_DETAILS].data = (w->vscroll.cap << 8) + 1;
	w->caption_color = v->owner;

	WP(w, vehicledetails_d).tab = 0;
}

/** Checks whether service interval is enabled for the vehicle. */
static inline bool IsVehicleServiceIntervalEnabled(const VehicleType vehicle_type)
{
	switch (vehicle_type) {
		default: NOT_REACHED();
		case VEH_TRAIN:    return _patches.servint_trains   != 0; break;
		case VEH_ROAD:     return _patches.servint_roadveh  != 0; break;
		case VEH_SHIP:     return _patches.servint_ships    != 0; break;
		case VEH_AIRCRAFT: return _patches.servint_aircraft != 0; break;
	}
	return false; // kill a compiler warning
}

extern int GetTrainDetailsWndVScroll(VehicleID veh_id, byte det_tab);
extern void DrawTrainDetails(const Vehicle *v, int x, int y, int vscroll_pos, uint16 vscroll_cap, byte det_tab);
extern void DrawRoadVehDetails(const Vehicle *v, int x, int y);
extern void DrawShipDetails(const Vehicle *v, int x, int y);
extern void DrawAircraftDetails(const Vehicle *v, int x, int y);

/**
* Draw the details for the given vehicle at the position (x,y) of the Details windows
*
* @param v current vehicle
* @param x The x coordinate
* @param y The y coordinate
* @param vscroll_pos (train only)
* @param vscroll_cap (train only)
* @param det_tab (train only)
*/
static inline void DrawVehicleDetails(const Vehicle *v, int x, int y, int vscroll_pos, uint vscroll_cap, byte det_tab)
{
	switch (v->type) {
		case VEH_TRAIN:    DrawTrainDetails(v, x, y, vscroll_pos, vscroll_cap, det_tab);  break;
		case VEH_ROAD:     DrawRoadVehDetails(v, x, y);  break;
		case VEH_SHIP:     DrawShipDetails(v, x, y);     break;
		case VEH_AIRCRAFT: DrawAircraftDetails(v, x, y); break;
		default: NOT_REACHED();
	}
}

/** Repaint vehicle details window. */
static void DrawVehicleDetailsWindow(Window *w)
{
	const Vehicle *v = GetVehicle(w->window_number);
	byte det_tab = WP(w, vehicledetails_d).tab;

	w->SetWidgetDisabledState(VLD_WIDGET_RENAME_VEHICLE, v->owner != _local_player);

	if (v->type == VEH_TRAIN) {
		w->DisableWidget(det_tab + VLD_WIDGET_DETAILS_CARGO_CARRIED);
		SetVScrollCount(w, GetTrainDetailsWndVScroll(v->index, det_tab));
	}

	w->SetWidgetsHiddenState(v->type != VEH_TRAIN,
		VLD_WIDGET_SCROLLBAR,
		VLD_WIDGET_DETAILS_CARGO_CARRIED,
		VLD_WIDGET_DETAILS_TRAIN_VEHICLES,
		VLD_WIDGET_DETAILS_CAPACITY_OF_EACH,
		VLD_WIDGET_DETAILS_TOTAL_CARGO,
		VLD_WIDGET_RESIZE,
		WIDGET_LIST_END);

	/* Disable service-scroller when interval is set to disabled */
	w->SetWidgetsDisabledState(!IsVehicleServiceIntervalEnabled(v->type),
		VLD_WIDGET_INCREASE_SERVICING_INTERVAL,
		VLD_WIDGET_DECREASE_SERVICING_INTERVAL,
		WIDGET_LIST_END);


	SetDParam(0, v->index);
	DrawWindowWidgets(w);

	/* Draw running cost */
	SetDParam(1, v->age / 366);
	SetDParam(0, (v->age + 365 < v->max_age) ? STR_AGE : STR_AGE_RED);
	SetDParam(2, v->max_age / 366);
	SetDParam(3, v->GetDisplayRunningCost());
	DrawString(2, 15, _vehicle_translation_table[VST_VEHICLE_AGE_RUNNING_COST_YR][v->type], TC_FROMSTRING);

	/* Draw max speed */
	switch (v->type) {
		case VEH_TRAIN:
			SetDParam(2, v->GetDisplayMaxSpeed());
			SetDParam(1, v->u.rail.cached_power);
			SetDParam(0, v->u.rail.cached_weight);
			SetDParam(3, v->u.rail.cached_max_te / 1000);
			DrawString(2, 25, (_patches.realistic_acceleration && v->u.rail.railtype != RAILTYPE_MAGLEV) ?
				STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED_MAX_TE :
				STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED, TC_FROMSTRING);
			break;

		case VEH_ROAD:
		case VEH_SHIP:
		case VEH_AIRCRAFT:
			SetDParam(0, v->GetDisplayMaxSpeed());
			DrawString(2, 25, _vehicle_translation_table[VST_VEHICLE_MAX_SPEED][v->type], TC_FROMSTRING);
			break;

		default: NOT_REACHED();
	}

	/* Draw profit */
	SetDParam(0, v->profit_this_year);
	SetDParam(1, v->profit_last_year);
	DrawString(2, 35, _vehicle_translation_table[VST_VEHICLE_PROFIT_THIS_YEAR_LAST_YEAR][v->type], TC_FROMSTRING);

	/* Draw breakdown & reliability */
	SetDParam(0, v->reliability * 100 >> 16);
	SetDParam(1, v->breakdowns_since_last_service);
	DrawString(2, 45, _vehicle_translation_table[VST_VEHICLE_RELIABILITY_BREAKDOWNS][v->type], TC_FROMSTRING);

	/* Draw service interval text */
	SetDParam(0, v->service_interval);
	SetDParam(1, v->date_of_last_service);
	DrawString(13, w->height - (v->type != VEH_TRAIN ? 11 : 23), _patches.servint_ispercent ? STR_SERVICING_INTERVAL_PERCENT : STR_883C_SERVICING_INTERVAL_DAYS, TC_FROMSTRING);

	switch (v->type) {
		case VEH_TRAIN:
			DrawVehicleDetails(v, 2, 57, w->vscroll.pos, w->vscroll.cap, det_tab);
			break;

		case VEH_ROAD:
		case VEH_SHIP:
		case VEH_AIRCRAFT:
			DrawVehicleImage(v, 3, 57, INVALID_VEHICLE, 0, 0);
			DrawVehicleDetails(v, 75, 57, w->vscroll.pos, w->vscroll.cap, det_tab);
			break;

		default: NOT_REACHED();
	}
}

/** Message strings for renaming vehicles indexed by vehicle type. */
static const StringID _name_vehicle_title[] = {
	STR_8865_NAME_TRAIN,
	STR_902C_NAME_ROAD_VEHICLE,
	STR_9831_NAME_SHIP,
	STR_A030_NAME_AIRCRAFT
};

/** Message strings for error while renaming indexed by vehicle type. */
static const StringID _name_vehicle_error[] = {
	STR_8866_CAN_T_NAME_TRAIN,
	STR_902D_CAN_T_NAME_ROAD_VEHICLE,
	STR_9832_CAN_T_NAME_SHIP,
	STR_A031_CAN_T_NAME_AIRCRAFT
};

/** Window event hook for vehicle details. */
static void VehicleDetailsWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_CREATE:
			CreateVehicleDetailsWindow(w);
			break;

		case WE_PAINT:
			DrawVehicleDetailsWindow(w);
			break;

		case WE_CLICK: {
			switch (e->we.click.widget) {
				case VLD_WIDGET_RENAME_VEHICLE: {// rename
					const Vehicle *v = GetVehicle(w->window_number);
					SetDParam(0, v->index);
					ShowQueryString(STR_VEHICLE_NAME, _name_vehicle_title[v->type], 31, 150, w, CS_ALPHANUMERAL);
				} break;

				case VLD_WIDGET_INCREASE_SERVICING_INTERVAL:   // increase int
				case VLD_WIDGET_DECREASE_SERVICING_INTERVAL: { // decrease int
					int mod = _ctrl_pressed ? 5 : 10;
					const Vehicle *v = GetVehicle(w->window_number);

					mod = (e->we.click.widget == VLD_WIDGET_DECREASE_SERVICING_INTERVAL) ? -mod : mod;
					mod = GetServiceIntervalClamped(mod + v->service_interval);
					if (mod == v->service_interval) return;

					DoCommandP(v->tile, v->index, mod, NULL, CMD_CHANGE_SERVICE_INT | CMD_MSG(STR_018A_CAN_T_CHANGE_SERVICING));
				} break;

				case VLD_WIDGET_DETAILS_CARGO_CARRIED:
				case VLD_WIDGET_DETAILS_TRAIN_VEHICLES:
				case VLD_WIDGET_DETAILS_CAPACITY_OF_EACH:
				case VLD_WIDGET_DETAILS_TOTAL_CARGO:
					w->SetWidgetsDisabledState(false,
						VLD_WIDGET_DETAILS_CARGO_CARRIED,
						VLD_WIDGET_DETAILS_TRAIN_VEHICLES,
						VLD_WIDGET_DETAILS_CAPACITY_OF_EACH,
						VLD_WIDGET_DETAILS_TOTAL_CARGO,
						e->we.click.widget,
						WIDGET_LIST_END);

					WP(w, vehicledetails_d).tab = e->we.click.widget - VLD_WIDGET_DETAILS_CARGO_CARRIED;
					SetWindowDirty(w);
					break;
			}
		} break;

		case WE_ON_EDIT_TEXT:
			if (!StrEmpty(e->we.edittext.str)) {
				_cmd_text = e->we.edittext.str;
				DoCommandP(0, w->window_number, 0, NULL, CMD_NAME_VEHICLE | CMD_MSG(_name_vehicle_error[GetVehicle(w->window_number)->type]));
			}
			break;

		case WE_RESIZE:
			if (e->we.sizing.diff.x != 0) ResizeButtons(w, VLD_WIDGET_DETAILS_CARGO_CARRIED, VLD_WIDGET_DETAILS_TOTAL_CARGO);
			if (e->we.sizing.diff.y == 0) break;

			w->vscroll.cap += e->we.sizing.diff.y / 14;
			w->widget[VLD_WIDGET_MIDDLE_DETAILS].data = (w->vscroll.cap << 8) + 1;
			break;
	}
}

/** Vehicle details window descriptor. */
static const WindowDesc _vehicle_details_desc = {
	WDP_AUTO, WDP_AUTO, 405, 113, 405, 113,
	WC_VEHICLE_DETAILS, WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_vehicle_details_widgets,
	VehicleDetailsWndProc
};

/** Shows the vehicle details window of the given vehicle. */
static void ShowVehicleDetailsWindow(const Vehicle *v)
{
	DeleteWindowById(WC_VEHICLE_ORDERS, v->index);
	DeleteWindowById(WC_VEHICLE_DETAILS, v->index);
	AllocateWindowDescFront(&_vehicle_details_desc, v->index);
}


/* Unified vehicle GUI - Vehicle View Window */

/** Constants of vehicle view widget indices */
enum VehicleViewWindowWidgets {
	VVW_WIDGET_CLOSEBOX = 0,
	VVW_WIDGET_CAPTION,
	VVW_WIDGET_STICKY,
	VVW_WIDGET_PANEL,
	VVW_WIDGET_VIEWPORT,
	VVW_WIDGET_START_STOP_VEH,
	VVW_WIDGET_CENTER_MAIN_VIEH,
	VVW_WIDGET_GOTO_DEPOT,
	VVW_WIDGET_REFIT_VEH,
	VVW_WIDGET_SHOW_ORDERS,
	VVW_WIDGET_SHOW_DETAILS,
	VVW_WIDGET_CLONE_VEH,
	VVW_WIDGET_EMPTY_BOTTOM_RIGHT,
	VVW_WIDGET_RESIZE,
	VVW_WIDGET_TURN_AROUND,
	VVW_WIDGET_FORCE_PROCEED,
};

/** Vehicle view widgets. */
static const Widget _vehicle_view_widgets[] = {
	{   WWT_CLOSEBOX,  RESIZE_NONE,  14,   0,  10,   0,  13, STR_00C5,                 STR_018B_CLOSE_WINDOW },           // VVW_WIDGET_CLOSEBOX
	{    WWT_CAPTION, RESIZE_RIGHT,  14,  11, 237,   0,  13, 0x0 /* filled later */,   STR_018C_WINDOW_TITLE_DRAG_THIS }, // VVW_WIDGET_CAPTION
	{  WWT_STICKYBOX,    RESIZE_LR,  14, 238, 249,   0,  13, 0x0,                      STR_STICKY_BUTTON },               // VVW_WIDGET_STICKY
	{      WWT_PANEL,    RESIZE_RB,  14,   0, 231,  14, 103, 0x0,                      STR_NULL },                        // VVW_WIDGET_PANEL
	{      WWT_INSET,    RESIZE_RB,  14,   2, 229,  16, 101, 0x0,                      STR_NULL },                        // VVW_WIDGET_VIEWPORT
	{    WWT_PUSHBTN,   RESIZE_RTB,  14,   0, 237, 104, 115, 0x0,                      0x0 /* filled later */ },          // VVW_WIDGET_START_STOP_VEH
	{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  14,  31, SPR_CENTRE_VIEW_VEHICLE,  0x0 /* filled later */ },          // VVW_WIDGET_CENTER_MAIN_VIEH
	{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  32,  49, 0x0 /* filled later */,   0x0 /* filled later */ },          // VVW_WIDGET_GOTO_DEPOT
	{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  50,  67, SPR_REFIT_VEHICLE,        0x0 /* filled later */ },          // VVW_WIDGET_REFIT_VEH
	{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  68,  85, SPR_SHOW_ORDERS,          0x0 /* filled later */ },          // VVW_WIDGET_SHOW_ORDERS
	{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  86, 103, SPR_SHOW_VEHICLE_DETAILS, 0x0 /* filled later */ },          // VVW_WIDGET_SHOW_DETAILS
	{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  32,  49, 0x0 /* filled later */,   0x0 /* filled later */ },          // VVW_WIDGET_CLONE_VEH
	{      WWT_PANEL,   RESIZE_LRB,  14, 232, 249, 104, 103, 0x0,                      STR_NULL },                        // VVW_WIDGET_EMPTY_BOTTOM_RIGHT
	{  WWT_RESIZEBOX,  RESIZE_LRTB,  14, 238, 249, 104, 115, 0x0,                      STR_NULL },                        // VVW_WIDGET_RESIZE
	{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  50,  67, SPR_FORCE_VEHICLE_TURN,   STR_9020_FORCE_VEHICLE_TO_TURN_AROUND }, // VVW_WIDGET_TURN_AROUND
	{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  50,  67, SPR_IGNORE_SIGNALS,       STR_884A_FORCE_TRAIN_TO_PROCEED },       // VVW_WIDGET_FORCE_PROCEED
{   WIDGETS_END},
};


static void VehicleViewWndProc(Window *w, WindowEvent *e);

/** Vehicle view window descriptor for all vehicles but trains. */
static const WindowDesc _vehicle_view_desc = {
	WDP_AUTO, WDP_AUTO, 250, 116, 250, 116,
	WC_VEHICLE_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_vehicle_view_widgets,
	VehicleViewWndProc
};

/** Vehicle view window descriptor for trains. Only minimum_height and
 *  default_height are different for train view.
 */
static const WindowDesc _train_view_desc = {
	WDP_AUTO, WDP_AUTO, 250, 134, 250, 134,
	WC_VEHICLE_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_vehicle_view_widgets,
	VehicleViewWndProc
};


/* Just to make sure, nobody has changed the vehicle type constants, as we are
	 using them for array indexing in a number of places here. */
assert_compile(VEH_TRAIN == 0);
assert_compile(VEH_ROAD == 1);
assert_compile(VEH_SHIP == 2);
assert_compile(VEH_AIRCRAFT == 3);

/** Zoom levels for vehicle views indexed by vehicle type. */
static const ZoomLevel _vehicle_view_zoom_levels[] = {
	ZOOM_LVL_TRAIN,
	ZOOM_LVL_ROADVEH,
	ZOOM_LVL_SHIP,
	ZOOM_LVL_AIRCRAFT,
};

/* Constants for geometry of vehicle view viewport */
static const int VV_VIEWPORT_X = 3;
static const int VV_VIEWPORT_Y = 17;
static const int VV_INITIAL_VIEWPORT_WIDTH = 226;
static const int VV_INITIAL_VIEWPORT_HEIGHT = 84;
static const int VV_INITIAL_VIEWPORT_HEIGHT_TRAIN = 102;

/** Shows the vehicle view window of the given vehicle. */
void ShowVehicleViewWindow(const Vehicle *v)
{
	Window *w = AllocateWindowDescFront((v->type == VEH_TRAIN) ? &_train_view_desc : &_vehicle_view_desc, v->index);

	if (w != NULL) {
		w->caption_color = v->owner;
		AssignWindowViewport(w, VV_VIEWPORT_X, VV_VIEWPORT_Y, VV_INITIAL_VIEWPORT_WIDTH,
												 (v->type == VEH_TRAIN) ? VV_INITIAL_VIEWPORT_HEIGHT_TRAIN : VV_INITIAL_VIEWPORT_HEIGHT,
												 w->window_number | (1 << 31), _vehicle_view_zoom_levels[v->type]);
	}
}

/** Initialize a newly created vehicle view window */
static void CreateVehicleViewWindow(Window *w)
{
	const Vehicle *v = GetVehicle(w->window_number);

	/*
	 * fill in data and tooltip codes for the widgets and
	 * move some of the buttons for trains
	 */
	switch (v->type) {
		case VEH_TRAIN:
			w->widget[VVW_WIDGET_CAPTION].data = STR_882E;

			w->widget[VVW_WIDGET_START_STOP_VEH].tooltips = STR_8846_CURRENT_TRAIN_ACTION_CLICK;

			w->widget[VVW_WIDGET_CENTER_MAIN_VIEH].tooltips = STR_8848_CENTER_MAIN_VIEW_ON_TRAIN;

			w->widget[VVW_WIDGET_GOTO_DEPOT].data = SPR_SEND_TRAIN_TODEPOT;
			w->widget[VVW_WIDGET_GOTO_DEPOT].tooltips = STR_8849_SEND_TRAIN_TO_DEPOT;

			w->widget[VVW_WIDGET_REFIT_VEH].tooltips = STR_RAIL_REFIT_VEHICLE_TO_CARRY;

			w->widget[VVW_WIDGET_SHOW_ORDERS].tooltips = STR_8847_SHOW_TRAIN_S_ORDERS;

			w->widget[VVW_WIDGET_SHOW_DETAILS].tooltips = STR_884C_SHOW_TRAIN_DETAILS;

			w->widget[VVW_WIDGET_CLONE_VEH].data = SPR_CLONE_TRAIN;
			w->widget[VVW_WIDGET_CLONE_VEH].tooltips = STR_CLONE_TRAIN_INFO;

			w->widget[VVW_WIDGET_TURN_AROUND].tooltips = STR_884B_REVERSE_DIRECTION_OF_TRAIN;


			/* due to more buttons we must modify the layout a bit for trains */
			w->widget[VVW_WIDGET_PANEL].bottom = 121;
			w->widget[VVW_WIDGET_VIEWPORT].bottom = 119;

			w->widget[VVW_WIDGET_START_STOP_VEH].top = 122;
			w->widget[VVW_WIDGET_START_STOP_VEH].bottom = 133;

			w->widget[VVW_WIDGET_REFIT_VEH].top = 68;
			w->widget[VVW_WIDGET_REFIT_VEH].bottom = 85;

			w->widget[VVW_WIDGET_SHOW_ORDERS].top = 86;
			w->widget[VVW_WIDGET_SHOW_ORDERS].bottom = 103;

			w->widget[VVW_WIDGET_SHOW_DETAILS].top = 104;
			w->widget[VVW_WIDGET_SHOW_DETAILS].bottom = 121;

			w->widget[VVW_WIDGET_EMPTY_BOTTOM_RIGHT].top = 122;
			w->widget[VVW_WIDGET_EMPTY_BOTTOM_RIGHT].bottom = 121;

			w->widget[VVW_WIDGET_RESIZE].top = 122;
			w->widget[VVW_WIDGET_RESIZE].bottom = 133;

			w->widget[VVW_WIDGET_TURN_AROUND].top = 68;
			w->widget[VVW_WIDGET_TURN_AROUND].bottom = 85;
			break;

		case VEH_ROAD:
			w->widget[VVW_WIDGET_CAPTION].data = STR_9002;

			w->widget[VVW_WIDGET_START_STOP_VEH].tooltips = STR_901C_CURRENT_VEHICLE_ACTION;

			w->widget[VVW_WIDGET_CENTER_MAIN_VIEH].tooltips = STR_901E_CENTER_MAIN_VIEW_ON_VEHICLE;

			w->widget[VVW_WIDGET_GOTO_DEPOT].data = SPR_SEND_ROADVEH_TODEPOT;
			w->widget[VVW_WIDGET_GOTO_DEPOT].tooltips = STR_901F_SEND_VEHICLE_TO_DEPOT;

			w->widget[VVW_WIDGET_REFIT_VEH].tooltips = STR_REFIT_ROAD_VEHICLE_TO_CARRY;

			w->widget[VVW_WIDGET_SHOW_ORDERS].tooltips = STR_901D_SHOW_VEHICLE_S_ORDERS;

			w->widget[VVW_WIDGET_SHOW_DETAILS].tooltips = STR_9021_SHOW_ROAD_VEHICLE_DETAILS;

			w->widget[VVW_WIDGET_CLONE_VEH].data = SPR_CLONE_ROADVEH;
			w->widget[VVW_WIDGET_CLONE_VEH].tooltips = STR_CLONE_ROAD_VEHICLE_INFO;

			w->SetWidgetHiddenState(VVW_WIDGET_FORCE_PROCEED, true);
			break;

		case VEH_SHIP:
			w->widget[VVW_WIDGET_CAPTION].data = STR_980F;

			w->widget[VVW_WIDGET_START_STOP_VEH].tooltips = STR_9827_CURRENT_SHIP_ACTION_CLICK;

			w->widget[VVW_WIDGET_CENTER_MAIN_VIEH].tooltips = STR_9829_CENTER_MAIN_VIEW_ON_SHIP;

			w->widget[VVW_WIDGET_GOTO_DEPOT].data = SPR_SEND_SHIP_TODEPOT;
			w->widget[VVW_WIDGET_GOTO_DEPOT].tooltips = STR_982A_SEND_SHIP_TO_DEPOT;

			w->widget[VVW_WIDGET_REFIT_VEH].tooltips = STR_983A_REFIT_CARGO_SHIP_TO_CARRY;

			w->widget[VVW_WIDGET_SHOW_ORDERS].tooltips = STR_9828_SHOW_SHIP_S_ORDERS;

			w->widget[VVW_WIDGET_SHOW_DETAILS].tooltips = STR_982B_SHOW_SHIP_DETAILS;

			w->widget[VVW_WIDGET_CLONE_VEH].data = SPR_CLONE_SHIP;
			w->widget[VVW_WIDGET_CLONE_VEH].tooltips = STR_CLONE_SHIP_INFO;

			w->SetWidgetsHiddenState(true,
																	VVW_WIDGET_TURN_AROUND,
																	VVW_WIDGET_FORCE_PROCEED,
																	WIDGET_LIST_END);
			break;

		case VEH_AIRCRAFT:
			w->widget[VVW_WIDGET_CAPTION].data = STR_A00A;

			w->widget[VVW_WIDGET_START_STOP_VEH].tooltips = STR_A027_CURRENT_AIRCRAFT_ACTION;

			w->widget[VVW_WIDGET_CENTER_MAIN_VIEH].tooltips = STR_A029_CENTER_MAIN_VIEW_ON_AIRCRAFT;

			w->widget[VVW_WIDGET_GOTO_DEPOT].data = SPR_SEND_AIRCRAFT_TODEPOT;
			w->widget[VVW_WIDGET_GOTO_DEPOT].tooltips = STR_A02A_SEND_AIRCRAFT_TO_HANGAR;

			w->widget[VVW_WIDGET_REFIT_VEH].tooltips = STR_A03B_REFIT_AIRCRAFT_TO_CARRY;

			w->widget[VVW_WIDGET_SHOW_ORDERS].tooltips = STR_A028_SHOW_AIRCRAFT_S_ORDERS;

			w->widget[VVW_WIDGET_SHOW_DETAILS].tooltips = STR_A02B_SHOW_AIRCRAFT_DETAILS;

			w->widget[VVW_WIDGET_CLONE_VEH].data = SPR_CLONE_AIRCRAFT;
			w->widget[VVW_WIDGET_CLONE_VEH].tooltips = STR_CLONE_AIRCRAFT_INFO;

			w->SetWidgetsHiddenState(true,
																	VVW_WIDGET_TURN_AROUND,
																	VVW_WIDGET_FORCE_PROCEED,
																	WIDGET_LIST_END);
			break;

			default: NOT_REACHED();
	}
}

/** Checks whether the vehicle may be refitted at the moment.*/
static bool IsVehicleRefitable(const Vehicle *v)
{
	/* Why is this so different for different vehicles?
	 * Does maybe work one solution for all?
	 */
	switch (v->type) {
		case VEH_TRAIN:    return false;
		case VEH_ROAD:     return EngInfo(v->engine_type)->refit_mask != 0 && v->IsStoppedInDepot();
		case VEH_SHIP:     return ShipVehInfo(v->engine_type)->refittable && v->IsStoppedInDepot();
		case VEH_AIRCRAFT: return v->IsStoppedInDepot();
		default: NOT_REACHED();
	}
}

/** Message strings for heading to depot indexed by vehicle type. */
static const StringID _heading_for_depot_strings[] = {
	STR_HEADING_FOR_TRAIN_DEPOT,
	STR_HEADING_FOR_ROAD_DEPOT,
	STR_HEADING_FOR_SHIP_DEPOT,
	STR_HEADING_FOR_HANGAR,
};

/** Message strings for heading to depot and servicing indexed by vehicle type. */
static const StringID _heading_for_depot_service_strings[] = {
	STR_HEADING_FOR_TRAIN_DEPOT_SERVICE,
	STR_HEADING_FOR_ROAD_DEPOT_SERVICE,
	STR_HEADING_FOR_SHIP_DEPOT_SERVICE,
	STR_HEADING_FOR_HANGAR_SERVICE,
};

/** Repaint vehicle view window. */
static void DrawVehicleViewWindow(Window *w)
{
	const Vehicle *v = GetVehicle(w->window_number);
	StringID str;
	bool is_localplayer = v->owner == _local_player;
	bool refitable_and_stopped_in_depot = IsVehicleRefitable(v);

	w->SetWidgetDisabledState(VVW_WIDGET_GOTO_DEPOT, !is_localplayer);
	w->SetWidgetDisabledState(VVW_WIDGET_REFIT_VEH,
															 !refitable_and_stopped_in_depot || !is_localplayer);
	w->SetWidgetDisabledState(VVW_WIDGET_CLONE_VEH, !is_localplayer);

	if (v->type == VEH_TRAIN) {
		w->SetWidgetDisabledState(VVW_WIDGET_FORCE_PROCEED, !is_localplayer);
		w->SetWidgetDisabledState(VVW_WIDGET_TURN_AROUND, !is_localplayer);

		/* Cargo refit button is disabled, until we know we can enable it below. */

		if (is_localplayer) {
			/* See if any vehicle can be refitted */
			for (const Vehicle *u = v; u != NULL; u = u->Next()) {
				if (EngInfo(u->engine_type)->refit_mask != 0 ||
						(RailVehInfo(v->engine_type)->railveh_type != RAILVEH_WAGON && v->cargo_cap != 0)) {
					w->EnableWidget(VVW_WIDGET_REFIT_VEH);
					/* We have a refittable carriage, bail out */
					break;
				}
			}
		}
	}

	/* draw widgets & caption */
	SetDParam(0, v->index);
	DrawWindowWidgets(w);

	if (v->vehstatus & VS_CRASHED) {
		str = STR_8863_CRASHED;
	} else if (v->type != VEH_AIRCRAFT && v->breakdown_ctr == 1) { // check for aircraft necessary?
		str = STR_885C_BROKEN_DOWN;
	} else if (v->vehstatus & VS_STOPPED) {
		if (v->type == VEH_TRAIN) {
			if (v->cur_speed == 0) {
				if (v->u.rail.cached_power == 0) {
					str = STR_TRAIN_NO_POWER;
				} else {
					str = STR_8861_STOPPED;
				}
			} else {
				SetDParam(0, v->GetDisplaySpeed());
				str = STR_TRAIN_STOPPING + _patches.vehicle_speed;
			}
		} else { // no train
			str = STR_8861_STOPPED;
		}
	} else { // vehicle is in a "normal" state, show current order
		switch (v->current_order.type) {
			case OT_GOTO_STATION: {
				SetDParam(0, v->current_order.dest);
				SetDParam(1, v->GetDisplaySpeed());
				str = STR_HEADING_FOR_STATION + _patches.vehicle_speed;
			} break;

			case OT_GOTO_DEPOT: {
				if (v->type == VEH_AIRCRAFT) {
					/* Aircrafts always go to a station, even if you say depot */
					SetDParam(0, v->current_order.dest);
					SetDParam(1, v->GetDisplaySpeed());
				} else {
					Depot *depot = GetDepot(v->current_order.dest);
					SetDParam(0, depot->town_index);
					SetDParam(1, v->GetDisplaySpeed());
				}
				if (HasBit(v->current_order.flags, OFB_HALT_IN_DEPOT) && !HasBit(v->current_order.flags, OFB_PART_OF_ORDERS)) {
					str = _heading_for_depot_strings[v->type] + _patches.vehicle_speed;
				} else {
					str = _heading_for_depot_service_strings[v->type] + _patches.vehicle_speed;
				}
			} break;

			case OT_LOADING:
				str = STR_882F_LOADING_UNLOADING;
				break;

			case OT_GOTO_WAYPOINT: {
				assert(v->type == VEH_TRAIN);
				SetDParam(0, v->current_order.dest);
				str = STR_HEADING_FOR_WAYPOINT + _patches.vehicle_speed;
				SetDParam(1, v->GetDisplaySpeed());
				break;
			}

			case OT_LEAVESTATION:
				if (v->type != VEH_AIRCRAFT) {
					str = STR_882F_LOADING_UNLOADING;
					break;
				}
				/* fall-through if aircraft. Does this even happen? */

			default:
				if (v->num_orders == 0) {
					str = STR_NO_ORDERS + _patches.vehicle_speed;
					SetDParam(0, v->GetDisplaySpeed());
				} else {
					str = STR_EMPTY;
				}
				break;
		}
	}

	/* draw the flag plus orders */
	DrawSprite(v->vehstatus & VS_STOPPED ? SPR_FLAG_VEH_STOPPED : SPR_FLAG_VEH_RUNNING, PAL_NONE, 2, w->widget[VVW_WIDGET_START_STOP_VEH].top + 1);
	DrawStringCenteredTruncated(w->widget[VVW_WIDGET_START_STOP_VEH].left + 8, w->widget[VVW_WIDGET_START_STOP_VEH].right, w->widget[VVW_WIDGET_START_STOP_VEH].top + 1, str, TC_FROMSTRING);
	DrawWindowViewport(w);
}

/** Command indices for the _vehicle_command_translation_table. */
enum VehicleCommandTranslation {
	VCT_CMD_START_STOP = 0,
	VCT_CMD_GOTO_DEPOT,
	VCT_CMD_CLONE_VEH,
	VCT_CMD_TURN_AROUND,
};

/** Command codes for the shared buttons indexed by VehicleCommandTranslation and vehicle type. */
static const uint32 _vehicle_command_translation_table[][4] = {
	{ // VCT_CMD_START_STOP
		CMD_START_STOP_TRAIN | CMD_MSG(STR_883B_CAN_T_STOP_START_TRAIN),
		CMD_START_STOP_ROADVEH | CMD_MSG(STR_9015_CAN_T_STOP_START_ROAD_VEHICLE),
		CMD_START_STOP_SHIP | CMD_MSG(STR_9818_CAN_T_STOP_START_SHIP),
		CMD_START_STOP_AIRCRAFT | CMD_MSG(STR_A016_CAN_T_STOP_START_AIRCRAFT)
	},
	{ // VCT_CMD_GOTO_DEPOT
		/* TrainGotoDepot has a nice randomizer in the pathfinder, which causes desyncs... */
		CMD_SEND_TRAIN_TO_DEPOT | CMD_NO_TEST_IF_IN_NETWORK | CMD_MSG(STR_8830_CAN_T_SEND_TRAIN_TO_DEPOT),
		CMD_SEND_ROADVEH_TO_DEPOT | CMD_MSG(STR_9018_CAN_T_SEND_VEHICLE_TO_DEPOT),
		CMD_SEND_SHIP_TO_DEPOT | CMD_MSG(STR_9819_CAN_T_SEND_SHIP_TO_DEPOT),
		CMD_SEND_AIRCRAFT_TO_HANGAR | CMD_MSG(STR_A012_CAN_T_SEND_AIRCRAFT_TO)
	},
	{ // VCT_CMD_CLONE_VEH
		CMD_CLONE_VEHICLE | CMD_MSG(STR_882B_CAN_T_BUILD_RAILROAD_VEHICLE),
		CMD_CLONE_VEHICLE | CMD_MSG(STR_9009_CAN_T_BUILD_ROAD_VEHICLE),
		CMD_CLONE_VEHICLE | CMD_MSG(STR_980D_CAN_T_BUILD_SHIP),
		CMD_CLONE_VEHICLE | CMD_MSG(STR_A008_CAN_T_BUILD_AIRCRAFT)
	},
	{ // VCT_CMD_TURN_AROUND
		CMD_REVERSE_TRAIN_DIRECTION | CMD_MSG(STR_8869_CAN_T_REVERSE_DIRECTION),
		CMD_TURN_ROADVEH | CMD_MSG(STR_9033_CAN_T_MAKE_VEHICLE_TURN),
		0xffffffff, // invalid for ships
		0xffffffff  // invalid for aircrafts
	},
};

/** Window event hook for vehicle view. */
static void VehicleViewWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_CREATE:
			CreateVehicleViewWindow(w);
			break;

		case WE_PAINT:
			DrawVehicleViewWindow(w);
			break;

		case WE_CLICK: {
			const Vehicle *v = GetVehicle(w->window_number);

			switch (e->we.click.widget) {
				case VVW_WIDGET_START_STOP_VEH: /* start stop */
					DoCommandP(v->tile, v->index, 0, NULL,
										 _vehicle_command_translation_table[VCT_CMD_START_STOP][v->type]);
					break;
				case VVW_WIDGET_CENTER_MAIN_VIEH: {/* center main view */
					const Window *mainwindow = FindWindowById(WC_MAIN_WINDOW, 0);
					/* code to allow the main window to 'follow' the vehicle if the ctrl key is pressed */
					if (_ctrl_pressed && mainwindow->viewport->zoom == ZOOM_LVL_NORMAL) {
						WP(mainwindow, vp_d).follow_vehicle = v->index;
					} else {
						ScrollMainWindowTo(v->x_pos, v->y_pos);
					}
				} break;

				case VVW_WIDGET_GOTO_DEPOT: /* goto hangar */
					DoCommandP(v->tile, v->index, _ctrl_pressed ? DEPOT_SERVICE : 0, NULL,
						_vehicle_command_translation_table[VCT_CMD_GOTO_DEPOT][v->type]);
					break;
				case VVW_WIDGET_REFIT_VEH: /* refit */
					ShowVehicleRefitWindow(v, INVALID_VEH_ORDER_ID);
					break;
				case VVW_WIDGET_SHOW_ORDERS: /* show orders */
					ShowOrdersWindow(v);
					break;
				case VVW_WIDGET_SHOW_DETAILS: /* show details */
					ShowVehicleDetailsWindow(v);
					break;
				case VVW_WIDGET_CLONE_VEH: /* clone vehicle */
					DoCommandP(v->tile, v->index, _ctrl_pressed ? 1 : 0, CcCloneVehicle,
										 _vehicle_command_translation_table[VCT_CMD_CLONE_VEH][v->type]);
					break;
				case VVW_WIDGET_TURN_AROUND: /* turn around */
					assert(v->type == VEH_TRAIN || v->type == VEH_ROAD);
					DoCommandP(v->tile, v->index, 0, NULL,
										 _vehicle_command_translation_table[VCT_CMD_TURN_AROUND][v->type]);
					break;
				case VVW_WIDGET_FORCE_PROCEED: /* force proceed */
					assert(v->type == VEH_TRAIN);
					DoCommandP(v->tile, v->index, 0, NULL, CMD_FORCE_TRAIN_PROCEED | CMD_MSG(STR_8862_CAN_T_MAKE_TRAIN_PASS_SIGNAL));
					break;
			}
		} break;

		case WE_RESIZE:
			w->viewport->width          += e->we.sizing.diff.x;
			w->viewport->height         += e->we.sizing.diff.y;
			w->viewport->virtual_width  += e->we.sizing.diff.x;
			w->viewport->virtual_height += e->we.sizing.diff.y;
			break;

		case WE_DESTROY:
			DeleteWindowById(WC_VEHICLE_ORDERS, w->window_number);
			DeleteWindowById(WC_VEHICLE_REFIT, w->window_number);
			DeleteWindowById(WC_VEHICLE_DETAILS, w->window_number);
			DeleteWindowById(WC_VEHICLE_TIMETABLE, w->window_number);
			break;

		case WE_MOUSELOOP: {
			const Vehicle *v = GetVehicle(w->window_number);
			bool veh_stopped = v->IsStoppedInDepot();

			/* Widget VVW_WIDGET_GOTO_DEPOT must be hidden if the vehicle is already
			 * stopped in depot.
			 * Widget VVW_WIDGET_CLONE_VEH should then be shown, since cloning is
			 * allowed only while in depot and stopped.
			 * This sytem allows to have two buttons, on top of each other.
			 * The same system applies to widget VVW_WIDGET_REFIT_VEH and VVW_WIDGET_TURN_AROUND.*/
			if (veh_stopped != w->IsWidgetHidden(VVW_WIDGET_GOTO_DEPOT) || veh_stopped == w->IsWidgetHidden(VVW_WIDGET_CLONE_VEH)) {
				w->SetWidgetHiddenState( VVW_WIDGET_GOTO_DEPOT, veh_stopped);  // send to depot
				w->SetWidgetHiddenState(VVW_WIDGET_CLONE_VEH, !veh_stopped); // clone
				if (v->type == VEH_ROAD || v->type == VEH_TRAIN) {
					w->SetWidgetHiddenState( VVW_WIDGET_REFIT_VEH, !veh_stopped); // refit
					w->SetWidgetHiddenState(VVW_WIDGET_TURN_AROUND, veh_stopped);  // force turn around
				}
				SetWindowDirty(w);
			}
		} break;
	}
}

void DrawVehicleImage(const Vehicle *v, int x, int y, VehicleID selection, int count, int skip)
{
	switch (v->type) {
		case VEH_TRAIN:    DrawTrainImage(v, x, y, selection, count, skip); break;
		case VEH_ROAD:     DrawRoadVehImage(v, x, y, selection, count);     break;
		case VEH_SHIP:     DrawShipImage(v, x, y, selection);               break;
		case VEH_AIRCRAFT: DrawAircraftImage(v, x, y, selection);           break;
		default: NOT_REACHED();
	}
}
