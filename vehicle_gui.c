/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "player.h"
#include "station.h"
#include "strings.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "vehicle.h"
#include "window.h"
#include "engine.h"
#include "gui.h"
#include "command.h"
#include "gfx.h"
#include "variables.h"
#include "vehicle_gui.h"
#include "viewport.h"
#include "train.h"
#include "newgrf_callbacks.h"
#include "newgrf_engine.h"
#include "newgrf_text.h"
#include "date.h"
#include "ship.h"
#include "aircraft.h"
#include "roadveh.h"
#include "depot.h"

typedef struct Sorting {
	Listing aircraft;
	Listing roadveh;
	Listing ship;
	Listing train;
} Sorting;

static Sorting _sorting;

typedef struct vehiclelist_d {
	const Vehicle** sort_list;  // List of vehicles (sorted)
	Listing *_sorting;          // pointer to the appropiate subcategory of _sorting
	uint16 length_of_sort_list; // Keeps track of how many vehicle pointers sort list got space for
	byte vehicle_type;          // The vehicle type that is sorted
	list_d l;                   // General list struct
} vehiclelist_d;
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(vehiclelist_d));

static bool   _internal_sort_order;     // descending/ascending

static RailType _railtype_selected_in_replace_gui;


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

static const StringID _vehicle_sort_listing[] = {
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

static const StringID _rail_types_list[] = {
	STR_RAIL_VEHICLES,
	STR_ELRAIL_VEHICLES,
	STR_MONORAIL_VEHICLES,
	STR_MAGLEV_VEHICLES,
	INVALID_STRING_ID
};

void RebuildVehicleLists(void)
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
		}
	}
}

void ResortVehicleLists(void)
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
		}
	}
}

static void BuildVehicleList(vehiclelist_d* vl, PlayerID owner, StationID station, OrderID order, uint16 depot_airport_index, uint16 window_type)
{
	if (!(vl->l.flags & VL_REBUILD)) return;

	DEBUG(misc, 1) ("Building vehicle list for player %d station %d...", owner, station);

	vl->l.list_length = GenerateVehicleSortList(&vl->sort_list, &vl->length_of_sort_list, vl->vehicle_type, owner, station, order, depot_airport_index, window_type);

	vl->l.flags &= ~VL_REBUILD;
	vl->l.flags |= VL_RESORT;
}

