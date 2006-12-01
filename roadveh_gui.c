/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "roadveh.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "map.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "vehicle.h"
#include "viewport.h"
#include "station.h"
#include "command.h"
#include "player.h"
#include "engine.h"
#include "depot.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "date.h"

/**
 * Draw the purchase info details of road vehicle at a given location.
 * @param x,y location where to draw the info
 * @param engine_number the engine of which to draw the info of
 */
void DrawRoadVehPurchaseInfo(int x, int y, uint w, EngineID engine_number)
{
	const RoadVehicleInfo *rvi = RoadVehInfo(engine_number);
	const Engine *e = GetEngine(engine_number);
	bool refittable = (_engine_info[engine_number].refit_mask != 0);
	YearMonthDay ymd;
	ConvertDateToYMD(e->intro_date, &ymd);

	/* Purchase cost - Max speed */
	SetDParam(0, rvi->base_cost * (_price.roadveh_base>>3)>>5);
	SetDParam(1, rvi->max_speed / 2);
	DrawString(x, y, STR_PURCHASE_INFO_COST_SPEED, 0);
	y += 10;

	/* Running cost */
	SetDParam(0, rvi->running_cost * _price.roadveh_running >> 8);
	DrawString(x, y, STR_PURCHASE_INFO_RUNNINGCOST, 0);
	y += 10;

	/* Cargo type + capacity */
	SetDParam(0, rvi->cargo_type);
	SetDParam(1, rvi->capacity);
	SetDParam(2, refittable ? STR_9842_REFITTABLE : STR_EMPTY);
	DrawString(x, y, STR_PURCHASE_INFO_CAPACITY, 0);
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

void DrawRoadVehImage(const Vehicle *v, int x, int y, VehicleID selection)
{
	PalSpriteID pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
	DrawSprite(GetRoadVehImage(v, DIR_W) | pal, x + 14, y + 6);

	if (v->index == selection) {
		DrawFrameRect(x - 1, y - 1, x + 28, y + 12, 15, FR_BORDERONLY);
	}
}

static void RoadVehDetailsWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		const Vehicle *v = GetVehicle(w->window_number);
		StringID str;

		SetWindowWidgetDisabledState(w, 2, v->owner != _local_player);
		// disable service-scroller when interval is set to disabled
		SetWindowWidgetDisabledState(w, 5, !_patches.servint_roadveh);
		SetWindowWidgetDisabledState(w, 6, !_patches.servint_roadveh);

		SetDParam(0, v->string_id);
		SetDParam(1, v->unitnumber);
		DrawWindowWidgets(w);

		/* Draw running cost */
		{
			int year = v->age / 366;

			SetDParam(1, year);

			SetDParam(0, (v->age + 365 < v->max_age) ? STR_AGE : STR_AGE_RED);
			SetDParam(2, v->max_age / 366);
			SetDParam(3, RoadVehInfo(v->engine_type)->running_cost * _price.roadveh_running >> 8);
			DrawString(2, 15, STR_900D_AGE_RUNNING_COST_YR, 0);
		}

		/* Draw max speed */
		{
			SetDParam(0, v->max_speed / 2);
			DrawString(2, 25, STR_900E_MAX_SPEED, 0);
		}

		/* Draw profit */
		{
			SetDParam(0, v->profit_this_year);
			SetDParam(1, v->profit_last_year);
			DrawString(2, 35, STR_900F_PROFIT_THIS_YEAR_LAST_YEAR, 0);
		}

		/* Draw breakdown & reliability */
		{
			SetDParam(0, v->reliability * 100 >> 16);
			SetDParam(1, v->breakdowns_since_last_service);
			DrawString(2, 45, STR_9010_RELIABILITY_BREAKDOWNS, 0);
		}

		/* Draw service interval text */
		{
			SetDParam(0, v->service_interval);
			SetDParam(1, v->date_of_last_service);
			DrawString(13, 90, _patches.servint_ispercent?STR_SERVICING_INTERVAL_PERCENT:STR_883C_SERVICING_INTERVAL_DAYS, 0);
		}

		DrawRoadVehImage(v, 3, 57, INVALID_VEHICLE);

		SetDParam(0, GetCustomEngineName(v->engine_type));
		SetDParam(1, v->build_year);
		SetDParam(2, v->value);
		DrawString(34, 57, STR_9011_BUILT_VALUE, 0);

		SetDParam(0, v->cargo_type);
		SetDParam(1, v->cargo_cap);
		DrawString(34, 67, STR_9012_CAPACITY, 0);

		str = STR_8812_EMPTY;
		if (v->cargo_count != 0) {
			SetDParam(0, v->cargo_type);
			SetDParam(1, v->cargo_count);
			SetDParam(2, v->cargo_source);
			str = STR_8813_FROM;
		}
		DrawString(34, 78, str, 0);
	} break;

	case WE_CLICK: {
		int mod;
		const Vehicle *v;
		switch (e->we.click.widget) {
		case 2: /* rename */
			v = GetVehicle(w->window_number);
			SetDParam(0, v->unitnumber);
			ShowQueryString(v->string_id, STR_902C_NAME_ROAD_VEHICLE, 31, 150, w->window_class, w->window_number, CS_ALPHANUMERAL);
			break;

		case 5: /* increase int */
			mod = _ctrl_pressed? 5 : 10;
			goto do_change_service_int;
		case 6: /* decrease int */
			mod = _ctrl_pressed? -5 : -10;
do_change_service_int:
			v = GetVehicle(w->window_number);

			mod = GetServiceIntervalClamped(mod + v->service_interval);
			if (mod == v->service_interval) return;

			DoCommandP(v->tile, v->index, mod, NULL, CMD_CHANGE_SERVICE_INT | CMD_MSG(STR_018A_CAN_T_CHANGE_SERVICING));
			break;
		}
	} break;

	case WE_ON_EDIT_TEXT: {
		if (e->we.edittext.str[0] != '\0') {
			_cmd_text = e->we.edittext.str;
			DoCommandP(0, w->window_number, 0, NULL,
				CMD_NAME_VEHICLE | CMD_MSG(STR_902D_CAN_T_NAME_ROAD_VEHICLE));
		}
	} break;

	}
}

