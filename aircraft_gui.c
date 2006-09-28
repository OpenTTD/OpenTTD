/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "aircraft.h"
#include "debug.h"
#include "functions.h"
#include "station_map.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "map.h"
#include "tile.h"
#include "window.h"
#include "gui.h"
#include "vehicle.h"
#include "gfx.h"
#include "station.h"
#include "command.h"
#include "engine.h"
#include "viewport.h"
#include "player.h"
#include "depot.h"
#include "airport.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "date.h"

/**
 * Draw the purchase info details of an aircraft at a given location.
 * @param x,y location where to draw the info
 * @param engine_number the engine of which to draw the info of
 */
void DrawAircraftPurchaseInfo(int x, int y, EngineID engine_number)
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
		SetDParam(0, _cargoc.names_long[cargo]);
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
	// XXX 227 will become a calculated width...
	y += ShowAdditionalText(x, y, 227, engine_number);
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

void CcCloneAircraft(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) ShowAircraftViewWindow(GetVehicle(_new_vehicle_id));
}

static void NewAircraftWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		TileIndex tile = w->window_number;
		byte acc_planes;

		if (tile == 0) {
			SETBIT(w->disabled_state, 5);
			acc_planes = ALL;
		} else {
			acc_planes = GetAirport(GetStationByTile(tile)->airport_type)->acc_planes;
		}

		{
			int count = 0;
			EngineID eid;

			for (eid = AIRCRAFT_ENGINES_INDEX; eid < AIRCRAFT_ENGINES_INDEX + NUM_AIRCRAFT_ENGINES; eid++) {
				const AircraftVehicleInfo *avi;

				if (!HASBIT(GetEngine(eid)->player_avail, _local_player)) continue;

				avi = AircraftVehInfo(eid);
				if ((avi->subtype & AIR_CTOL ? HELICOPTERS_ONLY : AIRCRAFT_ONLY) == acc_planes) continue;

				count++;
			}
			SetVScrollCount(w, count);
		}

		DrawWindowWidgets(w);

		{
			int x = 2;
			int y = 15;
			int sel = WP(w,buildtrain_d).sel_index;
			int pos = w->vscroll.pos;
			EngineID selected_id = INVALID_ENGINE;
			EngineID eid;

			for (eid = AIRCRAFT_ENGINES_INDEX; eid < AIRCRAFT_ENGINES_INDEX + NUM_AIRCRAFT_ENGINES; eid++) {
				const AircraftVehicleInfo *avi;

				if (!HASBIT(GetEngine(eid)->player_avail, _local_player)) continue;

				avi = AircraftVehInfo(eid);
				if ((avi->subtype & AIR_CTOL ? HELICOPTERS_ONLY : AIRCRAFT_ONLY) == acc_planes) continue;

				if (sel == 0) selected_id = eid;

				if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
					DrawString(x + 62, y + 7, GetCustomEngineName(eid), sel == 0 ? 0xC : 0x10);
					DrawAircraftEngine(x + 29, y + 10, eid, GetEnginePalette(eid, _local_player));
					y += 24;
				}

				sel--;
			}

			WP(w,buildtrain_d).sel_engine = selected_id;

			if (selected_id != INVALID_ENGINE) {
				DrawAircraftPurchaseInfo(2, w->widget[4].top + 1, selected_id);
			}
		}
	} break;

	case WE_CLICK:
		switch (e->we.click.widget) {
		case 2: { /* listbox */
			uint i = (e->we.click.pt.y - 14) / 24;
			if (i < w->vscroll.cap) {
				WP(w,buildtrain_d).sel_index = i + w->vscroll.pos;
				SetWindowDirty(w);
			}
		} break;

		case 5: { /* build */
			EngineID sel_eng = WP(w,buildtrain_d).sel_engine;
			if (sel_eng != INVALID_ENGINE)
				DoCommandP(w->window_number, sel_eng, 0, CcBuildAircraft, CMD_BUILD_AIRCRAFT | CMD_MSG(STR_A008_CAN_T_BUILD_AIRCRAFT));
		} break;

		case 6: { /* rename */
			EngineID sel_eng = WP(w,buildtrain_d).sel_engine;
			if (sel_eng != INVALID_ENGINE) {
				WP(w,buildtrain_d).rename_engine = sel_eng;
				ShowQueryString(GetCustomEngineName(sel_eng),
					STR_A039_RENAME_AIRCRAFT_TYPE, 31, 160, w->window_class, w->window_number, CS_ALPHANUMERAL);
			}
		} break;
		}
		break;

	case WE_ON_EDIT_TEXT: {
		if (e->we.edittext.str[0] != '\0') {
			_cmd_text = e->we.edittext.str;
			DoCommandP(0, WP(w, buildtrain_d).rename_engine, 0, NULL,
				CMD_RENAME_ENGINE | CMD_MSG(STR_A03A_CAN_T_RENAME_AIRCRAFT_TYPE));
		}
	} break;

	case WE_RESIZE:
		w->vscroll.cap += e->we.sizing.diff.y / 24;
		w->widget[2].data = (w->vscroll.cap << 8) + 1;
		break;
	}
}

