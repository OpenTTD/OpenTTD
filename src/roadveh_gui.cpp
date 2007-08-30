/* $Id$ */

/** @file roadveh_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "roadveh.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "window.h"
#include "gui.h"
#include "strings.h"
#include "vehicle.h"
#include "viewport.h"
#include "command.h"
#include "depot.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"

static inline int RoadVehLengthToPixels(int length)
{
	return (length * 28) / 8;
}

void DrawRoadVehImage(const Vehicle *v, int x, int y, int count, VehicleID selection)
{
	int dx = 0;

	/* Road vehicle lengths are measured in eighths of the standard length, so
	 * count is the number of standard vehicles that should be drawn. If it is
	 * 0, we draw enough vehicles for 10 standard vehicle lengths. */
	int max_length = (count == 0) ? 80 : count * 8;

	do {
		int length = v->u.road.cached_veh_length;

		if (dx + length > 0 && dx <= max_length) {
			SpriteID pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
			DrawSprite(v->GetImage(DIR_W), pal, x + 14 + RoadVehLengthToPixels(dx), y + 6);

			if (v->index == selection) {
				DrawFrameRect(x - 1, y - 1, x + 28, y + 12, 15, FR_BORDERONLY);
			}
		}

		dx += length;
		v = v->Next();
	} while (v != NULL && dx < max_length);
}

static void RoadVehDetailsWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_CREATE: {
		const Vehicle *v = GetVehicle(w->window_number);

		if (!RoadVehHasArticPart(v)) break;

		/* Draw the text under the vehicle instead of next to it, minus the
		 * height already allocated for the cargo of the first vehicle. */
		uint height_extension = 15 - 11;

		/* Add space for the cargo amount for each part. */
		do {
			height_extension += 11;
		} while ((v = v->Next()) != NULL);

		ResizeWindow(w, 0, height_extension);
	} break;

	case WE_PAINT: {
		const Vehicle *v = GetVehicle(w->window_number);
		StringID str;
		uint y_offset = RoadVehHasArticPart(v) ? 15 :0;

		SetWindowWidgetDisabledState(w, 2, v->owner != _local_player);
		/* disable service-scroller when interval is set to disabled */
		SetWindowWidgetDisabledState(w, 5, !_patches.servint_roadveh);
		SetWindowWidgetDisabledState(w, 6, !_patches.servint_roadveh);

		SetDParam(0, v->index);
		DrawWindowWidgets(w);

		/* Draw running cost */
		{
			int year = v->age / 366;

			SetDParam(1, year);

			SetDParam(0, (v->age + 365 < v->max_age) ? STR_AGE : STR_AGE_RED);
			SetDParam(2, v->max_age / 366);
			SetDParam(3, v->GetDisplayRunningCost());
			DrawString(2, 15, STR_900D_AGE_RUNNING_COST_YR, 0);
		}

		/* Draw max speed */
		{
			SetDParam(0, v->GetDisplayMaxSpeed());
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

		DrawRoadVehImage(v, 3, 57, 0, INVALID_VEHICLE);

		SetDParam(0, v->engine_type);
		SetDParam(1, v->build_year);
		SetDParam(2, v->value);
		DrawString(34, 57 + y_offset, STR_9011_BUILT_VALUE, 0);

		if (RoadVehHasArticPart(v)) {
			AcceptedCargo max_cargo;
			char capacity[512];

			memset(max_cargo, 0, sizeof(max_cargo));

			for (const Vehicle *u = v; u != NULL; u = u->Next()) {
				max_cargo[u->cargo_type] += u->cargo_cap;
			}

			GetString(capacity, STR_ARTICULATED_RV_CAPACITY, lastof(capacity));

			bool first = true;
			for (CargoID i = 0; i < NUM_CARGO; i++) {
				if (max_cargo[i] > 0) {
					char buffer[128];

					SetDParam(0, i);
					SetDParam(1, max_cargo[i]);
					GetString(buffer, STR_BARE_CARGO, lastof(buffer));

					if (!first) strecat(capacity, ", ", lastof(capacity));
					strecat(capacity, buffer, lastof(capacity));

					first = false;
				}
			}

			SetDParamStr(0, capacity);
			DrawStringTruncated(34, 67 + y_offset, STR_JUST_STRING, 0, w->width - 34);

			for (const Vehicle *u = v; u != NULL; u = u->Next()) {
				str = STR_8812_EMPTY;
				if (!u->cargo.Empty()) {
					SetDParam(0, u->cargo_type);
					SetDParam(1, u->cargo.Count());
					SetDParam(2, u->cargo.Source());
					str = STR_8813_FROM;
				}
				DrawString(34, 78 + y_offset, str, 0);

				y_offset += 11;
			}

			y_offset -= 11;
		} else {
			SetDParam(0, v->cargo_type);
			SetDParam(1, v->cargo_cap);
			DrawString(34, 67 + y_offset, STR_9012_CAPACITY, 0);

			str = STR_8812_EMPTY;
			if (!v->cargo.Empty()) {
				SetDParam(0, v->cargo_type);
				SetDParam(1, v->cargo.Count());
				SetDParam(2, v->cargo.Source());
				str = STR_8813_FROM;
			}
			DrawString(34, 78 + y_offset, str, 0);
		}

		/* Draw Transfer credits text */
		SetDParam(0, v->cargo.FeederShare());
		DrawString(34, 90 + y_offset, STR_FEEDER_CARGO_VALUE, 0);

		/* Draw service interval text */
		{
			SetDParam(0, v->service_interval);
			SetDParam(1, v->date_of_last_service);
			DrawString(13, 102 + y_offset, _patches.servint_ispercent ? STR_SERVICING_INTERVAL_PERCENT : STR_883C_SERVICING_INTERVAL_DAYS, 0);
		}
	} break;

	case WE_CLICK: {
		int mod;
		const Vehicle *v;
		switch (e->we.click.widget) {
		case 2: /* rename */
			v = GetVehicle(w->window_number);
			SetDParam(0, v->index);
			ShowQueryString(STR_VEHICLE_NAME, STR_902C_NAME_ROAD_VEHICLE, 31, 150, w, CS_ALPHANUMERAL);
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
{      WWT_PANEL,   RESIZE_BOTTOM,  14,     0,   379,    56,   100, 0x0,              STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_TB,      14,     0,    10,   101,   106, STR_0188,         STR_884D_INCREASE_SERVICING_INTERVAL},
{ WWT_PUSHTXTBTN,   RESIZE_TB,      14,     0,    10,   107,   112, STR_0189,         STR_884E_DECREASE_SERVICING_INTERVAL},
{      WWT_PANEL,   RESIZE_TB,      14,    11,   379,   101,   112, 0x0,              STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _roadveh_details_desc = {
	WDP_AUTO, WDP_AUTO, 380, 113, 380, 113,
	WC_VEHICLE_DETAILS,WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_roadveh_details_widgets,
	RoadVehDetailsWndProc
};


void ShowRoadVehDetailsWindow(const Vehicle *v)
{
	Window *w;
	VehicleID veh = v->index;

	DeleteWindowById(WC_VEHICLE_ORDERS, veh);
	DeleteWindowById(WC_VEHICLE_DETAILS, veh);

	w = AllocateWindowDescFront(&_roadveh_details_desc, veh);
	w->caption_color = v->owner;
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
	ShowVehicleViewWindow(v);
}