static const Widget _roadveh_details_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,         STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   339,     0,    13, STR_900C_DETAILS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   340,   379,     0,    13, STR_01AA_NAME,    STR_902E_NAME_ROAD_VEHICLE},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   379,    14,    55, 0x0,              STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   379,    56,    88, 0x0,              STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    10,    89,    94, STR_0188,         STR_884D_INCREASE_SERVICING_INTERVAL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    10,    95,   100, STR_0189,         STR_884E_DECREASE_SERVICING_INTERVAL},
{      WWT_PANEL,   RESIZE_NONE,    14,    11,   379,    89,   100, 0x0,              STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _roadveh_details_desc = {
	WDP_AUTO, WDP_AUTO, 380, 101,
	WC_VEHICLE_DETAILS,WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_roadveh_details_widgets,
	RoadVehDetailsWndProc
};

static void ShowRoadVehDetailsWindow(const Vehicle *v)
{
	Window *w;
	VehicleID veh = v->index;

	DeleteWindowById(WC_VEHICLE_ORDERS, veh);
	DeleteWindowById(WC_VEHICLE_DETAILS, veh);

	w = AllocateWindowDescFront(&_roadveh_details_desc, veh);
	w->caption_color = v->owner;
}

void CcCloneRoadVeh(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) ShowRoadVehViewWindow(GetVehicle(_new_vehicle_id));
}