static const Widget _new_aircraft_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW },
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   239,     0,    13, STR_A005_NEW_AIRCRAFT,   STR_018C_WINDOW_TITLE_DRAG_THIS },
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   227,    14,   109, 0x401,                   STR_A025_AIRCRAFT_SELECTION_LIST },
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   228,   239,    14,   109, 0x0,                     STR_0190_SCROLL_BAR_SCROLLS_LIST },
{     WWT_IMGBTN,     RESIZE_TB,    14,     0,   239,   110,   181, 0x0,                     STR_NULL },
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   114,   182,   193, STR_A006_BUILD_AIRCRAFT, STR_A026_BUILD_THE_HIGHLIGHTED_AIRCRAFT },
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   115,   227,   182,   193, STR_A037_RENAME,         STR_A038_RENAME_AIRCRAFT_TYPE },
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   228,   239,   182,   193, 0x0,                     STR_RESIZE_BUTTON },
{   WIDGETS_END},
};

static const WindowDesc _new_aircraft_desc = {
	-1, -1, 240, 194,
	WC_BUILD_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_new_aircraft_widgets,
	NewAircraftWndProc
};

void ShowBuildAircraftWindow(TileIndex tile)
{
	Window *w;

	DeleteWindowById(WC_BUILD_VEHICLE, tile);

	w = AllocateWindowDesc(&_new_aircraft_desc);
	w->window_number = tile;
	w->vscroll.cap = 4;
	w->widget[2].data = (w->vscroll.cap << 8) + 1;

	w->resize.step_height = 24;

	if (tile != 0) {
		w->caption_color = GetTileOwner(tile);
	} else {
		w->caption_color = _local_player;
	}
}

