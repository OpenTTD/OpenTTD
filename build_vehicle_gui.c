/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "aircraft.h"
#include "debug.h"
#include "functions.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "window.h"
#include "gui.h"
#include "vehicle.h"
#include "gfx.h"
#include "station.h"
#include "command.h"
#include "engine.h"
#include "player.h"
#include "depot.h"
#include "airport.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "date.h"
#include "strings.h"


typedef enum BuildVehicleWidgets {
	BUILD_VEHICLE_WIDGET_CLOSEBOX = 0,
	BUILD_VEHICLE_WIDGET_CAPTION,
	BUILD_VEHICLE_WIDGET_SORT_ASSENDING_DESCENDING,
	BUILD_VEHICLE_WIDGET_SORT_TEXT,
	BUILD_VEHICLE_WIDGET_SORT_DROPDOWN,
	BUILD_VEHICLE_WIDGET_LIST,
	BUILD_VEHICLE_WIDGET_SCROLLBAR,
	BUILD_VEHICLE_WIDGET_PANEL,
	BUILD_VEHICLE_WIDGET_BUILD,
	BUILD_VEHICLE_WIDGET_RENAME,
	BUILD_VEHICLE_WIDGET_RESIZE,
} BuildVehicleWidget;

static const Widget _build_vehicle_widgets[] = {
	{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW },
	{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   239,     0,    13, STR_A005_NEW_AIRCRAFT,   STR_018C_WINDOW_TITLE_DRAG_THIS },
	{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    80,    14,    25, STR_SORT_BY,             STR_SORT_ORDER_TIP},
	{      WWT_PANEL,   RESIZE_NONE,    14,    81,   227,    14,    25, 0x0,                     STR_SORT_CRITERIA_TIP},
	{    WWT_TEXTBTN,   RESIZE_NONE,    14,   228,   239,    14,    25, STR_0225,                STR_SORT_CRITERIA_TIP},
	{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   227,    26,   121, 0x401,                   STR_A025_AIRCRAFT_SELECTION_LIST },
	{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   228,   239,    26,   121, 0x0,                     STR_0190_SCROLL_BAR_SCROLLS_LIST },
	{      WWT_PANEL,     RESIZE_TB,    14,     0,   239,   122,   213, 0x0,                     STR_NULL },

	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   114,   214,   225, STR_A006_BUILD_AIRCRAFT, STR_A026_BUILD_THE_HIGHLIGHTED_AIRCRAFT },
	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   115,   227,   214,   225, STR_A037_RENAME,         STR_A038_RENAME_AIRCRAFT_TYPE },
	{  WWT_RESIZEBOX,     RESIZE_TB,    14,   228,   239,   214,   225, 0x0,                     STR_RESIZE_BUTTON },
	{   WIDGETS_END},
};

static bool _internal_sort_order; // descending/ascending
static byte _last_sort_criteria = 0;
static bool _last_sort_order = false;

static int CDECL EngineNumberSorter(const void *a, const void *b)
{
	const EngineID va = *(const EngineID*)a;
	const EngineID vb = *(const EngineID*)b;
	int r = va - vb;

	return _internal_sort_order ? -r : r;
}

