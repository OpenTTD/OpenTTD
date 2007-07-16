/* $Id$ */

/** @file ship_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "ship.h"
#include "table/strings.h"
#include "strings.h"
#include "table/sprites.h"
#include "gui.h"
#include "vehicle.h"
#include "viewport.h"
#include "command.h"
#include "depot.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"

void DrawShipImage(const Vehicle *v, int x, int y, VehicleID selection)
{
	DrawSprite(v->GetImage(DIR_W), GetVehiclePalette(v), x + 32, y + 10);

	if (v->index == selection) {
		DrawFrameRect(x - 5, y - 1, x + 67, y + 21, 15, FR_BORDERONLY);
	}
}

static void ShipDetailsWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		const Vehicle *v = GetVehicle(w->window_number);
		StringID str;

		SetWindowWidgetDisabledState(w, 2, v->owner != _local_player);
		/* disable service-scroller when interval is set to disabled */
		SetWindowWidgetDisabledState(w, 5, !_patches.servint_ships);
		SetWindowWidgetDisabledState(w, 6, !_patches.servint_ships);

		SetDParam(0, v->index);
		DrawWindowWidgets(w);

		/* Draw running cost */
		{
			int year = v->age / 366;

			SetDParam(1, year);

			SetDParam(0, (v->age + 365 < v->max_age) ? STR_AGE : STR_AGE_RED);
			SetDParam(2, v->max_age / 366);
			SetDParam(3, ShipVehInfo(v->engine_type)->running_cost * _price.ship_running >> 8);
			DrawString(2, 15, STR_9812_AGE_RUNNING_COST_YR, 0);
		}

		/* Draw max speed */
		{
			SetDParam(0, v->max_speed * 10 / 32);
			DrawString(2, 25, STR_9813_MAX_SPEED, 0);
		}

		/* Draw profit */
		{
			SetDParam(0, v->profit_this_year);
			SetDParam(1, v->profit_last_year);
			DrawString(2, 35, STR_9814_PROFIT_THIS_YEAR_LAST_YEAR, 0);
		}

		/* Draw breakdown & reliability */
		{
			SetDParam(0, v->reliability * 100 >> 16);
			SetDParam(1, v->breakdowns_since_last_service);
			DrawString(2, 45, STR_9815_RELIABILITY_BREAKDOWNS, 0);
		}

		/* Draw service interval text */
		{
			SetDParam(0, v->service_interval);
			SetDParam(1, v->date_of_last_service);
			DrawString(13, 102, _patches.servint_ispercent ? STR_SERVICING_INTERVAL_PERCENT : STR_883C_SERVICING_INTERVAL_DAYS, 0);
		}

		DrawShipImage(v, 3, 57, INVALID_VEHICLE);

		SetDParam(0, v->engine_type);
		SetDParam(1, v->build_year);
		SetDParam(2, v->value);
		DrawString(74, 57, STR_9816_BUILT_VALUE, 0);

		SetDParam(0, v->cargo_type);
		SetDParam(1, v->cargo_cap);
		DrawString(74, 67, STR_9817_CAPACITY, 0);

		str = STR_8812_EMPTY;
		if (!v->cargo.Empty()) {
			SetDParam(0, v->cargo_type);
			SetDParam(1, v->cargo.Count());
			SetDParam(2, v->cargo.Source());
			str = STR_8813_FROM;
		}
		DrawString(74, 78, str, 0);

		/* Draw Transfer credits text */
		SetDParam(0, v->cargo.FeederShare());
		DrawString(74, 89, STR_FEEDER_CARGO_VALUE, 0);

	} break;

	case WE_CLICK: {
		int mod;
		const Vehicle *v;
		switch (e->we.click.widget) {
		case 2: /* rename */
			v = GetVehicle(w->window_number);
			SetDParam(0, v->index);
			ShowQueryString(STR_VEHICLE_NAME, STR_9831_NAME_SHIP, 31, 150, w, CS_ALPHANUMERAL);
			break;
		case 5: /* increase int */
			mod = _ctrl_pressed? 5 : 10;
			goto do_change_service_int;
		case 6: /* decrease int */
			mod = _ctrl_pressed ? - 5 : -10;
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
				CMD_NAME_VEHICLE | CMD_MSG(STR_9832_CAN_T_NAME_SHIP));
		}
		break;
	}
}