static void AircraftDetailsWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		const Vehicle *v = GetVehicle(w->window_number);

		w->disabled_state = v->owner == _local_player ? 0 : (1 << 2);
		if (!_patches.servint_aircraft) // disable service-scroller when interval is set to disabled
			w->disabled_state |= (1 << 5) | (1 << 6);

		SetDParam(0, v->string_id);
		SetDParam(1, v->unitnumber);
		DrawWindowWidgets(w);

		/* Draw running cost */
		{
			int year = v->age / 366;

			SetDParam(1, year);

			SetDParam(0, (v->age + 365 < v->max_age) ? STR_AGE : STR_AGE_RED);
			SetDParam(2, v->max_age / 366);
			SetDParam(3, _price.aircraft_running * AircraftVehInfo(v->engine_type)->running_cost >> 8);
			DrawString(2, 15, STR_A00D_AGE_RUNNING_COST_YR, 0);
		}

		/* Draw max speed */
		{
			SetDParam(0, v->max_speed * 128 / 10);
			DrawString(2, 25, STR_A00E_MAX_SPEED, 0);
		}

		/* Draw profit */
		{
			SetDParam(0, v->profit_this_year);
			SetDParam(1, v->profit_last_year);
			DrawString(2, 35, STR_A00F_PROFIT_THIS_YEAR_LAST_YEAR, 0);
		}

		/* Draw breakdown & reliability */
		{
			SetDParam(0, v->reliability * 100 >> 16);
			SetDParam(1, v->breakdowns_since_last_service);
			DrawString(2, 45, STR_A010_RELIABILITY_BREAKDOWNS, 0);
		}

		/* Draw service interval text */
		{
			SetDParam(0, v->service_interval);
			SetDParam(1, v->date_of_last_service);
			DrawString(13, 103, _patches.servint_ispercent?STR_SERVICING_INTERVAL_PERCENT:STR_883C_SERVICING_INTERVAL_DAYS, 0);
		}

		DrawAircraftImage(v, 3, 57, INVALID_VEHICLE);

		{
			const Vehicle *u;
			int y = 57;

			do {
				if (v->subtype <= 2) {
					SetDParam(0, GetCustomEngineName(v->engine_type));
					SetDParam(1, v->build_year);
					SetDParam(2, v->value);
					DrawString(60, y, STR_A011_BUILT_VALUE, 0);
					y += 10;

					SetDParam(0, _cargoc.names_long[v->cargo_type]);
					SetDParam(1, v->cargo_cap);
					u = v->next;
					SetDParam(2, _cargoc.names_long[u->cargo_type]);
					SetDParam(3, u->cargo_cap);
					DrawString(60, y, (u->cargo_cap != 0) ? STR_A019_CAPACITY : STR_A01A_CAPACITY, 0);
					y += 14;
				}

				if (v->cargo_count != 0) {

					/* Cargo names (fix pluralness) */
					SetDParam(0, v->cargo_type);
					SetDParam(1, v->cargo_count);
					SetDParam(2, v->cargo_source);
					DrawString(60, y, STR_8813_FROM, 0);

					y += 10;
				}
			} while ( (v=v->next) != NULL);
		}
	} break;

	case WE_CLICK: {
		int mod;
		const Vehicle *v;
		switch (e->we.click.widget) {
		case 2: /* rename */
			v = GetVehicle(w->window_number);
			SetDParam(0, v->unitnumber);
			ShowQueryString(v->string_id, STR_A030_NAME_AIRCRAFT, 31, 150, w->window_class, w->window_number, CS_ALPHANUMERAL);
			break;
		case 5: /* increase int */
			mod = _ctrl_pressed? 5 : 10;
			goto do_change_service_int;
		case 6: /* decrease int */
			mod = _ctrl_pressed?- 5 : -10;
do_change_service_int:
			v = GetVehicle(w->window_number);

			mod = GetServiceIntervalClamped(mod + v->service_interval);
			if (mod == v->service_interval) return;

			DoCommandP(v->tile, v->index, mod, NULL, CMD_CHANGE_SERVICE_INT | CMD_MSG(STR_018A_CAN_T_CHANGE_SERVICING));
			break;
		}
	} break;

	case WE_ON_EDIT_TEXT:
		if (e->we.edittext.str[0] != '\0') {
			_cmd_text = e->we.edittext.str;
			DoCommandP(0, w->window_number, 0, NULL,
				CMD_NAME_VEHICLE | CMD_MSG(STR_A031_CAN_T_NAME_AIRCRAFT));
		}
		break;
	}
}


static const Widget _aircraft_details_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,         STR_018B_CLOSE_WINDOW },
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   349,     0,    13, STR_A00C_DETAILS, STR_018C_WINDOW_TITLE_DRAG_THIS },
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   350,   389,     0,    13, STR_01AA_NAME,    STR_A032_NAME_AIRCRAFT },
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   389,    14,    55, 0x0,              STR_NULL },
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   389,    56,   101, 0x0,              STR_NULL },
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    10,   102,   107, STR_0188,         STR_884D_INCREASE_SERVICING_INTERVAL },
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    10,   108,   113, STR_0189,         STR_884E_DECREASE_SERVICING_INTERVAL },
{     WWT_IMGBTN,   RESIZE_NONE,    14,    11,   389,   102,   113, 0x0,              STR_NULL },
{   WIDGETS_END},
};