static void RoadVehViewWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		Vehicle *v = GetVehicle(w->window_number);
		StringID str;
		bool is_localplayer = v->owner == _local_player;

		SetWindowWidgetDisabledState(w,  7, !is_localplayer);
		SetWindowWidgetDisabledState(w,  8, !is_localplayer);
		SetWindowWidgetDisabledState(w, 11, !is_localplayer);
		/* Disable refit button if vehicle not refittable */
		SetWindowWidgetDisabledState(w, 12, !is_localplayer ||
				_engine_info[v->engine_type].refit_mask == 0);

		/* draw widgets & caption */
		SetDParam(0, v->string_id);
		SetDParam(1, v->unitnumber);
		DrawWindowWidgets(w);

		if (v->u.road.crashed_ctr != 0) {
			str = STR_8863_CRASHED;
		} else if (v->breakdown_ctr == 1) {
			str = STR_885C_BROKEN_DOWN;
		} else if (v->vehstatus & VS_STOPPED) {
			str = STR_8861_STOPPED;
		} else {
			switch (v->current_order.type) {
			case OT_GOTO_STATION: {
				SetDParam(0, v->current_order.dest);
				SetDParam(1, v->cur_speed / 2);
				str = STR_HEADING_FOR_STATION + _patches.vehicle_speed;
			} break;

			case OT_GOTO_DEPOT: {
				Depot *depot = GetDepot(v->current_order.dest);
				SetDParam(0, depot->town_index);
				SetDParam(1, v->cur_speed / 2);
				if (HASBIT(v->current_order.flags, OFB_HALT_IN_DEPOT) && !HASBIT(v->current_order.flags, OFB_PART_OF_ORDERS)) {
					str = STR_HEADING_FOR_ROAD_DEPOT + _patches.vehicle_speed;
				} else {
					str = STR_HEADING_FOR_ROAD_DEPOT_SERVICE + _patches.vehicle_speed;
				}
			} break;

			case OT_LOADING:
			case OT_LEAVESTATION:
				str = STR_882F_LOADING_UNLOADING;
				break;

			default:
				if (v->num_orders == 0) {
					str = STR_NO_ORDERS + _patches.vehicle_speed;
					SetDParam(0, v->cur_speed / 2);
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
			DoCommandP(v->tile, v->index, 0, NULL, CMD_START_STOP_ROADVEH | CMD_MSG(STR_9015_CAN_T_STOP_START_ROAD_VEHICLE));
			break;
		case 6: /* center main view */
			ScrollMainWindowTo(v->x_pos, v->y_pos);
			break;
		case 7: /* goto depot */
			DoCommandP(v->tile, v->index, _ctrl_pressed ? DEPOT_SERVICE : 0, NULL, CMD_SEND_ROADVEH_TO_DEPOT | CMD_MSG(STR_9018_CAN_T_SEND_VEHICLE_TO_DEPOT));
			break;
		case 8: /* turn around */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_TURN_ROADVEH | CMD_MSG(STR_9033_CAN_T_MAKE_VEHICLE_TURN));
			break;
		case 9: /* show orders */
			ShowOrdersWindow(v);
			break;
		case 10: /* show details */
			ShowRoadVehDetailsWindow(v);
			break;
		case 11: /* clone vehicle */
			DoCommandP(v->tile, v->index, _ctrl_pressed ? 1 : 0, CcCloneRoadVeh, CMD_CLONE_VEHICLE | CMD_MSG(STR_9009_CAN_T_BUILD_ROAD_VEHICLE));
			break;
		case 12: /* Refit vehicle */
			ShowVehicleRefitWindow(v, INVALID_VEH_ORDER_ID);
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
		DeleteWindowById(WC_VEHICLE_REFIT, w->window_number);
		DeleteWindowById(WC_VEHICLE_ORDERS, w->window_number);
		DeleteWindowById(WC_VEHICLE_DETAILS, w->window_number);
		break;

	case WE_MOUSELOOP: {
			const Vehicle *v = GetVehicle(w->window_number);
			bool rv_stopped = IsRoadVehInDepotStopped(v);

			/* Widget 7 (send to depot) must be hidden if the truck/bus is already stopped in depot.
			 * Widget 11 (clone) should then be shown, since cloning is allowed only while in depot and stopped.
			 * This sytem allows to have two buttons, on top of each other.
			 * The same system applies to widget 8 and 12, force turn around and refit. */
			if (rv_stopped != IsWindowWidgetHidden(w, 7) || rv_stopped == IsWindowWidgetHidden(w, 11)) {
				SetWindowWidgetHiddenState(w,  7, rv_stopped);  // send to depot
				SetWindowWidgetHiddenState(w,  8, rv_stopped);  // force turn around
				SetWindowWidgetHiddenState(w, 11, !rv_stopped); // clone
				SetWindowWidgetHiddenState(w, 12, !rv_stopped); // refit
				SetWindowDirty(w);
			}
		}
	}
}