static void SortVehicleList(vehiclelist_d *vl)
{
	if (!(vl->l.flags & VL_RESORT)) return;

	_internal_sort_order = vl->l.flags & VL_DESC;
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

/* General Vehicle GUI based procedures that are independent of vehicle types */
void InitializeVehiclesGuiList(void)
{
	_railtype_selected_in_replace_gui = RAILTYPE_RAIL;
}

// draw the vehicle profit button in the vehicle list window.
void DrawVehicleProfitButton(const Vehicle *v, int x, int y)
{
	uint32 ormod;

	// draw profit-based colored icons
	if (v->age <= 365 * 2) {
		ormod = PALETTE_TO_GREY;
	} else if (v->profit_last_year < 0) {
		ormod = PALETTE_TO_RED;
	} else if (v->profit_last_year < 10000) {
		ormod = PALETTE_TO_YELLOW;
	} else {
		ormod = PALETTE_TO_GREEN;
	}
	DrawSprite(SPR_BLOT | ormod, x, y);
}

typedef struct RefitOption {
	CargoID cargo;
	byte subtype;
	uint16 value;
	EngineID engine;
} RefitOption;

typedef struct RefitList {
	uint num_lines;
	RefitOption *items;
} RefitList;

static RefitList *BuildRefitList(const Vehicle *v)
{
	uint max_lines = 256;
	RefitOption *refit = calloc(max_lines, sizeof(*refit));
	RefitList *list = calloc(1, sizeof(*list));
	Vehicle *u = (Vehicle*)v;
	uint num_lines = 0;
	uint i;

	do {
		CargoID cid;
		uint32 cmask = EngInfo(u->engine_type)->refit_mask;
		byte callbackmask = EngInfo(u->engine_type)->callbackmask;

		/* Skip this engine if it has no capacity */
		if (u->cargo_cap == 0) continue;

		/* Loop through all cargos in the refit mask */
		for (cid = 0; cmask != 0 && num_lines < max_lines; cmask >>= 1, cid++) {
			CargoID lcid;

			/* Skip cargo type if it's not listed */
			if (!HASBIT(cmask, 0)) continue;

			lcid = _local_cargo_id_ctype[cid];
			if (lcid == CT_INVALID) continue;

			/* Check the vehicle's callback mask for cargo suffixes */
			if (HASBIT(callbackmask, CBM_CARGO_SUFFIX)) {
				/* Make a note of the original cargo type. It has to be
				 * changed to test the cargo & subtype... */
				CargoID temp_cargo = u->cargo_type;
				byte temp_subtype  = u->cargo_subtype;
				byte refit_cyc;

				u->cargo_type = lcid;

				for (refit_cyc = 0; refit_cyc < 16 && num_lines < max_lines; refit_cyc++) {
					bool duplicate = false;
					uint16 callback;

					u->cargo_subtype = refit_cyc;
					callback = GetVehicleCallback(CBID_VEHICLE_CARGO_SUFFIX, 0, 0, u->engine_type, u);

					if (callback == 0xFF) callback = CALLBACK_FAILED;
					if (refit_cyc != 0 && callback == CALLBACK_FAILED) break;

					/* Check if this cargo and subtype combination are listed */
					for (i = 0; i < num_lines && !duplicate; i++) {
						if (refit[i].cargo == lcid && refit[i].value == callback) duplicate = true;
					}

					if (duplicate) continue;

					refit[num_lines].cargo   = lcid;
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
					if (refit[i].cargo == lcid && refit[i].value == CALLBACK_FAILED) duplicate = true;
				}

				if (!duplicate) {
					refit[num_lines].cargo   = lcid;
					refit[num_lines].subtype = 0;
					refit[num_lines].value   = CALLBACK_FAILED;
					refit[num_lines].engine  = INVALID_ENGINE;
					num_lines++;
				}
			}
		}
	} while (v->type == VEH_Train && (u = u->next) != NULL && num_lines < max_lines);

	list->num_lines = num_lines;
	list->items = refit;

	return list;
}

/** Draw the list of available refit options for a consist.
 * Draw the list and highlight the selected refit option (if any)
 * @param *v first vehicle in consist to get the refit-options of
 * @param sel selected refit cargo-type in the window
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
		byte colour = 16;
		if (sel == 0) {
			selected = &refit[i];
			colour = 12;
		}

		if (i >= pos && i < pos + rows) {
			/* Draw the cargo name */
			int last_x = DrawString(2, y, _cargoc.names_s[refit[i].cargo], colour);

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

			if (v->type == VEH_Train) {
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

			SetDParam(0, v->string_id);
			SetDParam(1, v->unitnumber);
			DrawWindowWidgets(w);

			WP(w,refit_d).cargo = DrawVehicleRefitWindow(WP(w, refit_d).list, WP(w, refit_d).sel, w->vscroll.pos, w->vscroll.cap, w->resize.step_height);

			if (WP(w,refit_d).cargo != NULL) {
				int32 cost = 0;
				switch (GetVehicle(w->window_number)->type) {
					case VEH_Train:    cost = CMD_REFIT_RAIL_VEHICLE; break;
					case VEH_Road:     cost = CMD_REFIT_ROAD_VEH;     break;
					case VEH_Ship:     cost = CMD_REFIT_SHIP;         break;
					case VEH_Aircraft: cost = CMD_REFIT_AIRCRAFT;     break;
				}

				cost = DoCommand(v->tile, v->index, WP(w,refit_d).cargo->cargo | WP(w,refit_d).cargo->subtype << 8, DC_QUERY_COST, cost);
				if (!CmdFailed(cost)) {
					SetDParam(0, WP(w,refit_d).cargo->cargo);
					SetDParam(1, _returned_refit_capacity);
					SetDParam(2, cost);
					DrawString(2, w->widget[5].top + 1, STR_9840_NEW_CAPACITY_COST_OF_REFIT, 0);
				}
			}
		}	break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case 3: { // listbox
					int y = e->we.click.pt.y - w->widget[3].top;
					if (y >= 0) {
						WP(w,refit_d).sel = (y / (int)w->resize.step_height) + w->vscroll.pos;
						SetWindowDirty(w);
					}
				} break;
				case 6: // refit button
					if (WP(w,refit_d).cargo != NULL) {
						const Vehicle *v = GetVehicle(w->window_number);

						if (WP(w, refit_d).order == INVALID_VEH_ORDER_ID) {
							int command = 0;

							switch (v->type) {
								case VEH_Train:    command = CMD_REFIT_RAIL_VEHICLE | CMD_MSG(STR_RAIL_CAN_T_REFIT_VEHICLE);  break;
								case VEH_Road:     command = CMD_REFIT_ROAD_VEH     | CMD_MSG(STR_REFIT_ROAD_VEHICLE_CAN_T);  break;
								case VEH_Ship:     command = CMD_REFIT_SHIP         | CMD_MSG(STR_9841_CAN_T_REFIT_SHIP);     break;
								case VEH_Aircraft: command = CMD_REFIT_AIRCRAFT     | CMD_MSG(STR_A042_CAN_T_REFIT_AIRCRAFT); break;
							}
							if (DoCommandP(v->tile, v->index, WP(w,refit_d).cargo->cargo | WP(w,refit_d).cargo->subtype << 8, NULL, command)) DeleteWindow(w);
						} else {
							if (DoCommandP(v->tile, v->index, WP(w,refit_d).cargo->cargo | WP(w,refit_d).cargo->subtype << 8 | WP(w, refit_d).order << 16, NULL, CMD_ORDER_REFIT)) DeleteWindow(w);
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
	WDP_AUTO, WDP_AUTO, 240, 174,
	WC_VEHICLE_REFIT,WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_vehicle_refit_widgets,
	VehicleRefitWndProc,
};

/** Show the refit window for a vehicle
* @param *v The vehicle to show the refit window for
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
		if (v->type == VEH_Train) WP(w, refit_d).length = CountVehiclesInChain(v);
		SetVScrollCount(w, WP(w, refit_d).list->num_lines);

		switch (v->type) {
			case VEH_Train:
				w->widget[3].tooltips = STR_RAIL_SELECT_TYPE_OF_CARGO_FOR;
				w->widget[6].data     = STR_RAIL_REFIT_VEHICLE;
				w->widget[6].tooltips = STR_RAIL_REFIT_TO_CARRY_HIGHLIGHTED;
				break;
			case VEH_Road:
				w->widget[3].tooltips = STR_ROAD_SELECT_TYPE_OF_CARGO_FOR;
				w->widget[6].data     = STR_REFIT_ROAD_VEHICLE;
				w->widget[6].tooltips = STR_REFIT_ROAD_VEHICLE_TO_CARRY_HIGHLIGHTED;
				break;
			case VEH_Ship:
				w->widget[3].tooltips = STR_983D_SELECT_TYPE_OF_CARGO_FOR;
				w->widget[6].data     = STR_983C_REFIT_SHIP;
				w->widget[6].tooltips = STR_983E_REFIT_SHIP_TO_CARRY_HIGHLIGHTED;
				break;
			case VEH_Aircraft:
				w->widget[3].tooltips = STR_A03E_SELECT_TYPE_OF_CARGO_FOR;
				w->widget[6].data     = STR_A03D_REFIT_AIRCRAFT;
				w->widget[6].tooltips = STR_A03F_REFIT_AIRCRAFT_TO_CARRY;
				break;
			default: NOT_REACHED();
		}
	}
}

/* Display additional text from NewGRF in the purchase information window */
uint ShowAdditionalText(int x, int y, uint w, EngineID engine)
{
	uint16 callback = GetVehicleCallback(CBID_VEHICLE_ADDITIONAL_TEXT, 0, 0, engine, NULL);
	if (callback == CALLBACK_FAILED) return 0;

	// STR_02BD is used to start the string with {BLACK}
	SetDParam(0, GetGRFStringID(GetEngineGRFID(engine), 0xD000 + callback));
	return DrawStringMultiLine(x, y, STR_02BD, w);
}

/* Count the number of bits that are set in a mask */
static uint CountBits(uint32 mask)
{
	uint c = 0;
	for (; mask != 0; mask >>= 1) if (HASBIT(mask, 0)) c++;
	return c;
}

/* Display list of cargo types of the engine, for the purchase information window */
uint ShowRefitOptionsList(int x, int y, uint w, EngineID engine)
{
	/* List of cargo types of this engine */
	uint32 cmask = EngInfo(engine)->refit_mask;
	/* List of cargo types available in this climate */
	uint32 lmask = _landscape_global_cargo_mask[_opt.landscape];
	char *b = _userstring;

	/* Draw nothing if the engine is not refittable */
	if (CountBits(cmask) <= 1) return 0;

	b = InlineString(b, STR_PURCHASE_INFO_REFITTABLE_TO);

	if (cmask == lmask) {
		/* Engine can be refitted to all types in this climate */
		b = InlineString(b, STR_PURCHASE_INFO_ALL_TYPES);
	} else {
		CargoID cid;

		/* Check if we are able to refit to more cargo types and unable to. If
		 * so, invert the cargo types to list those that we can't refit to. */
		if (CountBits(cmask ^ lmask) < CountBits(cmask)) {
			cmask ^= lmask;
			b = InlineString(b, STR_PURCHASE_INFO_ALL_BUT);
		}

		/* Add each cargo type to the list */
		for (cid = 0; cmask != 0; cmask >>= 1, cid++) {
			if (!HASBIT(cmask, 0)) continue;

			b = InlineString(b, _cargoc.names_s[_local_cargo_id_ctype[cid]]);
			if (cmask > 1) b = strecpy(b, ", ", lastof(_userstring));
		}
	}

	/* Terminate and display the completed string */
	*b = '\0';
	return DrawStringMultiLine(x, y, STR_SPEC_USERSTRING, w);
}


// if the sorting criteria had the same value, sort vehicle by unitnumber
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
	int r = va->profit_this_year - vb->profit_this_year;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleProfitLastYearSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	int r = va->profit_last_year - vb->profit_last_year;

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
	int i;

	memset(cargoa, 0, sizeof(cargoa));
	memset(cargob, 0, sizeof(cargob));
	for (v = va; v != NULL; v = v->next) cargoa[v->cargo_type] += v->cargo_cap;
	for (v = vb; v != NULL; v = v->next) cargob[v->cargo_type] += v->cargo_cap;

	for (i = 0; i < NUM_CARGO; i++) {
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

	if (va->type == VEH_Train && vb->type == VEH_Train) {
		do {
			if (RailVehInfo(ua->engine_type)->max_speed != 0)
				max_speed_a = min(max_speed_a, RailVehInfo(ua->engine_type)->max_speed);
		} while ((ua = ua->next) != NULL);

		do {
			if (RailVehInfo(ub->engine_type)->max_speed != 0)
				max_speed_b = min(max_speed_b, RailVehInfo(ub->engine_type)->max_speed);
		} while ((ub = ub->next) != NULL);

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
	int valuea = 0, valueb = 0;
	int r;

	for (u = va; u != NULL; u = u->next) valuea += u->value;
	for (u = vb; u != NULL; u = u->next) valueb += u->value;

	r = valuea - valueb;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

// this define is to match engine.c, but engine.c keeps it to itself
// ENGINE_AVAILABLE is used in ReplaceVehicleWndProc
#define ENGINE_AVAILABLE ((e->flags & 1 && HASBIT(info->climates, _opt.landscape)) || HASBIT(e->player_avail, _local_player))

/*  if show_outdated is selected, it do not sort psudo engines properly but it draws all engines
 * if used compined with show_cars set to false, it will work as intended. Replace window do it like that
 *  this was a big hack even before show_outdated was added. Stupid newgrf :p
 */
static void train_engine_drawing_loop(int *x, int *y, int *pos, int *sel, EngineID *selected_id, RailType railtype,
	uint8 lines_drawn, bool is_engine, bool show_cars, bool show_outdated, bool show_compatible)
{
	EngineID j;
	byte colour;
	const Player *p = GetPlayer(_local_player);

	for (j = 0; j < NUM_TRAIN_ENGINES; j++) {
		EngineID i = GetRailVehAtPosition(j);
		const Engine *e = GetEngine(i);
		const RailVehicleInfo *rvi = RailVehInfo(i);
		const EngineInfo *info = EngInfo(i);

		if (!EngineHasReplacementForPlayer(p, i) && p->num_engines[i] == 0 && show_outdated) continue;

		if ((rvi->power == 0 && !show_cars) || (rvi->power != 0 && show_cars))  // show wagons or engines (works since wagons do not have power)
			continue;

		if (*sel == 0) *selected_id = j;


		colour = *sel == 0 ? 0xC : 0x10;
		if (!(ENGINE_AVAILABLE && show_outdated && RailVehInfo(i)->power && IsCompatibleRail(e->railtype, railtype))) {
			if ((!IsCompatibleRail(e->railtype, railtype) && show_compatible)
				|| (e->railtype != railtype && !show_compatible)
				|| !(rvi->flags & RVI_WAGON) != is_engine ||
				!HASBIT(e->player_avail, _local_player))
				continue;
#if 0
		} else {
			// TODO find a nice red colour for vehicles being replaced
			if ( _autoreplace_array[i] != i )
				colour = *sel == 0 ? 0x44 : 0x45;
#endif
		}

		if (IS_INT_INSIDE(--*pos, -lines_drawn, 0)) {
			DrawString(*x + 59, *y + 2, GetCustomEngineName(i),
				colour);
			// show_outdated is true only for left side, which is where we show old replacements
			DrawTrainEngine(*x + 29, *y + 6, i, (p->num_engines[i] == 0 && show_outdated) ?
				PALETTE_CRASH : GetEnginePalette(i, _local_player));
			if ( show_outdated ) {
				SetDParam(0, p->num_engines[i]);
				DrawStringRightAligned(213, *y+5, STR_TINY_BLACK, 0);
			}
			*y += 14;
		}
		--*sel;
	}
}


static void SetupScrollStuffForReplaceWindow(Window *w)
{
	EngineID selected_id[2] = { INVALID_ENGINE, INVALID_ENGINE };
	const Player* p = GetPlayer(_local_player);
	uint sel[2];
	uint count = 0;
	uint count2 = 0;
	EngineID i;

	sel[0] = WP(w,replaceveh_d).sel_index[0];
	sel[1] = WP(w,replaceveh_d).sel_index[1];

	switch (WP(w,replaceveh_d).vehicletype) {
		case VEH_Train: {
			RailType railtype = _railtype_selected_in_replace_gui;

			w->widget[13].color = _player_colors[_local_player]; // sets the colour of that art thing
			w->widget[16].color = _player_colors[_local_player]; // sets the colour of that art thing

			for (i = 0; i < NUM_TRAIN_ENGINES; i++) {
				EngineID eid = GetRailVehAtPosition(i);
				const Engine* e = GetEngine(eid);
				const EngineInfo* info = EngInfo(eid);

				// left window contains compatible engines while right window only contains engines of the selected type
				if (ENGINE_AVAILABLE &&
						(RailVehInfo(eid)->power != 0) == (WP(w, replaceveh_d).wagon_btnstate != 0)) {
					if (IsCompatibleRail(e->railtype, railtype) && (p->num_engines[eid] > 0 || EngineHasReplacementForPlayer(p, eid))) {
						if (sel[0] == count) selected_id[0] = eid;
						count++;
					}
					if (e->railtype == railtype && HASBIT(e->player_avail, _local_player)) {
						if (sel[1] == count2) selected_id[1] = eid;
						count2++;
					}
				}
			}
			break;
		}

		case VEH_Road: {
			for (i = ROAD_ENGINES_INDEX; i < ROAD_ENGINES_INDEX + NUM_ROAD_ENGINES; i++) {
				if (p->num_engines[i] > 0 || EngineHasReplacementForPlayer(p, i)) {
					if (sel[0] == count) selected_id[0] = i;
					count++;
				}
			}

			if (selected_id[0] != INVALID_ENGINE) { // only draw right array if we have anything in the left one
				CargoID cargo = RoadVehInfo(selected_id[0])->cargo_type;

				for (i = ROAD_ENGINES_INDEX; i < ROAD_ENGINES_INDEX + NUM_ROAD_ENGINES; i++) {
					if (cargo == RoadVehInfo(i)->cargo_type &&
							HASBIT(GetEngine(i)->player_avail, _local_player)) {
						if (sel[1] == count2) selected_id[1] = i;
						count2++;
					}
				}
			}
			break;
		}

		case VEH_Ship: {
			for (i = SHIP_ENGINES_INDEX; i < SHIP_ENGINES_INDEX + NUM_SHIP_ENGINES; i++) {
				if (p->num_engines[i] > 0 || EngineHasReplacementForPlayer(p, i)) {
					if (sel[0] == count) selected_id[0] = i;
					count++;
				}
			}

			if (selected_id[0] != INVALID_ENGINE) {
				const ShipVehicleInfo* svi = ShipVehInfo(selected_id[0]);
				CargoID cargo = svi->cargo_type;
				byte refittable = svi->refittable;

				for (i = SHIP_ENGINES_INDEX; i < SHIP_ENGINES_INDEX + NUM_SHIP_ENGINES; i++) {
					if (HASBIT(GetEngine(i)->player_avail, _local_player) && (
								ShipVehInfo(i)->cargo_type == cargo ||
								ShipVehInfo(i)->refittable & refittable
							)) {
						if (sel[1] == count2) selected_id[1] = i;
						count2++;
					}
				}
			}
			break;
		}

		case VEH_Aircraft: {
			for (i = AIRCRAFT_ENGINES_INDEX; i < AIRCRAFT_ENGINES_INDEX + NUM_AIRCRAFT_ENGINES; i++) {
				if (p->num_engines[i] > 0 || EngineHasReplacementForPlayer(p, i)) {
					if (sel[0] == count) selected_id[0] = i;
					count++;
				}
			}

			if (selected_id[0] != INVALID_ENGINE) {
				byte subtype = AircraftVehInfo(selected_id[0])->subtype;

				for (i = AIRCRAFT_ENGINES_INDEX; i < AIRCRAFT_ENGINES_INDEX + NUM_AIRCRAFT_ENGINES; i++) {
					if (HASBIT(GetEngine(i)->player_avail, _local_player) &&
							(subtype & AIR_CTOL) == (AircraftVehInfo(i)->subtype & AIR_CTOL)) {
						if (sel[1] == count2) selected_id[1] = i;
						count2++;
					}
				}
			}
			break;
		}
	}
	// sets up the number of items in each list
	SetVScrollCount(w, count);
	SetVScroll2Count(w, count2);
	WP(w,replaceveh_d).sel_engine[0] = selected_id[0];
	WP(w,replaceveh_d).sel_engine[1] = selected_id[1];

	WP(w,replaceveh_d).count[0] = count;
	WP(w,replaceveh_d).count[1] = count2;
	return;
}


static void DrawEngineArrayInReplaceWindow(Window *w, int x, int y, int x2, int y2, int pos, int pos2,
	int sel1, int sel2, EngineID selected_id1, EngineID selected_id2)
{
	int sel[2];
	EngineID selected_id[2];
	const Player *p = GetPlayer(_local_player);

	sel[0] = sel1;
	sel[1] = sel2;

	selected_id[0] = selected_id1;
	selected_id[1] = selected_id2;

	switch (WP(w,replaceveh_d).vehicletype) {
		case VEH_Train: {
			RailType railtype = _railtype_selected_in_replace_gui;
			DrawString(157, w->widget[14].top + 1, _rail_types_list[railtype], 0x10);
			/* draw sorting criteria string */

			/* Ensure that custom engines which substituted wagons
			 * are sorted correctly.
			 * XXX - DO NOT EVER DO THIS EVER AGAIN! GRRR hacking in wagons as
			 * engines to get more types.. Stays here until we have our own format
			 * then it is exit!!! */
			if (WP(w,replaceveh_d).wagon_btnstate) {
				train_engine_drawing_loop(&x, &y, &pos, &sel[0], &selected_id[0], railtype, w->vscroll.cap, true, false, true, true); // True engines
				train_engine_drawing_loop(&x2, &y2, &pos2, &sel[1], &selected_id[1], railtype, w->vscroll.cap, true, false, false, false); // True engines
				train_engine_drawing_loop(&x2, &y2, &pos2, &sel[1], &selected_id[1], railtype, w->vscroll.cap, false, false, false, false); // Feeble wagons
			} else {
				train_engine_drawing_loop(&x, &y, &pos, &sel[0], &selected_id[0], railtype, w->vscroll.cap, false, true, true, true);
				train_engine_drawing_loop(&x2, &y2, &pos2, &sel[1], &selected_id[1], railtype, w->vscroll.cap, false, true, false, true);
			}
			break;
		}

		case VEH_Road: {
			int num = NUM_ROAD_ENGINES;
			const Engine* e = GetEngine(ROAD_ENGINES_INDEX);
			EngineID engine_id = ROAD_ENGINES_INDEX;

			do {
				if (p->num_engines[engine_id] > 0 || EngineHasReplacementForPlayer(p, engine_id)) {
					if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
						DrawString(x+59, y+2, GetCustomEngineName(engine_id), sel[0]==0 ? 0xC : 0x10);
						DrawRoadVehEngine(x+29, y+6, engine_id, p->num_engines[engine_id] > 0 ? GetEnginePalette(engine_id, _local_player) : PALETTE_CRASH);
						SetDParam(0, p->num_engines[engine_id]);
						DrawStringRightAligned(213, y+5, STR_TINY_BLACK, 0);
						y += 14;
					}
				sel[0]--;
				}

				if (selected_id[0] != INVALID_ENGINE) {
					byte cargo = RoadVehInfo(selected_id[0])->cargo_type;

					if (RoadVehInfo(engine_id)->cargo_type == cargo && HASBIT(e->player_avail, _local_player)) {
						if (IS_INT_INSIDE(--pos2, -w->vscroll.cap, 0) && RoadVehInfo(engine_id)->cargo_type == cargo) {
							DrawString(x2+59, y2+2, GetCustomEngineName(engine_id), sel[1]==0 ? 0xC : 0x10);
							DrawRoadVehEngine(x2+29, y2+6, engine_id, GetEnginePalette(engine_id, _local_player));
							y2 += 14;
						}
						sel[1]--;
					}
				}
			} while (++engine_id, ++e,--num);
			break;
		}

		case VEH_Ship: {
			int num = NUM_SHIP_ENGINES;
			const Engine* e = GetEngine(SHIP_ENGINES_INDEX);
			EngineID engine_id = SHIP_ENGINES_INDEX;

			do {
				if (p->num_engines[engine_id] > 0 || EngineHasReplacementForPlayer(p, engine_id)) {
					if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
						DrawString(x+75, y+7, GetCustomEngineName(engine_id), sel[0]==0 ? 0xC : 0x10);
						DrawShipEngine(x+35, y+10, engine_id, p->num_engines[engine_id] > 0 ? GetEnginePalette(engine_id, _local_player) : PALETTE_CRASH);
						SetDParam(0, p->num_engines[engine_id]);
						DrawStringRightAligned(213, y+15, STR_TINY_BLACK, 0);
						y += 24;
					}
					sel[0]--;
				}

				if (selected_id[0] != INVALID_ENGINE) {
					CargoID cargo = ShipVehInfo(selected_id[0])->cargo_type;
					bool refittable = ShipVehInfo(selected_id[0])->refittable;

					if (HASBIT(e->player_avail, _local_player) && ( cargo == ShipVehInfo(engine_id)->cargo_type || refittable & ShipVehInfo(engine_id)->refittable)) {
						if (IS_INT_INSIDE(--pos2, -w->vscroll.cap, 0)) {
							DrawString(x2+75, y2+7, GetCustomEngineName(engine_id), sel[1]==0 ? 0xC : 0x10);
							DrawShipEngine(x2+35, y2+10, engine_id, GetEnginePalette(engine_id, _local_player));
							y2 += 24;
						}
						sel[1]--;
					}
				}
			} while (++engine_id, ++e, --num);
			break;
		}   //end of ship

		case VEH_Aircraft: {
			int num = NUM_AIRCRAFT_ENGINES;
			const Engine* e = GetEngine(AIRCRAFT_ENGINES_INDEX);
			EngineID engine_id = AIRCRAFT_ENGINES_INDEX;

			do {
				if (p->num_engines[engine_id] > 0 || EngineHasReplacementForPlayer(p, engine_id)) {
					if (sel[0] == 0) selected_id[0] = engine_id;
					if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
						DrawString(x+62, y+7, GetCustomEngineName(engine_id), sel[0]==0 ? 0xC : 0x10);
						DrawAircraftEngine(x+29, y+10, engine_id, p->num_engines[engine_id] > 0 ? GetEnginePalette(engine_id, _local_player) : PALETTE_CRASH);
						SetDParam(0, p->num_engines[engine_id]);
						DrawStringRightAligned(213, y+15, STR_TINY_BLACK, 0);
						y += 24;
					}
					sel[0]--;
				}

				if (selected_id[0] != INVALID_ENGINE) {
					byte subtype = AircraftVehInfo(selected_id[0])->subtype;

					if ((subtype & AIR_CTOL) == (AircraftVehInfo(engine_id)->subtype & AIR_CTOL) &&
							HASBIT(e->player_avail, _local_player)) {
						if (sel[1] == 0) selected_id[1] = engine_id;
						if (IS_INT_INSIDE(--pos2, -w->vscroll.cap, 0)) {
							DrawString(x2+62, y2+7, GetCustomEngineName(engine_id), sel[1]==0 ? 0xC : 0x10);
							DrawAircraftEngine(x2+29, y2+10, engine_id, GetEnginePalette(engine_id, _local_player));
							y2 += 24;
						}
						sel[1]--;
					}
				}
			} while (++engine_id, ++e,--num);
			break;
		}   // end of aircraft
	}
}

static void DrawVehiclePurchaseInfo(const int x, const int y, uint w, const EngineID engine_number)
{
	switch (GetEngine(engine_number)->type) {
		case VEH_Train:
			if ((RailVehInfo(engine_number)->flags & RVI_WAGON) == 0) {
				DrawTrainEnginePurchaseInfo(x, y, w, engine_number);
			} else {
				DrawTrainWagonPurchaseInfo(x, y, w, engine_number);
			}
			break;

		case VEH_Road: DrawRoadVehPurchaseInfo(x, y, w, engine_number);      break;
		case VEH_Ship: DrawShipPurchaseInfo(x, y, w, engine_number);         break;
		case VEH_Aircraft: DrawAircraftPurchaseInfo(x, y, w, engine_number); break;
		default: NOT_REACHED();
	}
}

static void ReplaceVehicleWndProc(Window *w, WindowEvent *e)
{
	static const StringID _vehicle_type_names[] = {
		STR_019F_TRAIN,
		STR_019C_ROAD_VEHICLE,
		STR_019E_SHIP,
		STR_019D_AIRCRAFT
	};

	switch (e->event) {
		case WE_PAINT: {
				Player *p = GetPlayer(_local_player);
				int pos = w->vscroll.pos;
				EngineID selected_id[2] = { INVALID_ENGINE, INVALID_ENGINE };
				int x = 1;
				int y = 15;
				int pos2 = w->vscroll2.pos;
				int x2 = 1 + 228;
				int y2 = 15;
				int sel[2];
				byte i;
				sel[0] = WP(w,replaceveh_d).sel_index[0];
				sel[1] = WP(w,replaceveh_d).sel_index[1];

				SetupScrollStuffForReplaceWindow(w);

				selected_id[0] = WP(w,replaceveh_d).sel_engine[0];
				selected_id[1] = WP(w,replaceveh_d).sel_engine[1];

				// Disable the "Start Replacing" button if:
				//    Either list is empty
				// or Both lists have the same vehicle selected
				// or The selected replacement engine has a replacement (to prevent loops)
				// or The right list (new replacement) has the existing replacement vehicle selected
				SetWindowWidgetDisabledState(w, 4,
						selected_id[0] == INVALID_ENGINE ||
						selected_id[1] == INVALID_ENGINE ||
						selected_id[0] == selected_id[1] ||
						EngineReplacementForPlayer(p, selected_id[1]) != INVALID_ENGINE ||
						EngineReplacementForPlayer(p, selected_id[0]) == selected_id[1]);

				// Disable the "Stop Replacing" button if:
				//    The left list (existing vehicle) is empty
				// or The selected vehicle has no replacement set up
				SetWindowWidgetDisabledState(w, 6,
						selected_id[0] == INVALID_ENGINE ||
						!EngineHasReplacementForPlayer(p, selected_id[0]));

				// now the actual drawing of the window itself takes place
				SetDParam(0, _vehicle_type_names[WP(w, replaceveh_d).vehicletype - VEH_Train]);

				if (WP(w, replaceveh_d).vehicletype == VEH_Train) {
					// set on/off for renew_keep_length
					SetDParam(1, p->renew_keep_length ? STR_CONFIG_PATCHES_ON : STR_CONFIG_PATCHES_OFF);

					// set wagon/engine button
					SetDParam(2, WP(w, replaceveh_d).wagon_btnstate ? STR_ENGINES : STR_WAGONS);
				}

				DrawWindowWidgets(w);

				// sets up the string for the vehicle that is being replaced to
				if (selected_id[0] != INVALID_ENGINE) {
					if (!EngineHasReplacementForPlayer(p, selected_id[0])) {
						SetDParam(0, STR_NOT_REPLACING);
					} else {
						SetDParam(0, GetCustomEngineName(EngineReplacementForPlayer(p, selected_id[0])));
					}
				} else {
					SetDParam(0, STR_NOT_REPLACING_VEHICLE_SELECTED);
				}

				DrawString(145, w->widget[5].top + 1, STR_02BD, 0x10);

				/* now we draw the two arrays according to what we just counted */
				DrawEngineArrayInReplaceWindow(w, x, y, x2, y2, pos, pos2, sel[0], sel[1], selected_id[0], selected_id[1]);

				WP(w,replaceveh_d).sel_engine[0] = selected_id[0];
				WP(w,replaceveh_d).sel_engine[1] = selected_id[1];
				/* now we draw the info about the vehicles we selected */
				for (i = 0 ; i < 2 ; i++) {
					if (selected_id[i] != INVALID_ENGINE) {
						const Widget *wi = &w->widget[i == 0 ? 3 : 11];
						DrawVehiclePurchaseInfo(wi->left + 2 , wi->top + 1, wi->right - wi->left - 2, selected_id[i]);
					}
				}
			} break;   // end of paint

		case WE_CLICK: {
			// these 3 variables is used if any of the lists is clicked
			uint16 click_scroll_pos = w->vscroll2.pos;
			uint16 click_scroll_cap = w->vscroll2.cap;
			byte click_side = 1;

			switch (e->we.click.widget) {
				case 12:
					WP(w, replaceveh_d).wagon_btnstate = !(WP(w, replaceveh_d).wagon_btnstate);
					SetWindowDirty(w);
					break;

				case 14:
				case 15: /* Railtype selection dropdown menu */
					ShowDropDownMenu(w, _rail_types_list, _railtype_selected_in_replace_gui, 15, 0, ~GetPlayer(_local_player)->avail_railtypes);
					break;

				case 17: /* toggle renew_keep_length */
					DoCommandP(0, 5, GetPlayer(_local_player)->renew_keep_length ? 0 : 1, NULL, CMD_SET_AUTOREPLACE);
					break;

				case 4: { /* Start replacing */
					EngineID veh_from = WP(w, replaceveh_d).sel_engine[0];
					EngineID veh_to = WP(w, replaceveh_d).sel_engine[1];
					DoCommandP(0, 3, veh_from + (veh_to << 16), NULL, CMD_SET_AUTOREPLACE);
					break;
				}

				case 6: { /* Stop replacing */
					EngineID veh_from = WP(w, replaceveh_d).sel_engine[0];
					DoCommandP(0, 3, veh_from + (INVALID_ENGINE << 16), NULL, CMD_SET_AUTOREPLACE);
					break;
				}

				case 7:
					// sets up that the left one was clicked. The default values are for the right one (9)
					// this way, the code for 9 handles both sides
					click_scroll_pos = w->vscroll.pos;
					click_scroll_cap = w->vscroll.cap;
					click_side = 0;
					/* FALL THROUGH */

				case 9: {
					uint i = (e->we.click.pt.y - 14) / w->resize.step_height;
					if (i < click_scroll_cap) {
						WP(w,replaceveh_d).sel_index[click_side] = i + click_scroll_pos;
						SetWindowDirty(w);
					}
					break;
				}
			}
			break;
		}

		case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
			_railtype_selected_in_replace_gui = e->we.dropdown.index;
			/* Reset scrollbar positions */
			w->vscroll.pos  = 0;
			w->vscroll2.pos = 0;
			SetWindowDirty(w);
			break;

		case WE_RESIZE:
			w->vscroll.cap  += e->we.sizing.diff.y / (int)w->resize.step_height;
			w->vscroll2.cap += e->we.sizing.diff.y / (int)w->resize.step_height;

			w->widget[7].data = (w->vscroll.cap  << 8) + 1;
			w->widget[9].data = (w->vscroll2.cap << 8) + 1;
			break;
	}
}

static const Widget _replace_rail_vehicle_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,       STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   443,     0,    13, STR_REPLACE_VEHICLES_WHITE, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   444,   455,     0,    13, STR_NULL,       STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,     0,   227,   126,   227, 0x0,            STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   240,   251, STR_REPLACE_VEHICLES_START, STR_REPLACE_HELP_START_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,   139,   316,   228,   239, 0x0,            STR_REPLACE_HELP_REPLACE_INFO_TAB},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   306,   443,   240,   251, STR_REPLACE_VEHICLES_STOP,  STR_REPLACE_HELP_STOP_BUTTON},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   215,    14,   125, 0x801,          STR_REPLACE_HELP_LEFT_ARRAY},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   216,   227,    14,   125, STR_NULL,       STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,   228,   443,    14,   125, 0x801,          STR_REPLACE_HELP_RIGHT_ARRAY},
{ WWT_SCROLL2BAR, RESIZE_BOTTOM,    14,   444,   455,    14,   125, STR_NULL,       STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    14,   228,   455,   126,   227, 0x0,            STR_NULL},
// train specific stuff
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   228,   239, STR_REPLACE_ENGINE_WAGON_SELECT,       STR_REPLACE_ENGINE_WAGON_SELECT_HELP},  // widget 12
{      WWT_PANEL,     RESIZE_TB,    14,   139,   153,   240,   251, 0x0,            STR_NULL},
{      WWT_PANEL,     RESIZE_TB,    14,   154,   277,   240,   251, 0x0,            STR_REPLACE_HELP_RAILTYPE},
{    WWT_TEXTBTN,     RESIZE_TB,    14,   278,   289,   240,   251, STR_0225,       STR_REPLACE_HELP_RAILTYPE},
{      WWT_PANEL,     RESIZE_TB,    14,   290,   305,   240,   251, 0x0,            STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   317,   455,   228,   239, STR_REPLACE_REMOVE_WAGON,       STR_REPLACE_REMOVE_WAGON_HELP},
// end of train specific stuff
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   444,   455,   240,   251, STR_NULL,       STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const Widget _replace_road_vehicle_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                    STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   443,     0,    13, STR_REPLACE_VEHICLES_WHITE,  STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   444,   455,     0,    13, STR_NULL,                    STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,     0,   227,   126,   217, 0x0,                         STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   218,   229, STR_REPLACE_VEHICLES_START,  STR_REPLACE_HELP_START_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,   139,   305,   218,   229, 0x0,                         STR_REPLACE_HELP_REPLACE_INFO_TAB},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   306,   443,   218,   229, STR_REPLACE_VEHICLES_STOP,   STR_REPLACE_HELP_STOP_BUTTON},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   215,    14,   125, 0x801,                       STR_REPLACE_HELP_LEFT_ARRAY},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   216,   227,    14,   125, STR_NULL,                    STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,   228,   443,    14,   125, 0x801,                       STR_REPLACE_HELP_RIGHT_ARRAY},
{ WWT_SCROLL2BAR, RESIZE_BOTTOM,    14,   444,   455,    14,   125, STR_NULL,                    STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    14,   228,   455,   126,   217, 0x0,                         STR_NULL},
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   444,   455,   218,   229, STR_NULL,                    STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const Widget _replace_ship_aircraft_vehicle_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                    STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   443,     0,    13, STR_REPLACE_VEHICLES_WHITE,  STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   444,   455,     0,    13, STR_NULL,                    STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,     0,   227,   110,   201, 0x0,                         STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   202,   213, STR_REPLACE_VEHICLES_START,  STR_REPLACE_HELP_START_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,   139,   305,   202,   213, 0x0,                         STR_REPLACE_HELP_REPLACE_INFO_TAB},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   306,   443,   202,   213, STR_REPLACE_VEHICLES_STOP,   STR_REPLACE_HELP_STOP_BUTTON},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   215,    14,   109, 0x401,                       STR_REPLACE_HELP_LEFT_ARRAY},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   216,   227,    14,   109, STR_NULL,                    STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,   228,   443,    14,   109, 0x401,                       STR_REPLACE_HELP_RIGHT_ARRAY},
{ WWT_SCROLL2BAR, RESIZE_BOTTOM,    14,   444,   455,    14,   109, STR_NULL,                    STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    14,   228,   455,   110,   201, 0x0,                         STR_NULL},
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   444,   455,   202,   213, STR_NULL,                    STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const WindowDesc _replace_rail_vehicle_desc = {
	WDP_AUTO, WDP_AUTO, 456, 252,
	WC_REPLACE_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_replace_rail_vehicle_widgets,
	ReplaceVehicleWndProc
};

