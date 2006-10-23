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
	BUILD_VEHICLE_WIDGET_PLANES,
	BUILD_VEHICLE_WIDGET_JETS,
	BUILD_VEHICLE_WIDGET_HELICOPTERS,
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

	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,    79,   214,   225, STR_BLACK_PLANES,        STR_BUILD_PLANES_TIP },
	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,    80,   159,   214,   225, STR_BLACK_JETS,          STR_BUILD_JETS_TIP },
	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   160,   239,   214,   225, STR_BLACK_HELICOPTERS,   STR_BUILD_HELICOPTERS_TIP },

	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   114,   226,   237, STR_A006_BUILD_AIRCRAFT, STR_A026_BUILD_THE_HIGHLIGHTED_AIRCRAFT },
	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   115,   227,   226,   237, STR_A037_RENAME,         STR_A038_RENAME_AIRCRAFT_TYPE },
	{  WWT_RESIZEBOX,     RESIZE_TB,    14,   228,   239,   226,   237, 0x0,                     STR_RESIZE_BUTTON },
	{   WIDGETS_END},
};

static bool _internal_sort_order; // descending/ascending
static byte _last_sort_criteria = 0;
static bool _last_sort_order = false;

typedef int CDECL VehicleSortListingTypeFunction(const void*, const void*);

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