static const Widget _roadveh_view_widgets[] = {
{   WWT_CLOSEBOX, RESIZE_NONE,  14,   0,  10,   0,  13, STR_00C5,                STR_018B_CLOSE_WINDOW },
{    WWT_CAPTION, RESIZE_RIGHT, 14,  11, 237,   0,  13, STR_9002,                STR_018C_WINDOW_TITLE_DRAG_THIS },
{  WWT_STICKYBOX, RESIZE_LR,    14, 238, 249,   0,  13, 0x0,                     STR_STICKY_BUTTON },
{      WWT_PANEL, RESIZE_RB,    14,   0, 231,  14, 103, 0x0,                     STR_NULL },
{      WWT_INSET, RESIZE_RB,    14,   2, 229,  16, 101, 0x0,                     STR_NULL },
{    WWT_PUSHBTN, RESIZE_RTB,   14,   0, 237, 104, 115, 0x0,                     STR_901C_CURRENT_VEHICLE_ACTION },
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  14,  31, SPR_CENTRE_VIEW_VEHICLE, STR_901E_CENTER_MAIN_VIEW_ON_VEHICLE },
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  32,  49, SPR_SEND_ROADVEH_TODEPOT,STR_901F_SEND_VEHICLE_TO_DEPOT },
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  50,  67, SPR_FORCE_VEHICLE_TURN,  STR_9020_FORCE_VEHICLE_TO_TURN_AROUND },
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  68,  85, SPR_SHOW_ORDERS,         STR_901D_SHOW_VEHICLE_S_ORDERS },
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  86, 103, SPR_SHOW_VEHICLE_DETAILS,STR_9021_SHOW_ROAD_VEHICLE_DETAILS },
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  32,  49, SPR_CLONE_ROADVEH,       STR_CLONE_ROAD_VEHICLE_INFO },
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  50,  67, SPR_REFIT_VEHICLE,       STR_REFIT_ROAD_VEHICLE_TO_CARRY },
{      WWT_PANEL, RESIZE_LRB,   14, 232, 249, 104, 103, 0x0,                     STR_NULL },
{  WWT_RESIZEBOX, RESIZE_LRTB,  14, 238, 249, 104, 115, 0x0,                     STR_NULL },
{ WIDGETS_END }
};

static const WindowDesc _roadveh_view_desc = {
	WDP_AUTO, WDP_AUTO, 250, 116,
	WC_VEHICLE_VIEW,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_roadveh_view_widgets,
	RoadVehViewWndProc,
};

void ShowRoadVehViewWindow(const Vehicle *v)
{
	Window *w = AllocateWindowDescFront(&_roadveh_view_desc, v->index);

	if (w != NULL) {
		w->caption_color = v->owner;
		AssignWindowViewport(w, 3, 17, 0xE2, 0x54, w->window_number | (1 << 31), 0);
	}
}


static void DrawNewRoadVehWindow(Window *w)
{
	EngineID selected_id;
	EngineID e;
	uint count;
	int pos;
	int sel;
	int y;

	SetWindowWidgetDisabledState(w, 5, w->window_number == 0);

	count = 0;
	for (e = ROAD_ENGINES_INDEX; e < ROAD_ENGINES_INDEX + NUM_ROAD_ENGINES; e++) {
		if (HASBIT(GetEngine(e)->player_avail, _local_player)) count++;
	}
	SetVScrollCount(w, count);

	DrawWindowWidgets(w);

	y = 15;
	sel = WP(w,buildvehicle_d).sel_index;
	pos = w->vscroll.pos;
	selected_id = INVALID_ENGINE;
	for (e = ROAD_ENGINES_INDEX; e < ROAD_ENGINES_INDEX + NUM_ROAD_ENGINES; e++) {
		if (!HASBIT(GetEngine(e)->player_avail, _local_player)) continue;
		if (sel == 0) selected_id = e;
		if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
			DrawString(60, y + 2, GetCustomEngineName(e), sel == 0 ? 0xC : 0x10);
			DrawRoadVehEngine(30, y + 6, e, GetEnginePalette(e, _local_player));
			y += 14;
		}
		sel--;
	}

	WP(w,buildvehicle_d).sel_engine = selected_id;
	if (selected_id != INVALID_ENGINE) {
		const Widget *wi = &w->widget[4];
		DrawRoadVehPurchaseInfo(2, wi->top + 1, wi->right - wi->left - 2, selected_id);
	}
}

