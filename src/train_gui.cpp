/* $Id$ */

/** @file train_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "strings.h"
#include "window.h"
#include "gui.h"
#include "vehicle.h"
#include "viewport.h"
#include "command.h"
#include "vehicle_gui.h"
#include "depot.h"
#include "train.h"
#include "newgrf_engine.h"

void CcBuildWagon(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	Vehicle *v, *found;

	if (!success) return;

	/* find a locomotive in the depot. */
	found = NULL;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_TRAIN && IsFrontEngine(v) &&
				v->tile == tile &&
				v->u.rail.track == TRACK_BIT_DEPOT) {
			if (found != NULL) return; // must be exactly one.
			found = v;
		}
	}

	/* if we found a loco, */
	if (found != NULL) {
		found = GetLastVehicleInChain(found);
		/* put the new wagon at the end of the loco. */
		DoCommandP(0, _new_vehicle_id | (found->index << 16), 0, NULL, CMD_MOVE_RAIL_VEHICLE);
		RebuildVehicleLists();
	}
}

void CcBuildLoco(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	const Vehicle *v;

	if (!success) return;

	v = GetVehicle(_new_vehicle_id);
	if (tile == _backup_orders_tile) {
		_backup_orders_tile = 0;
		RestoreVehicleOrders(v, _backup_orders_data);
	}
	ShowVehicleViewWindow(v);
}

/**
 * Get the number of pixels for the given wagon length.
 * @param len Length measured in 1/8ths of a standard wagon.
 * @return Number of pixels across.
 */
int WagonLengthToPixels(int len)
{
	return (len * _traininfo_vehicle_width) / 8;
}

void DrawTrainImage(const Vehicle *v, int x, int y, int count, int skip, VehicleID selection)
{
	DrawPixelInfo tmp_dpi, *old_dpi;
	int dx = -(skip * 8) / _traininfo_vehicle_width;
	/* Position of highlight box */
	int highlight_l = 0;
	int highlight_r = 0;

	if (!FillDrawPixelInfo(&tmp_dpi, x - 2, y - 1, count + 1, 14)) return;

	count = (count * 8) / _traininfo_vehicle_width;

	old_dpi = _cur_dpi;
	_cur_dpi = &tmp_dpi;

	do {
		int width = v->u.rail.cached_veh_length;

		if (dx + width > 0) {
			if (dx <= count) {
				SpriteID pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
				DrawSprite(v->GetImage(DIR_W), pal, 16 + WagonLengthToPixels(dx), 7 + (is_custom_sprite(RailVehInfo(v->engine_type)->image_index) ? _traininfo_vehicle_pitch : 0));
				if (v->index == selection) {
					/* Set the highlight position */
					highlight_l = WagonLengthToPixels(dx) + 1;
					highlight_r = WagonLengthToPixels(dx + width) + 1;
				}
			}
		}
		dx += width;

		v = v->next;
	} while (dx < count && v != NULL);

	if (highlight_l != highlight_r) {
		/* Draw the highlight. Now done after drawing all the engines, as
		 * the next engine after the highlight could overlap it. */
		DrawFrameRect(highlight_l, 0, highlight_r, 13, 15, FR_BORDERONLY);
	}

	_cur_dpi = old_dpi;
}

static void TrainDetailsCargoTab(const Vehicle *v, int x, int y)
{
	if (v->cargo_cap != 0) {
		StringID str = STR_8812_EMPTY;

		if (!v->cargo.Empty()) {
			SetDParam(0, v->cargo_type);
			SetDParam(1, v->cargo.Count());
			SetDParam(2, v->cargo.Source());
			SetDParam(3, _patches.freight_trains);
			str = FreightWagonMult(v->cargo_type) > 1 ? STR_FROM_MULT : STR_8813_FROM;
		}
		DrawString(x, y, str, 0);
	}
}