static const Widget _ship_details_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,         STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   364,     0,    13, STR_9811_DETAILS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   365,   404,     0,    13, STR_01AA_NAME,    STR_982F_NAME_SHIP},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   404,    14,    55, 0x0,              STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   404,    56,   100, 0x0,              STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    10,   101,   106, STR_0188,         STR_884D_INCREASE_SERVICING_INTERVAL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    10,   107,   112, STR_0189,         STR_884E_DECREASE_SERVICING_INTERVAL},
{      WWT_PANEL,   RESIZE_NONE,    14,    11,   404,   101,   112, 0x0,              STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _ship_details_desc = {
	WDP_AUTO, WDP_AUTO, 405, 113,
	WC_VEHICLE_DETAILS, WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_ship_details_widgets,
	ShipDetailsWndProc
};

static void ShowShipDetailsWindow(const Vehicle *v)
{
	Window *w;
	VehicleID veh = v->index;

	DeleteWindowById(WC_VEHICLE_ORDERS, veh);
	DeleteWindowById(WC_VEHICLE_DETAILS, veh);
	w = AllocateWindowDescFront(&_ship_details_desc, veh);
	w->caption_color = v->owner;
}

void CcBuildShip(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	const Vehicle *v;
	if (!success) return;

	v = GetVehicle(_new_vehicle_id);
	if (v->tile == _backup_orders_tile) {
		_backup_orders_tile = 0;
		RestoreVehicleOrders(v, _backup_orders_data);
	}
	ShowShipViewWindow(v);
}

void CcCloneShip(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) ShowShipViewWindow(GetVehicle(_new_vehicle_id));
}

static void ShipViewWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			Vehicle *v = GetVehicle(w->window_number);
			StringID str;
			bool refitable_and_stopped_in_depot = ShipVehInfo(v->engine_type)->refittable && IsShipInDepotStopped(v);
			bool is_localplayer = v->owner == _local_player;

			SetWindowWidgetDisabledState(w,  7, !is_localplayer);
			SetWindowWidgetDisabledState(w,  8,
			                             !is_localplayer ||      // Disable if owner is not local player
			                             !refitable_and_stopped_in_depot); // Disable if the ship is not refitable or stopped in a depot
			SetWindowWidgetDisabledState(w, 11, !is_localplayer);

			/* draw widgets & caption */
			SetDParam(0, v->index);
			DrawWindowWidgets(w);

			if (v->breakdown_ctr == 1) {
				str = STR_885C_BROKEN_DOWN;
			} else if (v->vehstatus & VS_STOPPED) {
				str = STR_8861_STOPPED;
			} else {
				switch (v->current_order.type) {
					case OT_GOTO_STATION: {
						SetDParam(0, v->current_order.dest);
						SetDParam(1, v->cur_speed * 10 / 32);
						str = STR_HEADING_FOR_STATION + _patches.vehicle_speed;
					} break;

					case OT_GOTO_DEPOT: {
						Depot *depot = GetDepot(v->current_order.dest);
						SetDParam(0, depot->town_index);
						SetDParam(1, v->cur_speed * 10 / 32);
						if (HASBIT(v->current_order.flags, OFB_HALT_IN_DEPOT) && !HASBIT(v->current_order.flags, OFB_PART_OF_ORDERS)) {
							str = STR_HEADING_FOR_SHIP_DEPOT + _patches.vehicle_speed;
						} else {
							str = STR_HEADING_FOR_SHIP_DEPOT_SERVICE + _patches.vehicle_speed;
						}
					} break;

					case OT_LOADING:
					case OT_LEAVESTATION:
						str = STR_882F_LOADING_UNLOADING;
						break;

					default:
						if (v->num_orders == 0) {
							str = STR_NO_ORDERS + _patches.vehicle_speed;
							SetDParam(0, v->cur_speed * 10 / 32);
						} else {
							str = STR_EMPTY;
						}
						break;
				}
			}

		/* draw the flag plus orders */
		DrawSprite(v->vehstatus & VS_STOPPED ? SPR_FLAG_VEH_STOPPED : SPR_FLAG_VEH_RUNNING, PAL_NONE, 2, w->widget[5].top + 1);
		DrawStringCenteredTruncated(w->widget[5].left + 8, w->widget[5].right, w->widget[5].top + 1, str, 0);
		DrawWindowViewport(w);
	} break;

		case WE_CLICK: {
			const Vehicle *v = GetVehicle(w->window_number);

			switch (e->we.click.widget) {
				case 5: /* start stop */
					DoCommandP(v->tile, v->index, 0, NULL, CMD_START_STOP_SHIP | CMD_MSG(STR_9818_CAN_T_STOP_START_SHIP));
					break;
				case 6: /* center main view */
					ScrollMainWindowTo(v->x_pos, v->y_pos);
					break;
				case 7: /* goto hangar */
					DoCommandP(v->tile, v->index, _ctrl_pressed ? DEPOT_SERVICE : 0, NULL, CMD_SEND_SHIP_TO_DEPOT | CMD_MSG(STR_9819_CAN_T_SEND_SHIP_TO_DEPOT));
					break;
				case 8: /* refit */
					ShowVehicleRefitWindow(v, INVALID_VEH_ORDER_ID);
					break;
				case 9: /* show orders */
					ShowOrdersWindow(v);
					break;
				case 10: /* show details */
					ShowShipDetailsWindow(v);
					break;
				case 11: {
					/* clone vehicle */
					DoCommandP(v->tile, v->index, _ctrl_pressed ? 1 : 0, CcCloneShip, CMD_CLONE_VEHICLE | CMD_MSG(STR_980D_CAN_T_BUILD_SHIP));
				} break;
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
			bool ship_stopped = IsShipInDepotStopped(v);

			/* Widget 7 (send to depot) must be hidden if the ship is already stopped in depot.
			 * Widget 11 (clone) should then be shown, since cloning is allowed only while in depot and stopped.
			 * This sytem allows to have two buttons, on top of each otherother*/
			if (ship_stopped != IsWindowWidgetHidden(w, 7) || ship_stopped == IsWindowWidgetHidden(w, 11)) {
				SetWindowWidgetHiddenState(w,  7, ship_stopped);  // send to depot
				SetWindowWidgetHiddenState(w, 11, !ship_stopped); // clone
				SetWindowDirty(w);
			}
		}
	}
}