void CcBuildRoadVeh(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	const Vehicle *v;

	if (!success) return;

	v = GetVehicle(_new_vehicle_id);
	if (v->tile == _backup_orders_tile) {
		_backup_orders_tile = 0;
		RestoreVehicleOrders(v, _backup_orders_data);
	}
	ShowRoadVehViewWindow(v);
}

static void NewRoadVehWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT:
		DrawNewRoadVehWindow(w);
		break;

	case WE_CLICK:
		switch (e->we.click.widget) {
		case 2: { /* listbox */
			uint i = (e->we.click.pt.y - 14) / 14;
			if (i < w->vscroll.cap) {
				WP(w,buildvehicle_d).sel_index = i + w->vscroll.pos;
				SetWindowDirty(w);
			}
		} break;

		case 5: { /* build */
			EngineID sel_eng = WP(w,buildvehicle_d).sel_engine;
			if (sel_eng != INVALID_ENGINE)
				DoCommandP(w->window_number, sel_eng, 0, CcBuildRoadVeh, CMD_BUILD_ROAD_VEH | CMD_MSG(STR_9009_CAN_T_BUILD_ROAD_VEHICLE));
		} break;

		case 6: { /* rename */
			EngineID sel_eng = WP(w,buildvehicle_d).sel_engine;
			if (sel_eng != INVALID_ENGINE) {
				WP(w,buildvehicle_d).rename_engine = sel_eng;
				ShowQueryString(GetCustomEngineName(sel_eng),
					STR_9036_RENAME_ROAD_VEHICLE_TYPE, 31, 160, w->window_class, w->window_number, CS_ALPHANUMERAL);
			}
		}	break;
		}
		break;

	case WE_ON_EDIT_TEXT:
		if (e->we.edittext.str[0] != '\0') {
			_cmd_text = e->we.edittext.str;
			DoCommandP(0, WP(w, buildvehicle_d).rename_engine, 0, NULL,
				CMD_RENAME_ENGINE | CMD_MSG(STR_9037_CAN_T_RENAME_ROAD_VEHICLE));
		}
		break;

	case WE_RESIZE: {
		if (e->we.sizing.diff.y == 0) break;

		w->vscroll.cap += e->we.sizing.diff.y / 14;
		w->widget[2].data = (w->vscroll.cap << 8) + 1;
	} break;

	}
}

static const Widget _new_road_veh_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                   STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   247,     0,    13, STR_9006_NEW_ROAD_VEHICLES, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   235,    14,   125, 0x801,                      STR_9026_ROAD_VEHICLE_SELECTION},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   236,   247,    14,   125, 0x0,                        STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    14,     0,   247,   126,   217, 0x0,                        STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   117,   218,   229, STR_9007_BUILD_VEHICLE,     STR_9027_BUILD_THE_HIGHLIGHTED_ROAD},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   118,   235,   218,   229, STR_9034_RENAME,            STR_9035_RENAME_ROAD_VEHICLE_TYPE},
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   236,   247,   218,   229, 0x0,                        STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const WindowDesc _new_road_veh_desc = {
	WDP_AUTO, WDP_AUTO, 248, 230,
	WC_BUILD_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_new_road_veh_widgets,
	NewRoadVehWndProc
};

void ShowBuildRoadVehWindow(TileIndex tile)
{
	Window *w;

	DeleteWindowById(WC_BUILD_VEHICLE, tile);

	w = AllocateWindowDescFront(&_new_road_veh_desc, tile);
	w->vscroll.cap = 8;
	w->widget[2].data = (w->vscroll.cap << 8) + 1;

	w->resize.step_height = 14;
	w->resize.height = w->height - 14 * 4; /* Minimum of 4 vehicles in the display */

	if (tile != 0) {
		w->caption_color = GetTileOwner(tile);
	} else {
		w->caption_color = _local_player;
	}
}