static void TrainDetailsInfoTab(const Vehicle *v, int x, int y)
{
	if (RailVehInfo(v->engine_type)->railveh_type == RAILVEH_WAGON) {
		SetDParam(0, v->engine_type);
		SetDParam(1, v->value);
		DrawString(x, y, STR_882D_VALUE, 0x10);
	} else {
		SetDParam(0, v->engine_type);
		SetDParam(1, v->build_year);
		SetDParam(2, v->value);
		DrawString(x, y, STR_882C_BUILT_VALUE, 0x10);
	}
}

static void TrainDetailsCapacityTab(const Vehicle *v, int x, int y)
{
	if (v->cargo_cap != 0) {
		SetDParam(0, v->cargo_type);
		SetDParam(1, v->cargo_cap);
		SetDParam(2, _patches.freight_trains);
		DrawString(x, y, FreightWagonMult(v->cargo_type) > 1 ? STR_CAPACITY_MULT : STR_013F_CAPACITY, 0);
	}
}


static void DrawTrainDetailsWindow(Window *w)
{
	byte det_tab = WP(w, traindetails_d).tab;
	const Vehicle *v;
	const Vehicle *u;
	AcceptedCargo act_cargo;
	AcceptedCargo max_cargo;
	int num;
	int x;
	int y;
	int sel;

	num = 0;
	u = v = GetVehicle(w->window_number);
	if (det_tab == 3) { // Total cargo tab
		for (CargoID i = 0; i < lengthof(act_cargo); i++) {
			act_cargo[i] = 0;
			max_cargo[i] = 0;
		}

		do {
			act_cargo[u->cargo_type] += u->cargo.Count();
			max_cargo[u->cargo_type] += u->cargo_cap;
		} while ((u = u->next) != NULL);

		/* Set scroll-amount seperately from counting, as to not compute num double
		 * for more carriages of the same type
		 */
		for (CargoID i = 0; i < NUM_CARGO; i++) {
			if (max_cargo[i] > 0) num++; // only count carriages that the train has
		}
		num++; // needs one more because first line is description string
	} else {
		do {
			if (!IsArticulatedPart(u) || u->cargo_cap != 0) num++;
		} while ((u = u->next) != NULL);
	}

	SetVScrollCount(w, num);

	DisableWindowWidget(w, det_tab + 9);
	SetWindowWidgetDisabledState(w, 2, v->owner != _local_player);

	/* disable service-scroller when interval is set to disabled */
	SetWindowWidgetDisabledState(w, 6, !_patches.servint_trains);
	SetWindowWidgetDisabledState(w, 7, !_patches.servint_trains);

	SetDParam(0, v->index);
	DrawWindowWidgets(w);

	SetDParam(1, v->age / 366);

	x = 2;

	SetDParam(0, (v->age + 365 < v->max_age) ? STR_AGE : STR_AGE_RED);
	SetDParam(2, v->max_age / 366);
	SetDParam(3, GetTrainRunningCost(v) >> 8);
	DrawString(x, 15, STR_885D_AGE_RUNNING_COST_YR, 0);

	SetDParam(2, v->GetDisplayMaxSpeed());
	SetDParam(1, v->u.rail.cached_power);
	SetDParam(0, v->u.rail.cached_weight);
	SetDParam(3, v->u.rail.cached_max_te / 1000);
	DrawString(x, 25, (_patches.realistic_acceleration && v->u.rail.railtype != RAILTYPE_MAGLEV) ?
		STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED_MAX_TE :
		STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED, 0);

	SetDParam(0, v->profit_this_year);
	SetDParam(1, v->profit_last_year);
	DrawString(x, 35, STR_885F_PROFIT_THIS_YEAR_LAST_YEAR, 0);

	SetDParam(0, 100 * (v->reliability>>8) >> 8);
	SetDParam(1, v->breakdowns_since_last_service);
	DrawString(x, 45, STR_8860_RELIABILITY_BREAKDOWNS, 0);

	SetDParam(0, v->service_interval);
	SetDParam(1, v->date_of_last_service);
	DrawString(x + 11, 57 + (w->vscroll.cap * 14), _patches.servint_ispercent ? STR_SERVICING_INTERVAL_PERCENT : STR_883C_SERVICING_INTERVAL_DAYS, 0);

	y = 57;
	sel = w->vscroll.pos;

	/* draw the first 3 details tabs */
	if (det_tab != 3) {
		x = 1;
		for (;;) {
			if (--sel < 0 && sel >= -w->vscroll.cap) {
				int dx = 0;
				int px;
				int py;

				u = v;
				do {
					SpriteID pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
					DrawSprite(u->GetImage(DIR_W), pal, x + WagonLengthToPixels(4 + dx), y + 6 + (is_custom_sprite(RailVehInfo(u->engine_type)->image_index) ? _traininfo_vehicle_pitch : 0));
					dx += u->u.rail.cached_veh_length;
					u = u->next;
				} while (u != NULL && IsArticulatedPart(u) && u->cargo_cap == 0);

				px = x + WagonLengthToPixels(dx) + 2;
				py = y + 2;
				switch (det_tab) {
					default: NOT_REACHED();
					case 0: TrainDetailsCargoTab(   v, px, py); break;
					case 1:
						/* Only show name and value for the 'real' part */
						if (!IsArticulatedPart(v)) {
							TrainDetailsInfoTab(v, px, py);
						}
						break;
					case 2: TrainDetailsCapacityTab(v, px, py); break;
				}
				y += 14;

				v = u;
			} else {
				/* Move to the next line */
				do {
					v = v->next;
				} while (v != NULL && IsArticulatedPart(v) && v->cargo_cap == 0);
			}
			if (v == NULL) return;
		}
	} else {
		/* draw total cargo tab */
		DrawString(x, y + 2, STR_013F_TOTAL_CAPACITY_TEXT, 0);
		for (CargoID i = 0; i < NUM_CARGO; i++) {
			if (max_cargo[i] > 0 && --sel < 0 && sel > -w->vscroll.cap) {
				y += 14;
				SetDParam(0, i);            // {CARGO} #1
				SetDParam(1, act_cargo[i]); // {CARGO} #2
				SetDParam(2, i);            // {SHORTCARGO} #1
				SetDParam(3, max_cargo[i]); // {SHORTCARGO} #2
				SetDParam(4, _patches.freight_trains);
				DrawString(x, y + 2, FreightWagonMult(i) > 1 ? STR_TOTAL_CAPACITY_MULT : STR_013F_TOTAL_CAPACITY, 0);
			}
		}
		SetDParam(0, v->cargo.FeederShare());
		DrawString(x, y + 15, STR_FEEDER_CARGO_VALUE, 0);
	}
}