static const Widget _ship_view_widgets[] = {
{   WWT_CLOSEBOX, RESIZE_NONE,  14,   0,  10,   0,  13, STR_00C5,                STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION, RESIZE_RIGHT, 14,  11, 237,   0,  13, STR_980F,                STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX, RESIZE_LR,    14, 238, 249,   0,  13, 0x0,                     STR_STICKY_BUTTON},
{      WWT_PANEL, RESIZE_RB,    14,   0, 231,  14, 103, 0x0,                     STR_NULL},
{      WWT_INSET, RESIZE_RB,    14,   2, 229,  16, 101, 0x0,                     STR_NULL},
{    WWT_PUSHBTN, RESIZE_RTB,   14,   0, 237, 104, 115, 0x0,                     STR_9827_CURRENT_SHIP_ACTION_CLICK},
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  14,  31, SPR_CENTRE_VIEW_VEHICLE, STR_9829_CENTER_MAIN_VIEW_ON_SHIP},
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  32,  49, SPR_SEND_SHIP_TODEPOT,   STR_982A_SEND_SHIP_TO_DEPOT},
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  50,  67, SPR_REFIT_VEHICLE,       STR_983A_REFIT_CARGO_SHIP_TO_CARRY},
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  68,  85, SPR_SHOW_ORDERS,         STR_9828_SHOW_SHIP_S_ORDERS},
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  86, 103, SPR_SHOW_VEHICLE_DETAILS, STR_982B_SHOW_SHIP_DETAILS},
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  32,  49, SPR_CLONE_SHIP,          STR_CLONE_SHIP_INFO},
{      WWT_PANEL, RESIZE_LRB,   14, 232, 249, 104, 103, 0x0,                     STR_NULL },
{  WWT_RESIZEBOX, RESIZE_LRTB,  14, 238, 249, 104, 115, 0x0,                     STR_NULL },
{ WIDGETS_END }
};

static const WindowDesc _ship_view_desc = {
	WDP_AUTO, WDP_AUTO, 250, 116,
	WC_VEHICLE_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_ship_view_widgets,
	ShipViewWndProc
};

void ShowShipViewWindow(const Vehicle *v)
{
	Window *w = AllocateWindowDescFront(&_ship_view_desc, v->index);

	if (w != NULL) {
		w->caption_color = v->owner;
		AssignWindowViewport(w, 3, 17, 0xE2, 0x54, w->window_number | (1 << 31), ZOOM_LVL_SHIP);
	}
}