static const WindowDesc _aircraft_details_desc = {
	-1, -1, 390, 114,
	WC_VEHICLE_DETAILS, WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_aircraft_details_widgets,
	AircraftDetailsWndProc
};


static void ShowAircraftDetailsWindow(const Vehicle *v)
{
	Window *w;
	VehicleID veh = v->index;

	DeleteWindowById(WC_VEHICLE_ORDERS, veh);
	DeleteWindowById(WC_VEHICLE_DETAILS, veh);

	_alloc_wnd_parent_num = veh;
	w = AllocateWindowDesc(&_aircraft_details_desc);
	w->window_number = veh;
	w->caption_color = v->owner;
//	w->vscroll.cap = 6;
//	w->traindetails_d.tab = 0;
}


static const Widget _aircraft_view_widgets[] = {
{   WWT_CLOSEBOX,  RESIZE_NONE,  14,   0,  10,   0,  13, STR_00C5,           STR_018B_CLOSE_WINDOW },
{    WWT_CAPTION, RESIZE_RIGHT,  14,  11, 237,   0,  13, STR_A00A,           STR_018C_WINDOW_TITLE_DRAG_THIS },
{  WWT_STICKYBOX,    RESIZE_LR,  14, 238, 249,   0,  13, 0x0,                STR_STICKY_BUTTON },
{     WWT_IMGBTN,    RESIZE_RB,  14,   0, 231,  14, 103, 0x0,                STR_NULL },
{          WWT_6,    RESIZE_RB,  14,   2, 229,  16, 101, 0x0,                STR_NULL },
{ WWT_PUSHIMGBTN,   RESIZE_RTB,  14,   0, 237, 104, 115, 0x0,                STR_A027_CURRENT_AIRCRAFT_ACTION },
{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  14,  31, 0x2AB,              STR_A029_CENTER_MAIN_VIEW_ON_AIRCRAFT },
{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  32,  49, 0x2AF,              STR_A02A_SEND_AIRCRAFT_TO_HANGAR },
{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  50,  67, 0x2B4,              STR_A03B_REFIT_AIRCRAFT_TO_CARRY },
{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  68,  85, 0x2B2,              STR_A028_SHOW_AIRCRAFT_S_ORDERS },
{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  86, 103, 0x2B3,              STR_A02B_SHOW_AIRCRAFT_DETAILS },
{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  32,  49, SPR_CLONE_AIRCRAFT, STR_CLONE_AIRCRAFT_INFO },
{      WWT_PANEL,   RESIZE_LRB,  14, 232, 249, 104, 103, 0x0,                STR_NULL },
{  WWT_RESIZEBOX,  RESIZE_LRTB,  14, 238, 249, 104, 115, 0x0,                STR_NULL },
{   WIDGETS_END},
};