static void TrainDetailsWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT:
		DrawTrainDetailsWindow(w);
		break;
	case WE_CLICK: {
		int mod;
		const Vehicle *v;
		switch (e->we.click.widget) {
		case 2: /* name train */
			v = GetVehicle(w->window_number);
			SetDParam(0, v->index);
			ShowQueryString(STR_VEHICLE_NAME, STR_8865_NAME_TRAIN, 31, 150, w, CS_ALPHANUMERAL);
			break;
		case 6: /* inc serv interval */
			mod = _ctrl_pressed? 5 : 10;
			goto do_change_service_int;

		case 7: /* dec serv interval */
			mod = _ctrl_pressed? -5 : -10;
do_change_service_int:
			v = GetVehicle(w->window_number);

			mod = GetServiceIntervalClamped(mod + v->service_interval);
			if (mod == v->service_interval) return;

			DoCommandP(v->tile, v->index, mod, NULL, CMD_CHANGE_SERVICE_INT | CMD_MSG(STR_018A_CAN_T_CHANGE_SERVICING));
			break;
		/* details buttons*/
		case 9:  // Cargo
		case 10: // Information
		case 11: // Capacities
		case 12: // Total cargo
			EnableWindowWidget(w,  9);
			EnableWindowWidget(w, 10);
			EnableWindowWidget(w, 11);
			EnableWindowWidget(w, 12);
			EnableWindowWidget(w, e->we.click.widget);
			WP(w,traindetails_d).tab = e->we.click.widget - 9;
			SetWindowDirty(w);
			break;
		}
	} break;

	case WE_ON_EDIT_TEXT:
		if (e->we.edittext.str[0] != '\0') {
			_cmd_text = e->we.edittext.str;
			DoCommandP(0, w->window_number, 0, NULL,
				CMD_NAME_VEHICLE | CMD_MSG(STR_8866_CAN_T_NAME_TRAIN));
		}
		break;

	case WE_RESIZE:
		if (e->we.sizing.diff.x != 0) ResizeButtons(w, 9, 12);
		if (e->we.sizing.diff.y == 0) break;

		w->vscroll.cap += e->we.sizing.diff.y / 14;
		w->widget[4].data = (w->vscroll.cap << 8) + 1;
		break;
	}
}