static const WindowDesc _replace_road_vehicle_desc = {
	WDP_AUTO, WDP_AUTO, 456, 230,
	WC_REPLACE_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_replace_road_vehicle_widgets,
	ReplaceVehicleWndProc
};

static const WindowDesc _replace_ship_aircraft_vehicle_desc = {
	WDP_AUTO, WDP_AUTO, 456, 214,
	WC_REPLACE_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_replace_ship_aircraft_vehicle_widgets,
	ReplaceVehicleWndProc
};


static void ShowReplaceVehicleWindow(byte vehicletype)
{
	Window *w;

	DeleteWindowById(WC_REPLACE_VEHICLE, vehicletype);

	switch (vehicletype) {
		case VEH_Train:
			w = AllocateWindowDescFront(&_replace_rail_vehicle_desc, vehicletype);
			w->vscroll.cap  = 8;
			w->resize.step_height = 14;
			WP(w, replaceveh_d).wagon_btnstate = true;
			break;
		case VEH_Road:
			w = AllocateWindowDescFront(&_replace_road_vehicle_desc, vehicletype);
			w->vscroll.cap  = 8;
			w->resize.step_height = 14;
			break;
		case VEH_Ship:
		case VEH_Aircraft:
			w = AllocateWindowDescFront(&_replace_ship_aircraft_vehicle_desc, vehicletype);
			w->vscroll.cap  = 4;
			w->resize.step_height = 24;
			break;
		default: return;
	}

	w->caption_color = _local_player;
	WP(w, replaceveh_d).vehicletype = vehicletype;
	w->vscroll2.cap = w->vscroll.cap;   // these two are always the same
}