static int CDECL EngineIntroDateSorter(const void *a, const void *b)
{
	const int va = GetEngine(*(const EngineID*)a)->intro_date;
	const int vb = GetEngine(*(const EngineID*)b)->intro_date;
	const int r = va - vb;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return EngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

static int CDECL EngineNameSorter(const void *a, const void *b)
{
	static EngineID last_engine[2] = { INVALID_ENGINE, INVALID_ENGINE };
	static char     last_name[2][64] = { "\0", "\0" };

	const EngineID va = *(const EngineID*)a;
	const EngineID vb = *(const EngineID*)b;
	int r;

	if (va != last_engine[0]) {
		last_engine[0] = va;
		GetString(last_name[0], GetCustomEngineName(va), lastof(last_name[0]));
	}

	if (vb != last_engine[1]) {
		last_engine[1] = vb;
		GetString(last_name[1], GetCustomEngineName(vb), lastof(last_name[1]));
	}

	r = strcmp(last_name[0], last_name[1]); // sort by name

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return EngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

static int CDECL EngineReliabilitySorter(const void *a, const void *b)
{
	const int va = GetEngine(*(const EngineID*)a)->reliability;
	const int vb = GetEngine(*(const EngineID*)b)->reliability;
	const int r = va - vb;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return EngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

/* Aircraft sorting functions */

static int CDECL AircraftEngineCostSorter(const void *a, const void *b)
{
	const int va = AircraftVehInfo(*(const EngineID*)a)->base_cost;
	const int vb = AircraftVehInfo(*(const EngineID*)b)->base_cost;
	int r = va - vb;

	return _internal_sort_order ? -r : r;
}

static int CDECL AircraftEngineSpeedSorter(const void *a, const void *b)
{
	const int va = AircraftVehInfo(*(const EngineID*)a)->max_speed;
	const int vb = AircraftVehInfo(*(const EngineID*)b)->max_speed;
	const int r = va - vb;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return EngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

static int CDECL AircraftEngineRunningCostSorter(const void *a, const void *b)
{
	const int va = AircraftVehInfo(*(const EngineID*)a)->running_cost;
	const int vb = AircraftVehInfo(*(const EngineID*)b)->running_cost;
	const int r = va - vb;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return EngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

static int CDECL AircraftEngineCargoSorter(const void *a, const void *b)
{
	const int va = AircraftVehInfo(*(const EngineID*)a)->passenger_capacity;
	const int vb = AircraftVehInfo(*(const EngineID*)b)->passenger_capacity;
	const int r = va - vb;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return EngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

static EngList_SortTypeFunction * const _aircraft_sorter[] = {
	&EngineNumberSorter,
	&AircraftEngineCostSorter,
	&AircraftEngineSpeedSorter,
	&EngineIntroDateSorter,
	&EngineNameSorter,
	&AircraftEngineRunningCostSorter,
	&EngineReliabilitySorter,
	&AircraftEngineCargoSorter,
};

static const StringID _aircraft_sort_listing[] = {
	STR_ENGINE_SORT_ENGINE_ID,
	STR_ENGINE_SORT_COST,
	STR_SORT_BY_MAX_SPEED,
	STR_ENGINE_SORT_INTRO_DATE,
	STR_SORT_BY_DROPDOWN_NAME,
	STR_ENGINE_SORT_RUNNING_COST,
	STR_SORT_BY_RELIABILITY,
	STR_ENGINE_SORT_CARGO_CAPACITY,
	INVALID_STRING_ID
};


/**
* Draw the purchase info details of an aircraft at a given location.
 * @param x,y location where to draw the info
 * @param engine_number the engine of which to draw the info of
 */
void DrawAircraftPurchaseInfo(int x, int y, uint w, EngineID engine_number)
{
	const AircraftVehicleInfo *avi = AircraftVehInfo(engine_number);
	const Engine *e = GetEngine(engine_number);
	CargoID cargo;
	YearMonthDay ymd;
	ConvertDateToYMD(e->intro_date, &ymd);

	/* Purchase cost - Max speed */
	SetDParam(0, avi->base_cost * (_price.aircraft_base>>3)>>5);
	SetDParam(1, avi->max_speed * 128 / 10);
	DrawString(x, y, STR_PURCHASE_INFO_COST_SPEED, 0);
	y += 10;

	/* Cargo capacity */
	cargo = FindFirstRefittableCargo(engine_number);
	if (cargo == CT_INVALID || cargo == CT_PASSENGERS) {
		SetDParam(0, avi->passenger_capacity);
		SetDParam(1, avi->mail_capacity);
		DrawString(x, y, STR_PURCHASE_INFO_AIRCRAFT_CAPACITY, 0);
	} else {
		/* Note, if the default capacity is selected by the refit capacity
		* callback, then the capacity shown is likely to be incorrect. */
		SetDParam(0, cargo);
		SetDParam(1, AircraftDefaultCargoCapacity(cargo, engine_number));
		SetDParam(2, STR_9842_REFITTABLE);
		DrawString(x, y, STR_PURCHASE_INFO_CAPACITY, 0);
	}
	y += 10;

	/* Running cost */
	SetDParam(0, avi->running_cost * _price.aircraft_running >> 8);
	DrawString(x, y, STR_PURCHASE_INFO_RUNNINGCOST, 0);
	y += 10;

	/* Design date - Life length */
	SetDParam(0, ymd.year);
	SetDParam(1, e->lifelength);
	DrawString(x, y, STR_PURCHASE_INFO_DESIGNED_LIFE, 0);
	y += 10;

	/* Reliability */
	SetDParam(0, e->reliability * 100 >> 16);
	DrawString(x, y, STR_PURCHASE_INFO_RELIABILITY, 0);
	y += 10;

	/* Additional text from NewGRF */
	y += ShowAdditionalText(x, y, w, engine_number);
	y += ShowRefitOptionsList(x, y, w, engine_number);
}

void DrawAircraftImage(const Vehicle *v, int x, int y, VehicleID selection)
{
	PalSpriteID pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
	DrawSprite(GetAircraftImage(v, DIR_W) | pal, x + 25, y + 10);
	if (v->subtype == 0) {
		SpriteID rotor_sprite = GetCustomRotorSprite(v, true);
		if (rotor_sprite == 0) rotor_sprite = SPR_ROTOR_STOPPED;
		DrawSprite(rotor_sprite, x + 25, y + 5);
	}
	if (v->index == selection) {
		DrawFrameRect(x - 1, y - 1, x + 58, y + 21, 0xF, FR_BORDERONLY);
	}
}

void CcBuildAircraft(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		const Vehicle *v = GetVehicle(_new_vehicle_id);

		if (v->tile == _backup_orders_tile) {
			_backup_orders_tile = 0;
			RestoreVehicleOrders(v, _backup_orders_data);
		}
		ShowAircraftViewWindow(v);
	}
}

static void GenerateBuildAircraftList(Window *w)
{
	EngineID eid;
	buildvehicle_d *bv = &WP(w, buildvehicle_d);

	EngList_RemoveAll(&bv->eng_list);

	for (eid = AIRCRAFT_ENGINES_INDEX; eid < AIRCRAFT_ENGINES_INDEX + NUM_AIRCRAFT_ENGINES; eid++) {
		if (IsEngineBuildable(eid, VEH_Aircraft)) {
			const AircraftVehicleInfo *avi = AircraftVehInfo(eid);
			switch (bv->filter.acc_planes) {
				case HELICOPTERS_ONLY:
					if (avi->subtype != 0) continue; // if not helicopter
					break;

				case AIRCRAFT_ONLY:
					if (avi->subtype == 0) continue; // if helicopter
					break;

				case ALL:
					break;
			}
			EngList_Add(&bv->eng_list, eid);
		}
	}
}

static void GenerateBuildList(Window *w)
{
	buildvehicle_d *bv = &WP(w, buildvehicle_d);

	switch (bv->vehicle_type) {
		case VEH_Aircraft:
			GenerateBuildAircraftList(w);
			_internal_sort_order = WP(w,buildvehicle_d).descending_sort_order;
			EngList_Sort(&WP(w, buildvehicle_d).eng_list, _aircraft_sorter[WP(w,buildvehicle_d).sort_criteria]);
			break;

		default: NOT_REACHED();
	}
}

static inline const EngineID *GetEngineArray(Window *w)
{
	return WP(w, buildvehicle_d).eng_list;
}

static inline uint16 GetEngineArrayLength(Window *w)
{
	return EngList_Count(&WP(w, buildvehicle_d).eng_list);
}

static void DrawBuildAircraftWindow(Window *w)
{
	buildvehicle_d *bv = &WP(w, buildvehicle_d);

	SetWindowWidgetDisabledState(w, BUILD_VEHICLE_WIDGET_BUILD, w->window_number == 0);

	GenerateBuildList(w);

	if (bv->sel_engine != INVALID_ENGINE) {
		int i;
		bool found = false;
		int num_planes = GetEngineArrayLength(w);
		for (i = 0; i < num_planes; i++) {
			if (bv->sel_engine != GetEngineArray(w)[i]) continue;
			found = true;
			break;
		}
		if (!found) bv->sel_engine = INVALID_ENGINE;
	}

	SetVScrollCount(w, GetEngineArrayLength(w));
	DrawWindowWidgets(w);

	{
		int x = 2;
		int y = 27;
		EngineID selected_id = bv->sel_engine;
		EngineID eid = w->vscroll.pos;
		const EngineID *list = GetEngineArray(w);
		uint16 list_length = GetEngineArrayLength(w);
		uint16 max = min(w->vscroll.pos + w->vscroll.cap, list_length);

		for(; eid < max; eid++) {
			const EngineID engine = list[eid];

			DrawString(x + 62, y + 7, GetCustomEngineName(engine), engine == selected_id ? 0xC : 0x10);
			DrawAircraftEngine(x + 29, y + 10, engine, GetEnginePalette(engine, _local_player));
			y += 24;
		}

		if (selected_id != INVALID_ENGINE) {
			const Widget *wi = &w->widget[BUILD_VEHICLE_WIDGET_PANEL];
			DrawAircraftPurchaseInfo(x, wi->top + 1, wi->right - wi->left - 2, selected_id);
		}
	}
	DrawString(85, 15, _aircraft_sort_listing[bv->sort_criteria], 0x10);
	DoDrawString(bv->descending_sort_order ? DOWNARROW : UPARROW, 69, 15, 0x10);
}

static void BuildAircraftClickEvent(Window *w, WindowEvent *e)
{
	buildvehicle_d *bv = &WP(w, buildvehicle_d);

	switch (e->we.click.widget) {
		case BUILD_VEHICLE_WIDGET_SORT_ASSENDING_DESCENDING:
			bv->descending_sort_order = !bv->descending_sort_order;
			_last_sort_order = bv->descending_sort_order;
			GenerateBuildList(w);
			SetWindowDirty(w);
			break;

		case BUILD_VEHICLE_WIDGET_LIST: {
			uint i = (e->we.click.pt.y - 26) / 24;
			if (i < w->vscroll.cap) {
				i += w->vscroll.pos;
				bv->sel_engine = (i < GetEngineArrayLength(w)) ? GetEngineArray(w)[i] : INVALID_ENGINE;
				SetWindowDirty(w);
			}
			break;
		}

		case BUILD_VEHICLE_WIDGET_SORT_TEXT: case BUILD_VEHICLE_WIDGET_SORT_DROPDOWN:/* Select sorting criteria dropdown menu */
			ShowDropDownMenu(w, _aircraft_sort_listing, bv->sort_criteria, BUILD_VEHICLE_WIDGET_SORT_DROPDOWN, 0, 0);
			return;

		case BUILD_VEHICLE_WIDGET_BUILD: {
			EngineID sel_eng = bv->sel_engine;
			if (sel_eng != INVALID_ENGINE) {
				DoCommandP(w->window_number, sel_eng, 0, CcBuildAircraft, CMD_BUILD_AIRCRAFT | CMD_MSG(STR_A008_CAN_T_BUILD_AIRCRAFT));
			}
			break;
		}

		case BUILD_VEHICLE_WIDGET_RENAME: {
			EngineID sel_eng = bv->sel_engine;
			if (sel_eng != INVALID_ENGINE) {
				bv->rename_engine = sel_eng;
				ShowQueryString(GetCustomEngineName(sel_eng), STR_A039_RENAME_AIRCRAFT_TYPE, 31, 160, w->window_class, w->window_number, CS_ALPHANUMERAL);
			}
			break;
		}
	}
}

static void NewAircraftWndProc(Window *w, WindowEvent *e)
{
	buildvehicle_d *bv = &WP(w, buildvehicle_d);

	switch (e->event) {
		case WE_INVALIDATE_DATA:
			SetWindowDirty(w);
			break;

		case WE_DESTROY:
			EngList_Destroy(&bv->eng_list);
			break;

		case WE_PAINT:
			DrawBuildAircraftWindow(w);
			break;

		case WE_CLICK:
			BuildAircraftClickEvent(w, e);
			break;

		case WE_ON_EDIT_TEXT: {
			if (e->we.edittext.str[0] != '\0') {
				_cmd_text = e->we.edittext.str;
				DoCommandP(0, bv->rename_engine, 0, NULL, CMD_RENAME_ENGINE | CMD_MSG(STR_A03A_CAN_T_RENAME_AIRCRAFT_TYPE));
			}
			break;
		}

		case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
			if (bv->sort_criteria != e->we.dropdown.index) {
				bv->sort_criteria = e->we.dropdown.index;
				_last_sort_criteria = e->we.dropdown.index;
				GenerateBuildList(w);
			}
			SetWindowDirty(w);
			break;

		case WE_RESIZE:
			w->vscroll.cap += e->we.sizing.diff.y / 24;
			w->widget[BUILD_VEHICLE_WIDGET_LIST].data = (w->vscroll.cap << 8) + 1;
			break;
	}
}

static const WindowDesc _build_vehicle_desc = {
	WDP_AUTO, WDP_AUTO, 240, 226,
	WC_BUILD_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_build_vehicle_widgets,
	NewAircraftWndProc
};

void ShowBuildVehicleWindow(TileIndex tile, byte type)
{
	buildvehicle_d *bv;
	Window *w;

	DeleteWindowById(WC_BUILD_VEHICLE, tile);
	w = AllocateWindowDescFront(&_build_vehicle_desc, tile);
	if (w == NULL) return;

	if (tile != 0) {
		w->caption_color = GetTileOwner(tile);
	} else {
		w->caption_color = _local_player;
	}

	w->resize.step_height = GetVehicleListHeight(type);
	w->vscroll.cap = 4;
	w->widget[BUILD_VEHICLE_WIDGET_LIST].data = (w->vscroll.cap << 8) + 1;

	bv = &WP(w, buildvehicle_d);
	EngList_Create(&bv->eng_list);
	bv->sel_engine           = INVALID_ENGINE;
	bv->sort_criteria        = _last_sort_criteria;
	bv->descending_sort_order = _last_sort_order;

	bv->vehicle_type = type;

	switch (type) {
		case VEH_Aircraft: {
			byte acc_planes = GetAirport(GetStationByTile(tile)->airport_type)->acc_planes;
			bv->filter.acc_planes = acc_planes;
			break;
		}
		default: NOT_REACHED();
	}

	GenerateBuildList(w);
}