static const Widget _train_details_widgets[] = {
{   WWT_CLOSEBOX, RESIZE_NONE,   14,   0,  10,   0,  13, STR_00C5,             STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION, RESIZE_RIGHT,  14,  11, 329,   0,  13, STR_8802_DETAILS,     STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN, RESIZE_LR,     14, 330, 369,   0,  13, STR_01AA_NAME,        STR_8867_NAME_TRAIN},
{      WWT_PANEL, RESIZE_RIGHT,  14,   0, 369,  14,  55, 0x0,                  STR_NULL},
{     WWT_MATRIX, RESIZE_RB,     14,   0, 357,  56, 139, 0x601,                STR_NULL},
{  WWT_SCROLLBAR, RESIZE_LRB,    14, 358, 369,  56, 139, 0x0,                  STR_0190_SCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN, RESIZE_TB,     14,   0,  10, 140, 145, STR_0188,             STR_884D_INCREASE_SERVICING_INTERVAL},
{ WWT_PUSHTXTBTN, RESIZE_TB,     14,   0,  10, 146, 151, STR_0189,             STR_884E_DECREASE_SERVICING_INTERVAL},
{      WWT_PANEL, RESIZE_RTB,    14,  11, 369, 140, 151, 0x0,                  STR_NULL},
{ WWT_PUSHTXTBTN, RESIZE_TB,     14,   0,  89, 152, 163, STR_013C_CARGO,       STR_884F_SHOW_DETAILS_OF_CARGO_CARRIED},
{ WWT_PUSHTXTBTN, RESIZE_TB,     14,  90, 178, 152, 163, STR_013D_INFORMATION, STR_8850_SHOW_DETAILS_OF_TRAIN_VEHICLES},
{ WWT_PUSHTXTBTN, RESIZE_TB,     14, 179, 268, 152, 163, STR_013E_CAPACITIES,  STR_8851_SHOW_CAPACITIES_OF_EACH},
{ WWT_PUSHTXTBTN, RESIZE_RTB,    14, 269, 357, 152, 163, STR_013E_TOTAL_CARGO, STR_8852_SHOW_TOTAL_CARGO},
{  WWT_RESIZEBOX, RESIZE_LRTB,   14, 358, 369, 152, 163, 0x0,                  STR_RESIZE_BUTTON},
{   WIDGETS_END},
};


static const WindowDesc _train_details_desc = {
	WDP_AUTO, WDP_AUTO, 370, 164, 370, 164,
	WC_VEHICLE_DETAILS,WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_train_details_widgets,
	TrainDetailsWndProc
};


void ShowTrainDetailsWindow(const Vehicle *v)
{
	Window *w;
	VehicleID veh = v->index;

	DeleteWindowById(WC_VEHICLE_ORDERS, veh);
	DeleteWindowById(WC_VEHICLE_DETAILS, veh);

	w = AllocateWindowDescFront(&_train_details_desc, veh);

	w->caption_color = v->owner;
	w->vscroll.cap = 6;
	w->widget[4].data = (w->vscroll.cap << 8) + 1;

	w->resize.step_height = 14;
	w->resize.height = w->height - 14 * 2; /* Minimum of 4 wagons in the display */

	WP(w,traindetails_d).tab = 0;
}