static VehicleSortListingTypeFunction* const _aircraft_sorter[] = {
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

static inline void ExtendEngineListSize(EngineID **engine_list, uint16 *engine_list_length, uint16 step_size, uint16 max)
{
	*engine_list_length = min(*engine_list_length + step_size, max);
	*engine_list = realloc((void*)*engine_list, (*engine_list_length) * sizeof((*engine_list)[0]));
}

static void GenerateBuildAircraftList(EngineID **planes, uint16 *num_planes, EngineID **jets, uint16 *num_jets, EngineID **helicopters, uint16 *num_helicopters)
{
	uint16 plane_length      = *num_planes;
	uint16 jet_length        = *num_jets;
	uint16 helicopter_length = *num_helicopters;
	EngineID eid;

	(*num_planes)      = 0;
	(*num_jets)        = 0;
	(*num_helicopters) = 0;

	for (eid = AIRCRAFT_ENGINES_INDEX; eid < AIRCRAFT_ENGINES_INDEX + NUM_AIRCRAFT_ENGINES; eid++) {

		if (IsEngineBuildable(eid, VEH_Aircraft)) {
			const AircraftVehicleInfo *avi = AircraftVehInfo(eid);

			switch (avi->subtype) {
				case AIR_CTOL: // Propeller planes
					if (*num_planes == plane_length) ExtendEngineListSize(planes, &plane_length, 5, NUM_AIRCRAFT_ENGINES);
					(*planes)[(*num_planes)++] = eid;
					break;

				case (AIR_CTOL | AIR_FAST): // Jet planes
					if (*num_jets == jet_length) ExtendEngineListSize(jets, &jet_length, 5, NUM_AIRCRAFT_ENGINES);
					(*jets)[(*num_jets)++] = eid;
					break;

				case 0: // Helicopters
					if (*num_helicopters == helicopter_length) ExtendEngineListSize(helicopters, &helicopter_length, 5, NUM_AIRCRAFT_ENGINES);
					(*helicopters)[(*num_helicopters)++] = eid;
					break;
			}
		}
	}
}

static void GenerateBuildList(Window *w)
{
	switch (WP(w, buildvehicle_d).vehicle_type) {
		case VEH_Aircraft:
			GenerateBuildAircraftList(&WP(w, buildvehicle_d).list_a, &WP(w, buildvehicle_d).list_a_length,
									  &WP(w, buildvehicle_d).list_b, &WP(w, buildvehicle_d).list_b_length,
									  &WP(w, buildvehicle_d).list_c, &WP(w, buildvehicle_d).list_c_length);
			break;

		default: NOT_REACHED();
	}

	/* Ensure that we do not have trailing unused blocks in the arrays. We will never be able to access them anyway since we are unaware that they are there */
	WP(w, buildvehicle_d).list_a = realloc((void*)WP(w, buildvehicle_d).list_a, WP(w, buildvehicle_d).list_a_length * sizeof(WP(w, buildvehicle_d).list_a[0]));
	WP(w, buildvehicle_d).list_b = realloc((void*)WP(w, buildvehicle_d).list_b, WP(w, buildvehicle_d).list_b_length * sizeof(WP(w, buildvehicle_d).list_b[0]));
	WP(w, buildvehicle_d).list_c = realloc((void*)WP(w, buildvehicle_d).list_c, WP(w, buildvehicle_d).list_c_length * sizeof(WP(w, buildvehicle_d).list_c[0]));

	WP(w, buildvehicle_d).data_invalidated = false; // No need to regenerate the list anymore. We just did it
}

static inline EngineID *GetEngineArray(Window *w)
{
	switch (WP(w,buildvehicle_d).show_engine_button) {
		case 1: return WP(w, buildvehicle_d).list_a;
		case 2: return WP(w, buildvehicle_d).list_b;
		case 3: return WP(w, buildvehicle_d).list_c;
		default: NOT_REACHED();
	}
	return NULL;
}

static inline uint16 GetEngineArrayLength(Window *w)
{
	switch (WP(w,buildvehicle_d).show_engine_button) {
		case 1: return WP(w, buildvehicle_d).list_a_length;
		case 2: return WP(w, buildvehicle_d).list_b_length;
		case 3: return WP(w, buildvehicle_d).list_c_length;
		default: NOT_REACHED();
	}
	return 0;
}

static void SortAircraftBuildList(Window *w)
{
	_internal_sort_order = WP(w,buildvehicle_d).decenting_sort_order;
	qsort((void*)GetEngineArray(w), GetEngineArrayLength(w), sizeof(GetEngineArray(w)[0]),
		  _aircraft_sorter[WP(w,buildvehicle_d).sort_criteria]);
}

static void DrawBuildAircraftWindow(Window *w)
{
	SetWindowWidgetLoweredState(w, BUILD_VEHICLE_WIDGET_PLANES,      WP(w,buildvehicle_d).show_engine_button == 1);
	SetWindowWidgetLoweredState(w, BUILD_VEHICLE_WIDGET_JETS,        WP(w,buildvehicle_d).show_engine_button == 2);
	SetWindowWidgetLoweredState(w, BUILD_VEHICLE_WIDGET_HELICOPTERS, WP(w,buildvehicle_d).show_engine_button == 3);

	SetWindowWidgetDisabledState(w, BUILD_VEHICLE_WIDGET_BUILD, w->window_number == 0);

	if (WP(w, buildvehicle_d).data_invalidated) {
		GenerateBuildList(w);

		if (WP(w,buildvehicle_d).sel_engine != INVALID_ENGINE) {
			int i;
			bool found = false;
			if (HASBIT(WP(w,buildvehicle_d).show_engine_button, 0)) {
				for (i = 0; i < GetEngineArrayLength(w); i++) {
					if (WP(w,buildvehicle_d).sel_engine != GetEngineArray(w)[i]) continue;
					found = true;
					break;
				}
			}
			if (!found) WP(w,buildvehicle_d).sel_engine = INVALID_ENGINE;
		}
	}

	SetVScrollCount(w, GetEngineArrayLength(w));
	DrawWindowWidgets(w);

	if (WP(w,buildvehicle_d).sel_engine == INVALID_ENGINE && GetEngineArrayLength(w) != 0) {
		WP(w,buildvehicle_d).sel_engine = GetEngineArray(w)[0];
	}

	{
		int x = 2;
		int y = 27;
		EngineID selected_id = WP(w,buildvehicle_d).sel_engine;
		EngineID eid = w->vscroll.pos;
		EngineID *list = GetEngineArray(w);
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
	DrawString(85, 15, _aircraft_sort_listing[WP(w,buildvehicle_d).sort_criteria], 0x10);
	DoDrawString(WP(w,buildvehicle_d).decenting_sort_order ? DOWNARROW : UPARROW, 69, 15, 0x10);
}

static void BuildAircraftClickEvent(Window *w, WindowEvent *e)
{
	byte click_state = 0;

	switch (e->we.click.widget) {
		case BUILD_VEHICLE_WIDGET_SORT_ASSENDING_DESCENDING:
			WP(w,buildvehicle_d).decenting_sort_order = !WP(w,buildvehicle_d).decenting_sort_order;
			_last_sort_order = WP(w,buildvehicle_d).decenting_sort_order;
			SortAircraftBuildList(w);
			SetWindowDirty(w);
			break;

		case BUILD_VEHICLE_WIDGET_LIST: {
			uint i = (e->we.click.pt.y - 26) / 24;
			if (i < w->vscroll.cap) {
				i += w->vscroll.pos;

				if (i < GetEngineArrayLength(w)) {
					WP(w,buildvehicle_d).sel_engine = GetEngineArray(w)[i];
					SetWindowDirty(w);
				}
			}
		} break;

		case BUILD_VEHICLE_WIDGET_SORT_TEXT: case BUILD_VEHICLE_WIDGET_SORT_DROPDOWN:/* Select sorting criteria dropdown menu */
			ShowDropDownMenu(w, _aircraft_sort_listing, WP(w,buildvehicle_d).sort_criteria, BUILD_VEHICLE_WIDGET_SORT_DROPDOWN, 0, 0);
			return;

		case BUILD_VEHICLE_WIDGET_HELICOPTERS: click_state++;
		case BUILD_VEHICLE_WIDGET_JETS:        click_state++;
		case BUILD_VEHICLE_WIDGET_PLANES:      click_state++;

			if (WP(w,buildvehicle_d).show_engine_button == click_state) break; // We clicked the pressed button

				WP(w,buildvehicle_d).sel_engine = INVALID_ENGINE;
			WP(w,buildvehicle_d).show_engine_button = click_state;
			w->vscroll.pos = 0;
			SortAircraftBuildList(w);
			SetWindowDirty(w);
			break;

		case BUILD_VEHICLE_WIDGET_BUILD: {
			EngineID sel_eng = WP(w,buildvehicle_d).sel_engine;
			if (sel_eng != INVALID_ENGINE)
				DoCommandP(w->window_number, sel_eng, 0, CcBuildAircraft, CMD_BUILD_AIRCRAFT | CMD_MSG(STR_A008_CAN_T_BUILD_AIRCRAFT));
		} break;

		case BUILD_VEHICLE_WIDGET_RENAME: {
			EngineID sel_eng = WP(w,buildvehicle_d).sel_engine;
			if (sel_eng != INVALID_ENGINE) {
				WP(w,buildvehicle_d).rename_engine = sel_eng;
				ShowQueryString(GetCustomEngineName(sel_eng),
								STR_A039_RENAME_AIRCRAFT_TYPE, 31, 160, w->window_class, w->window_number, CS_ALPHANUMERAL);
			}
		} break;
	}
}

static void NewAircraftWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_INVALIDATE_DATA:
			WP(w,buildvehicle_d).data_invalidated = true;
			SetWindowDirty(w);
			break;

		case WE_DESTROY:
			free((void*)WP(w, buildvehicle_d).list_a);
			free((void*)WP(w, buildvehicle_d).list_b);
			free((void*)WP(w, buildvehicle_d).list_c);
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
				DoCommandP(0, WP(w, buildvehicle_d).rename_engine, 0, NULL,
						   CMD_RENAME_ENGINE | CMD_MSG(STR_A03A_CAN_T_RENAME_AIRCRAFT_TYPE));
			}
		} break;

		case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
			if (WP(w,buildvehicle_d).sort_criteria != e->we.dropdown.index) {
				WP(w,buildvehicle_d).sort_criteria = e->we.dropdown.index;
				_last_sort_criteria = e->we.dropdown.index;
				SortAircraftBuildList(w);
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
	-1, -1, 240, 238,
	WC_BUILD_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_build_vehicle_widgets,
	NewAircraftWndProc
};