void InitializeGUI(void)
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

/*
 * Start of functions regarding vehicle list windows
 */

enum {
	PLY_WND_PRC__OFFSET_TOP_WIDGET = 26,
	PLY_WND_PRC__SIZE_OF_ROW_SMALL = 26,
	PLY_WND_PRC__SIZE_OF_ROW_BIG   = 36,
};

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
	VLW_WIDGET_NEW_VEHICLES,
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
	{      WWT_PANEL,    RESIZE_RTB,    14,     0,   259,   170,   181, 0x0,                  STR_NULL},

	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   105,   170,   181, 0x0,                  STR_AVAILABLE_ENGINES_TIP},
	{    WWT_TEXTBTN,     RESIZE_TB,    14,   106,   211,   170,   181, STR_MANAGE_LIST,      STR_SORT_CRITERIA_TIP},
	{    WWT_TEXTBTN,     RESIZE_TB,    14,   212,   223,   170,   181, STR_0225,             STR_SORT_CRITERIA_TIP},

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
	PlayerID player = GB(w->window_number, 0, 8);

	vl->vehicle_type = GB(w->window_number, 11, 5);
	vl->length_of_sort_list = 0;
	vl->sort_list = NULL;
	w->caption_color = player;

	/* Hide the widgets that we will not use in this window
	 * Some windows contains actions only fit for the owner */
	if (player == _local_player) {
		HideWindowWidget(w, VLW_WIDGET_OTHER_PLAYER_FILLER);
		SetWindowWidgetDisabledState(w, VLW_WIDGET_NEW_VEHICLES, window_type != VLW_STANDARD);
	} else {
		SetWindowWidgetsHiddenState(w, true,
			VLW_WIDGET_NEW_VEHICLES,
			VLW_WIDGET_MANAGE_VEHICLES,
			VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN,
			VLW_WIDGET_STOP_ALL,
			VLW_WIDGET_START_ALL,
			VLW_WIDGET_EMPTY_BOTTOM_RIGHT,
			WIDGET_LIST_END);
	}

	/* Set up the window widgets */
	switch (vl->vehicle_type) {
		case VEH_Train:
			w->widget[VLW_WIDGET_LIST].tooltips          = STR_883D_TRAINS_CLICK_ON_TRAIN_FOR;
			w->widget[VLW_WIDGET_NEW_VEHICLES].data = STR_8815_NEW_VEHICLES;
			break;

		case VEH_Road:
			w->widget[VLW_WIDGET_LIST].tooltips          = STR_901A_ROAD_VEHICLES_CLICK_ON;
			w->widget[VLW_WIDGET_NEW_VEHICLES].data = STR_9004_NEW_VEHICLES;
			break;

		case VEH_Ship:
			w->widget[VLW_WIDGET_LIST].tooltips          = STR_9823_SHIPS_CLICK_ON_SHIP_FOR;
			w->widget[VLW_WIDGET_NEW_VEHICLES].data = STR_9804_NEW_SHIPS;
			break;

		case VEH_Aircraft:
			w->widget[VLW_WIDGET_CAPTION].data           = STR_A009_AIRCRAFT;
			w->widget[VLW_WIDGET_LIST].tooltips          = STR_A01F_AIRCRAFT_CLICK_ON_AIRCRAFT;
			w->widget[VLW_WIDGET_NEW_VEHICLES].data = STR_A003_NEW_AIRCRAFT;
			break;

		default: NOT_REACHED();
	}

	switch (window_type) {
		case VLW_SHARED_ORDERS:
			w->widget[VLW_WIDGET_CAPTION].data  = STR_VEH_WITH_SHARED_ORDERS_LIST;
			break;
		case VLW_STANDARD: /* Company Name - standard widget setup */
			switch (vl->vehicle_type) {
				case VEH_Train:    w->widget[VLW_WIDGET_CAPTION].data = STR_881B_TRAINS;        break;
				case VEH_Road:     w->widget[VLW_WIDGET_CAPTION].data = STR_9001_ROAD_VEHICLES; break;
				case VEH_Ship:     w->widget[VLW_WIDGET_CAPTION].data = STR_9805_SHIPS;         break;
				case VEH_Aircraft: w->widget[VLW_WIDGET_CAPTION].data = STR_A009_AIRCRAFT;      break;
				default: NOT_REACHED(); break;
			}
			break;
		case VLW_STATION_LIST: /* Station Name */
			switch (vl->vehicle_type) {
				case VEH_Train:    w->widget[VLW_WIDGET_CAPTION].data = STR_SCHEDULED_TRAINS;        break;
				case VEH_Road:     w->widget[VLW_WIDGET_CAPTION].data = STR_SCHEDULED_ROAD_VEHICLES; break;
				case VEH_Ship:     w->widget[VLW_WIDGET_CAPTION].data = STR_SCHEDULED_SHIPS;         break;
				case VEH_Aircraft: w->widget[VLW_WIDGET_CAPTION].data = STR_SCHEDULED_AIRCRAFT;      break;
				default: NOT_REACHED(); break;
			}
			break;

		case VLW_DEPOT_LIST:
			switch (vl->vehicle_type) {
				case VEH_Train:    w->widget[VLW_WIDGET_CAPTION].data = STR_VEHICLE_LIST_TRAIN_DEPOT;    break;
				case VEH_Road:     w->widget[VLW_WIDGET_CAPTION].data = STR_VEHICLE_LIST_ROADVEH_DEPOT;  break;
				case VEH_Ship:     w->widget[VLW_WIDGET_CAPTION].data = STR_VEHICLE_LIST_SHIP_DEPOT;     break;
				case VEH_Aircraft: w->widget[VLW_WIDGET_CAPTION].data = STR_VEHICLE_LIST_AIRCRAFT_DEPOT; break;
				default: NOT_REACHED(); break;
			}
			break;
		default: NOT_REACHED(); break;
	}

	switch (vl->vehicle_type) {
		case VEH_Train:
			w->resize.step_width = 1;
			/* Fallthrough */
		case VEH_Road:
			w->vscroll.cap = 7;
			w->resize.step_height = PLY_WND_PRC__SIZE_OF_ROW_SMALL;
			w->resize.height = 220 - (PLY_WND_PRC__SIZE_OF_ROW_SMALL * 3); // Minimum of 4 vehicles
			break;
		case VEH_Ship:
		case VEH_Aircraft:
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
		case VEH_Train:    vl->_sorting = &_sorting.train; break;
		case VEH_Road:     vl->_sorting = &_sorting.roadveh; break;
		case VEH_Ship:     vl->_sorting = &_sorting.ship; break;
		case VEH_Aircraft: vl->_sorting = &_sorting.aircraft; break;
		default: NOT_REACHED(); break;
	}

	vl->l.flags = VL_REBUILD | (vl->_sorting->order << (VL_DESC - 1));
	vl->l.sort_type = vl->_sorting->criteria;
	vl->sort_list = NULL;
	vl->l.resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;	// Set up resort timer
}