static void AircraftViewWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		const Vehicle *v = GetVehicle(w->window_number);
		uint32 disabled = 1 << 8;
		StringID str;

		if (IsAircraftInHangarStopped(v)) disabled = 0;

		if (v->owner != _local_player) disabled |= 1 << 8 | 1 << 7;
		w->disabled_state = disabled;

		/* draw widgets & caption */
		SetDParam(0, v->string_id);
		SetDParam(1, v->unitnumber);
		DrawWindowWidgets(w);

		if (v->vehstatus & VS_CRASHED) {
			str = STR_8863_CRASHED;
		} else if (v->vehstatus & VS_STOPPED) {
			str = STR_8861_STOPPED;
		} else {
			switch (v->current_order.type) {
			case OT_GOTO_STATION: {
				SetDParam(0, v->current_order.dest);
				SetDParam(1, v->cur_speed * 128 / 10);
				str = STR_HEADING_FOR_STATION + _patches.vehicle_speed;
			} break;

			case OT_GOTO_DEPOT: {
				/* Aircrafts always go to a station, even if you say depot */
				SetDParam(0, v->current_order.dest);
				SetDParam(1, v->cur_speed * 128 / 10);
				if (HASBIT(v->current_order.flags, OFB_HALT_IN_DEPOT) && !HASBIT(v->current_order.flags, OFB_PART_OF_ORDERS)) {
					str = STR_HEADING_FOR_HANGAR + _patches.vehicle_speed;
				} else {
					str = STR_HEADING_FOR_HANGAR_SERVICE + _patches.vehicle_speed;
				}
			} break;

			case OT_LOADING:
				str = STR_882F_LOADING_UNLOADING;
				break;

			default:
				if (v->num_orders == 0) {
					str = STR_NO_ORDERS + _patches.vehicle_speed;
					SetDParam(0, v->cur_speed * 128 / 10);
				} else {
					str = STR_EMPTY;
				}
				break;
			}
		}

		/* draw the flag plus orders */
		DrawSprite(v->vehstatus & VS_STOPPED ? SPR_FLAG_VEH_STOPPED : SPR_FLAG_VEH_RUNNING, 2, w->widget[5].top + 1);
		DrawStringCenteredTruncated(w->widget[5].left + 8, w->widget[5].right, w->widget[5].top + 1, str, 0);
		DrawWindowViewport(w);
	} break;

	case WE_CLICK: {
		const Vehicle *v = GetVehicle(w->window_number);

		switch (e->we.click.widget) {
		case 5: /* start stop */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_START_STOP_AIRCRAFT | CMD_MSG(STR_A016_CAN_T_STOP_START_AIRCRAFT));
			break;
		case 6: /* center main view */
			ScrollMainWindowTo(v->x_pos, v->y_pos);
			break;
		case 7: /* goto hangar */
			DoCommandP(v->tile, v->index, _ctrl_pressed ? DEPOT_SERVICE : 0, NULL, CMD_SEND_AIRCRAFT_TO_HANGAR | CMD_MSG(STR_A012_CAN_T_SEND_AIRCRAFT_TO));
			break;
		case 8: /* refit */
			ShowVehicleRefitWindow(v);
			break;
		case 9: /* show orders */
			ShowOrdersWindow(v);
			break;
		case 10: /* show details */
			ShowAircraftDetailsWindow(v);
			break;
		case 11:
			/* clone vehicle */
			DoCommandP(v->tile, v->index, _ctrl_pressed ? 1 : 0, CcCloneAircraft, CMD_CLONE_VEHICLE | CMD_MSG(STR_A008_CAN_T_BUILD_AIRCRAFT));
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
		break;

	case WE_MOUSELOOP: {
		const Vehicle *v = GetVehicle(w->window_number);
		uint32 h = IsAircraftInHangarStopped(v) ? 1 << 7 : 1 << 11;

		if (h != w->hidden_state) {
			w->hidden_state = h;
			SetWindowDirty(w);
		}
	} break;
	}
}


static const WindowDesc _aircraft_view_desc = {
	-1,-1, 250, 116,
	WC_VEHICLE_VIEW ,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_aircraft_view_widgets,
	AircraftViewWndProc
};


void ShowAircraftViewWindow(const Vehicle *v)
{
	Window *w = AllocateWindowDescFront(&_aircraft_view_desc, v->index);

	if (w != NULL) {
		w->caption_color = v->owner;
		AssignWindowViewport(w, 3, 17, 0xE2, 0x54, w->window_number | (1 << 31), 0);
	}
}

void DrawSmallOrderListAircraft(const Vehicle *v, int x, int y)
{
	const Order *order;
	int sel, i = 0;

	sel = v->cur_order_index;

	FOR_VEHICLE_ORDERS(v, order) {
		if (sel == 0) DrawString(x - 6, y, STR_SMALL_RIGHT_ARROW, 16);
		sel--;

		if (order->type == OT_GOTO_STATION) {
			SetDParam(0, order->dest);
			DrawString(x, y, STR_A036, 0);

			y += 6;
			if (++i == 4) break;
		}
	}
}