/* Disable the aircraft subtype buttons for the types, that can't be build at the current airport */
static void CreateAircraftWindow(Window *w)
{
	TileIndex tile = w->window_number;

	if (tile == 0) {
		WP(w, buildvehicle_d).show_engine_button = 1;
	} else {
		byte acc_planes = GetAirport(GetStationByTile(tile)->airport_type)->acc_planes;

		WP(w, buildvehicle_d).show_engine_button = 0;
		if (acc_planes == HELICOPTERS_ONLY || acc_planes == ALL) {
			WP(w, buildvehicle_d).show_engine_button = 3;
		} else {
			DisableWindowWidget(w, BUILD_VEHICLE_WIDGET_HELICOPTERS);
		}

		if (acc_planes == AIRCRAFT_ONLY || acc_planes == ALL) {
			/* Set the start clicked button to jets if the list isn't empty. If not, then show propeller planes */
			WP(w, buildvehicle_d).show_engine_button = WP(w, buildvehicle_d).list_b_length == 0 ? 1 : 2;
		} else {
			DisableWindowWidget(w, BUILD_VEHICLE_WIDGET_JETS);
			DisableWindowWidget(w, BUILD_VEHICLE_WIDGET_PLANES);
		}

		if (WP(w, buildvehicle_d).show_engine_button == 0) {
			/* No plane type are buildable here */
			NOT_REACHED();
			WP(w, buildvehicle_d).show_engine_button = 1;
		}
	}
}

void ShowBuildVehicleWindow(TileIndex tile, byte type)
{
	Window *w;

	DeleteWindowById(WC_BUILD_VEHICLE, tile);

	w = AllocateWindowDescFront(&_build_vehicle_desc, tile);

	if (w == NULL) return;

	WP(w, buildvehicle_d).vehicle_type = type;

	w->resize.step_height = GetVehicleListHeight(type);
	w->vscroll.cap = 4;
	w->widget[BUILD_VEHICLE_WIDGET_LIST].data = (w->vscroll.cap << 8) + 1;

	if (tile != 0) {
		w->caption_color = GetTileOwner(tile);
	} else {
		w->caption_color = _local_player;
	}

	WP(w, buildvehicle_d).list_a_length = 0;
	WP(w, buildvehicle_d).list_b_length = 0;
	WP(w, buildvehicle_d).list_c_length = 0;
	WP(w, buildvehicle_d).list_a        = NULL;
	WP(w, buildvehicle_d).list_b        = NULL;
	WP(w, buildvehicle_d).list_c        = NULL;
	WP(w, buildvehicle_d).sel_engine           = INVALID_ENGINE;
	WP(w, buildvehicle_d).sort_criteria        = _last_sort_criteria;
	WP(w, buildvehicle_d).decenting_sort_order = _last_sort_order;

	GenerateBuildList(w);
	switch (type) {
		case VEH_Aircraft: CreateAircraftWindow(w); break;
		default: NOT_REACHED();
	}

	SortAircraftBuildList(w);
}