static void DrawSmallOrderList(const Vehicle *v, int x, int y)
{
	const Order *order;
	int sel, i = 0;

	sel = v->cur_order_index;

	FOR_VEHICLE_ORDERS(v, order) {
		if (sel == 0) DrawString(x - 6, y, STR_SMALL_RIGHT_ARROW, 16);
		sel--;

		if (order->type == OT_GOTO_STATION) {
			if (v->type == VEH_Ship && IsBuoy(GetStation(order->dest))) continue;

			SetDParam(0, order->dest);
			DrawString(x, y, STR_A036, 0);

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
	const StationID station          = (window_type == VLW_STATION_LIST)  ? GB(w->window_number, 16, 16) : INVALID_STATION;
	const OrderID order              = (window_type == VLW_SHARED_ORDERS) ? GB(w->window_number, 16, 16) : INVALID_ORDER;
	const uint16 depot_airport_index = (window_type == VLW_DEPOT_LIST)    ? GB(w->window_number, 16, 16) : INVALID_STATION;

	BuildVehicleList(vl, owner, station, order, depot_airport_index, window_type);
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
			SetDParam(0, p->name_1);
			SetDParam(1, p->name_2);
			SetDParam(2, w->vscroll.count);
			break;

		case VLW_STATION_LIST: /* Station Name */
			SetDParam(0, station);
			SetDParam(1, w->vscroll.count);
			break;

		case VLW_DEPOT_LIST:
			switch (vl->vehicle_type) {
				case VEH_Train:    SetDParam(0, STR_8800_TRAIN_DEPOT);        break;
				case VEH_Road:     SetDParam(0, STR_9003_ROAD_VEHICLE_DEPOT); break;
				case VEH_Ship:     SetDParam(0, STR_9803_SHIP_DEPOT);         break;
				case VEH_Aircraft: SetDParam(0, STR_A002_AIRCRAFT_HANGAR);    break;
				default: NOT_REACHED(); break;
			}
			if (vl->vehicle_type == VEH_Aircraft) {
				SetDParam(1, depot_airport_index); // Airport name
			} else {
				SetDParam(1, GetDepot(depot_airport_index)->town_index);
			}
			SetDParam(2, w->vscroll.count);
			break;
		default: NOT_REACHED(); break;
	}

	SetWindowWidgetsDisabledState(w, vl->l.list_length == 0,
		VLW_WIDGET_MANAGE_VEHICLES,
		VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN,
		VLW_WIDGET_STOP_ALL,
		VLW_WIDGET_START_ALL,
		WIDGET_LIST_END);

	DrawWindowWidgets(w);

	/* draw sorting criteria string */
	DrawString(85, 15, _vehicle_sort_listing[vl->l.sort_type], 0x10);
	/* draw arrow pointing up/down for ascending/descending sorting */
	DoDrawString(vl->l.flags & VL_DESC ? DOWNARROW : UPARROW, 69, 15, 0x10);

	max = min(w->vscroll.pos + w->vscroll.cap, vl->l.list_length);
	for (i = w->vscroll.pos; i < max; ++i) {
		const Vehicle *v = vl->sort_list[i];
		StringID str;

		SetDParam(0, v->profit_this_year);
		SetDParam(1, v->profit_last_year);

		DrawVehicleImage(v, x + 19, y + 6, w->widget[VLW_WIDGET_LIST].right - w->widget[VLW_WIDGET_LIST].left - 20, 0, INVALID_VEHICLE);
		DrawString(x + 19, y + w->resize.step_height - 8, STR_0198_PROFIT_THIS_YEAR_LAST_YEAR, 0);

		if ((v->type == VEH_Train    && v->string_id != STR_SV_TRAIN_NAME)   ||
			(v->type == VEH_Road     && v->string_id != STR_SV_ROADVEH_NAME) ||
			(v->type == VEH_Ship     && v->string_id != STR_SV_SHIP_NAME)    ||
			(v->type == VEH_Aircraft && v->string_id != STR_SV_AIRCRAFT_NAME)) {

			/* The vehicle got a name so we will print it */
			SetDParam(0, v->string_id);
			DrawString(x + 19, y, STR_01AB, 0);
		}

		if (w->resize.step_height == PLY_WND_PRC__SIZE_OF_ROW_BIG) DrawSmallOrderList(v, x + 138, y);

		if (IsVehicleInDepot(v)) {
			str = STR_021F;
		} else {
			str = (v->age > v->max_age - 366) ? STR_00E3 : STR_00E2;
		}

		SetDParam(0, v->unitnumber);
		DrawString(x, y + 2, str, 0);

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

					switch (vl->vehicle_type) {
						case VEH_Train: ShowTrainViewWindow(v); break;
						case VEH_Road: ShowRoadVehViewWindow(v); break;
						case VEH_Ship: ShowShipViewWindow(v); break;
						case VEH_Aircraft: ShowAircraftViewWindow(v); break;
						default: NOT_REACHED(); break;
					}
				} break;

				case VLW_WIDGET_NEW_VEHICLES:
					switch (vl->vehicle_type) {
						case VEH_Train: ShowBuildTrainWindow(0); break;
						case VEH_Road:  ShowBuildRoadVehWindow(0); break;
						case VEH_Ship:  ShowBuildShipWindow(0); break;
						case VEH_Aircraft: ShowBuildVehicleWindow(0, vl->vehicle_type); break;
					}
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
					action_str[2] = depot_name[vl->vehicle_type - VEH_Train];
					ShowDropDownMenu(w, action_str, 0, VLW_WIDGET_MANAGE_VEHICLES_DROPDOWN, 0, 0);
					break;
				}

				case VLW_WIDGET_STOP_ALL:
				case VLW_WIDGET_START_ALL:
					DoCommandP(0, GB(w->window_number, 16, 16), (w->window_number & VLW_MASK) | (1 << 6) | (e->we.click.widget == VLW_WIDGET_START_ALL ? (1 << 5) : 0) | vl->vehicle_type, NULL, CMD_MASS_START_STOP);
					break;
			}
		}	break;

		case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
			switch (e->we.dropdown.button) {
				case VLW_WIDGET_SORT_BY_PULLDOWN:
					if (vl->l.sort_type != e->we.dropdown.index) {
						// value has changed -> resort
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
								CMD_SEND_TO_DEPOT(vl->vehicle_type));
							break;
						case 2: /* Send to Depots */
							DoCommandP(0, GB(w->window_number, 16, 16) /* StationID or OrderID (depending on VLW) */,
								(w->window_number & VLW_MASK) | DEPOT_MASS_SEND,
								NULL,
								CMD_SEND_TO_DEPOT(vl->vehicle_type));
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
			if (--vl->l.resort_timer == 0) {
				StationID station = ((w->window_number & VLW_MASK) == VLW_STATION_LIST) ? GB(w->window_number, 16, 16) : INVALID_STATION;
				PlayerID owner = (PlayerID)w->caption_color;

				DEBUG(misc, 1) ("Periodic resort %d list player %d station %d", vl->vehicle_type, owner, station);
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
	WDP_AUTO, WDP_AUTO, 260, 182,
	WC_TRAINS_LIST, 0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_vehicle_list_widgets,
	PlayerVehWndProc
};

static const WindowDesc _player_vehicle_list_road_veh_desc = {
	WDP_AUTO, WDP_AUTO, 260, 182,
	WC_ROADVEH_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_vehicle_list_widgets,
	PlayerVehWndProc
};

static const WindowDesc _player_vehicle_list_ship_desc = {
	WDP_AUTO, WDP_AUTO, 260, 182,
	WC_SHIPS_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_vehicle_list_widgets,
	PlayerVehWndProc
};

static const WindowDesc _player_vehicle_list_aircraft_desc = {
	WDP_AUTO, WDP_AUTO, 260, 182,
	WC_AIRCRAFT_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_vehicle_list_widgets,
	PlayerVehWndProc
};

static void ShowVehicleListWindowLocal(PlayerID player, byte vehicle_type, StationID station, OrderID order, uint16 depot_airport_index)
{
	Window *w;
	WindowNumber num;

	if (!IsValidPlayer(player)) return;

	num = (vehicle_type << 11) | player;
	if (order != INVALID_ORDER) {
		num |= (order << 16) | VLW_SHARED_ORDERS;
	} else if (depot_airport_index != INVALID_STATION) {
		num |= (depot_airport_index << 16) | VLW_DEPOT_LIST;
	} else if (station == INVALID_STATION) {
		num |= VLW_STANDARD;
	} else {
		num |= (station << 16) | VLW_STATION_LIST;
	}

	/* The vehicle list windows have been unified. Just some strings need
	 * to be changed which happens in the WE_CREATE event and resizing
	 * some of the windows to the correct size */
	switch (vehicle_type) {
		default: NOT_REACHED();
		case VEH_Train:
			w = AllocateWindowDescFront(&_player_vehicle_list_train_desc, num);
			if (w != NULL) ResizeWindow(w, 65, 38);
			break;
		case VEH_Road:
			w = AllocateWindowDescFront(&_player_vehicle_list_road_veh_desc, num);
			if (w != NULL) ResizeWindow(w, 0, 38);
			break;
		case VEH_Ship:
			w = AllocateWindowDescFront(&_player_vehicle_list_ship_desc, num);
			break;
		case VEH_Aircraft:
			w = AllocateWindowDescFront(&_player_vehicle_list_aircraft_desc, num);
			break;
	}

	if (w != NULL) {
		/* Set the minimum window size to the current window size */
		w->resize.width = w->width;
		w->resize.height = w->height;
	}
}

void ShowVehicleListWindow(PlayerID player, StationID station, byte vehicle_type)
{
	ShowVehicleListWindowLocal(player, vehicle_type, station, INVALID_ORDER, INVALID_STATION);
}

void ShowVehWithSharedOrders(Vehicle *v, byte vehicle_type)
{
	if (v->orders == NULL) return; // no shared list to show
	ShowVehicleListWindowLocal(v->owner, vehicle_type, INVALID_STATION, v->orders->index, INVALID_STATION);
}

void ShowVehDepotOrders(PlayerID player, byte vehicle_type, TileIndex depot_tile)
{
	uint16 depot_airport_index;

	if (vehicle_type == VEH_Aircraft) {
		depot_airport_index = GetStationIndex(depot_tile);
	} else {
		Depot *depot = GetDepotByTile(depot_tile);
		if (depot == NULL) return; // no depot to show
		depot_airport_index = depot->index;
	}
	ShowVehicleListWindowLocal(player, vehicle_type, INVALID_STATION, INVALID_ORDER, depot_airport_index);
}
