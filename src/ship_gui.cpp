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
			SetDParam(3, v->GetDisplayRunningCost());
			DrawString(2, 15, STR_9812_AGE_RUNNING_COST_YR, 0);
		}

		/* Draw max speed */
		{
			SetDParam(0, v->GetDisplayMaxSpeed());
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
	WDP_AUTO, WDP_AUTO, 405, 113, 405, 113,
	WC_VEHICLE_DETAILS, WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_ship_details_widgets,
	ShipDetailsWndProc
};

void ShowShipDetailsWindow(const Vehicle *v)
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
	ShowVehicleViewWindow(v);
}
