/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "aircraft.h"
#include "debug.h"
#include "functions.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "map.h"
#include "window.h"
#include "gui.h"
#include "vehicle.h"
#include "gfx.h"
#include "command.h"
#include "engine.h"
#include "viewport.h"
#include "player.h"
#include "depot.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"


void CcCloneAircraft(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) ShowAircraftViewWindow(GetVehicle(_new_vehicle_id));
}

static void AircraftDetailsWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		const Vehicle *v = GetVehicle(w->window_number);

		SetWindowWidgetDisabledState(w, 2, v->owner != _local_player);

		/* Disable service-scroller when interval is set to disabled */
		SetWindowWidgetDisabledState(w, 5, !_patches.servint_aircraft);
		SetWindowWidgetDisabledState(w, 6, !_patches.servint_aircraft);

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

					SetDParam(0, v->cargo_type);
					SetDParam(1, v->cargo_cap);
					u = v->next;
					SetDParam(2, u->cargo_type);
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
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   389,    14,    55, 0x0,              STR_NULL },
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   389,    56,   101, 0x0,              STR_NULL },
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    10,   102,   107, STR_0188,         STR_884D_INCREASE_SERVICING_INTERVAL },
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    10,   108,   113, STR_0189,         STR_884E_DECREASE_SERVICING_INTERVAL },
{      WWT_PANEL,   RESIZE_NONE,    14,    11,   389,   102,   113, 0x0,              STR_NULL },
{   WIDGETS_END},
};

static const WindowDesc _aircraft_details_desc = {
	WDP_AUTO, WDP_AUTO, 390, 114,
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

	w = AllocateWindowDescFront(&_aircraft_details_desc, veh);
	w->caption_color = v->owner;
//	w->vscroll.cap = 6;
//	w->traindetails_d.tab = 0;
}


static const Widget _aircraft_view_widgets[] = {
{   WWT_CLOSEBOX,  RESIZE_NONE,  14,   0,  10,   0,  13, STR_00C5,                 STR_018B_CLOSE_WINDOW },
{    WWT_CAPTION, RESIZE_RIGHT,  14,  11, 237,   0,  13, STR_A00A,                 STR_018C_WINDOW_TITLE_DRAG_THIS },
{  WWT_STICKYBOX,    RESIZE_LR,  14, 238, 249,   0,  13, 0x0,                      STR_STICKY_BUTTON },
{      WWT_PANEL,    RESIZE_RB,  14,   0, 231,  14, 103, 0x0,                      STR_NULL },
{      WWT_INSET,    RESIZE_RB,  14,   2, 229,  16, 101, 0x0,                      STR_NULL },
{    WWT_PUSHBTN,   RESIZE_RTB,  14,   0, 237, 104, 115, 0x0,                      STR_A027_CURRENT_AIRCRAFT_ACTION },
{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  14,  31, SPR_CENTRE_VIEW_VEHICLE,  STR_A029_CENTER_MAIN_VIEW_ON_AIRCRAFT },
{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  32,  49, SPR_SEND_AIRCRAFT_TODEPOT,STR_A02A_SEND_AIRCRAFT_TO_HANGAR },
{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  50,  67, SPR_REFIT_VEHICLE,        STR_A03B_REFIT_AIRCRAFT_TO_CARRY },
{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  68,  85, SPR_SHOW_ORDERS,          STR_A028_SHOW_AIRCRAFT_S_ORDERS },
{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  86, 103, SPR_SHOW_VEHICLE_DETAILS, STR_A02B_SHOW_AIRCRAFT_DETAILS },
{ WWT_PUSHIMGBTN,    RESIZE_LR,  14, 232, 249,  32,  49, SPR_CLONE_AIRCRAFT,       STR_CLONE_AIRCRAFT_INFO },
{      WWT_PANEL,   RESIZE_LRB,  14, 232, 249, 104, 103, 0x0,                      STR_NULL },
{  WWT_RESIZEBOX,  RESIZE_LRTB,  14, 238, 249, 104, 115, 0x0,                      STR_NULL },
{   WIDGETS_END},
};


static void AircraftViewWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		const Vehicle *v = GetVehicle(w->window_number);
		StringID str;
		bool is_localplayer = v->owner == _local_player;

		SetWindowWidgetDisabledState(w,  7, !is_localplayer);
		SetWindowWidgetDisabledState(w,  8, !IsAircraftInHangarStopped(v) || !is_localplayer);
		SetWindowWidgetDisabledState(w, 11, !is_localplayer);


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
			ShowVehicleRefitWindow(v, INVALID_VEH_ORDER_ID);
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
		bool plane_stopped = IsAircraftInHangarStopped(v);

		/* Widget 7 (send to hangar) must be hidden if the plane is already stopped in hangar.
		 * Widget 11 (clone) should then be shown, since cloning is allowed only while in hangar and stopped.
		 * This sytem allows to have two buttons, on top of each other*/
		if (plane_stopped != IsWindowWidgetHidden(w, 7) || plane_stopped == IsWindowWidgetHidden(w, 11)) {
			SetWindowWidgetHiddenState(w,  7, plane_stopped);  // send to hangar
			SetWindowWidgetHiddenState(w, 11, !plane_stopped); // clone
			SetWindowDirty(w);
		}
	} break;
	}
}


static const WindowDesc _aircraft_view_desc = {
	WDP_AUTO, WDP_AUTO, 250, 116,
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
