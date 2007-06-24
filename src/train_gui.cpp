/* $Id$ */

/** @file train_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "table/sprites.h"
#include "table/strings.h"
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
	ShowTrainViewWindow(v);
}

void CcCloneTrain(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) ShowTrainViewWindow(GetVehicle(_new_vehicle_id));
}

/**
 * Get the number of pixels for the given wagon length.
 * @param len Length measured in 1/8ths of a standard wagon.
 * @return Number of pixels across.
 */
int WagonLengthToPixels(int len) {
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
				DrawSprite(GetTrainImage(v, DIR_W), pal, 16 + WagonLengthToPixels(dx), 7 + (is_custom_sprite(RailVehInfo(v->engine_type)->image_index) ? _traininfo_vehicle_pitch : 0));
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

static const Widget _train_view_widgets[] = {
{   WWT_CLOSEBOX,  RESIZE_NONE, 14,   0,  10,   0,  13, STR_00C5,                STR_018B_CLOSE_WINDOW },
{    WWT_CAPTION, RESIZE_RIGHT, 14,  11, 237,   0,  13, STR_882E,                STR_018C_WINDOW_TITLE_DRAG_THIS },
{  WWT_STICKYBOX,    RESIZE_LR, 14, 238, 249,   0,  13, 0x0,                     STR_STICKY_BUTTON },
{      WWT_PANEL,    RESIZE_RB, 14,   0, 231,  14, 121, 0x0,                     STR_NULL },
{      WWT_INSET,    RESIZE_RB, 14,   2, 229,  16, 119, 0x0,                     STR_NULL },
{    WWT_PUSHBTN,   RESIZE_RTB, 14,   0, 237, 122, 133, 0x0,                     STR_8846_CURRENT_TRAIN_ACTION_CLICK },
{ WWT_PUSHIMGBTN,    RESIZE_LR, 14, 232, 249,  14,  31, SPR_CENTRE_VIEW_VEHICLE, STR_8848_CENTER_MAIN_VIEW_ON_TRAIN },
{ WWT_PUSHIMGBTN,    RESIZE_LR, 14, 232, 249,  32,  49, SPR_SEND_TRAIN_TODEPOT,  STR_8849_SEND_TRAIN_TO_DEPOT },
{ WWT_PUSHIMGBTN,    RESIZE_LR, 14, 232, 249,  50,  67, SPR_IGNORE_SIGNALS,      STR_884A_FORCE_TRAIN_TO_PROCEED },
{ WWT_PUSHIMGBTN,    RESIZE_LR, 14, 232, 249,  68,  85, SPR_FORCE_VEHICLE_TURN,  STR_884B_REVERSE_DIRECTION_OF_TRAIN },
{ WWT_PUSHIMGBTN,    RESIZE_LR, 14, 232, 249,  86, 103, SPR_SHOW_ORDERS,         STR_8847_SHOW_TRAIN_S_ORDERS },
{ WWT_PUSHIMGBTN,    RESIZE_LR, 14, 232, 249, 104, 121, SPR_SHOW_VEHICLE_DETAILS,STR_884C_SHOW_TRAIN_DETAILS },
{ WWT_PUSHIMGBTN,    RESIZE_LR, 14, 232, 249,  68,  85, SPR_REFIT_VEHICLE,       STR_RAIL_REFIT_VEHICLE_TO_CARRY },
{ WWT_PUSHIMGBTN,    RESIZE_LR, 14, 232, 249,  32,  49, SPR_CLONE_TRAIN,         STR_CLONE_TRAIN_INFO },
{      WWT_PANEL,   RESIZE_LRB, 14, 232, 249, 122, 121, 0x0,                     STR_NULL },
{  WWT_RESIZEBOX,  RESIZE_LRTB, 14, 238, 249, 122, 133, 0x0,                     STR_NULL },
{ WIDGETS_END }
};

static void ShowTrainDetailsWindow(const Vehicle *v);

static void TrainViewWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		const Vehicle *v, *u;
		StringID str;
		bool is_localplayer;

		v = GetVehicle(w->window_number);

		is_localplayer = v->owner == _local_player;
		SetWindowWidgetDisabledState(w,  7, !is_localplayer);
		SetWindowWidgetDisabledState(w,  8, !is_localplayer);
		SetWindowWidgetDisabledState(w,  9, !is_localplayer);
		SetWindowWidgetDisabledState(w, 13, !is_localplayer);

		/* Disable cargo refit button, until we know we can enable it below. */
		DisableWindowWidget(w, 12);

		if (is_localplayer) {
			/* See if any vehicle can be refitted */
			for (u = v; u != NULL; u = u->next) {
				if (EngInfo(u->engine_type)->refit_mask != 0 ||
						(RailVehInfo(v->engine_type)->railveh_type != RAILVEH_WAGON && v->cargo_cap != 0)) {
					EnableWindowWidget(w, 12);
					/* We have a refittable carriage, bail out */
					break;
				}
			}
		}

		/* draw widgets & caption */
		SetDParam(0, v->index);
		DrawWindowWidgets(w);

		if (v->u.rail.crash_anim_pos != 0) {
			str = STR_8863_CRASHED;
		} else if (v->breakdown_ctr == 1) {
			str = STR_885C_BROKEN_DOWN;
		} else if (v->vehstatus & VS_STOPPED) {
			if (v->u.rail.last_speed == 0) {
				if (v->u.rail.cached_power == 0) {
					str = STR_TRAIN_NO_POWER;
				} else {
					str = STR_8861_STOPPED;
				}
			} else {
				SetDParam(0, v->u.rail.last_speed * 10 / 16);
				str = STR_TRAIN_STOPPING + _patches.vehicle_speed;
			}
		} else {
			switch (v->current_order.type) {
			case OT_GOTO_STATION: {
				str = STR_HEADING_FOR_STATION + _patches.vehicle_speed;
				SetDParam(0, v->current_order.dest);
				SetDParam(1, v->u.rail.last_speed * 10 / 16);
			} break;

			case OT_GOTO_DEPOT: {
				Depot *dep = GetDepot(v->current_order.dest);
				SetDParam(0, dep->town_index);
				if (HASBIT(v->current_order.flags, OFB_HALT_IN_DEPOT) && !HASBIT(v->current_order.flags, OFB_PART_OF_ORDERS)) {
					str = STR_HEADING_FOR_TRAIN_DEPOT + _patches.vehicle_speed;
				} else {
					str = STR_HEADING_FOR_TRAIN_DEPOT_SERVICE + _patches.vehicle_speed;
				}
				SetDParam(1, v->u.rail.last_speed * 10 / 16);
			} break;

			case OT_LOADING:
			case OT_LEAVESTATION:
				str = STR_882F_LOADING_UNLOADING;
				break;

			case OT_GOTO_WAYPOINT: {
				SetDParam(0, v->current_order.dest);
				str = STR_HEADING_FOR_WAYPOINT + _patches.vehicle_speed;
				SetDParam(1, v->u.rail.last_speed * 10 / 16);
				break;
			}

			default:
				if (v->num_orders == 0) {
					str = STR_NO_ORDERS + _patches.vehicle_speed;
					SetDParam(0, v->u.rail.last_speed * 10 / 16);
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
		int wid = e->we.click.widget;
		Vehicle *v = GetVehicle(w->window_number);

		switch (wid) {
		case 5: /* start/stop train */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_START_STOP_TRAIN | CMD_MSG(STR_883B_CAN_T_STOP_START_TRAIN));
			break;
		case 6: /* center main view */
			ScrollMainWindowTo(v->x_pos, v->y_pos);
			break;
		case 7: /* goto depot */
			/* TrainGotoDepot has a nice randomizer in the pathfinder, which causes desyncs... */
			DoCommandP(v->tile, v->index, _ctrl_pressed ? DEPOT_SERVICE : 0, NULL, CMD_SEND_TRAIN_TO_DEPOT | CMD_NO_TEST_IF_IN_NETWORK | CMD_MSG(STR_8830_CAN_T_SEND_TRAIN_TO_DEPOT));
			break;
		case 8: /* force proceed */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_FORCE_TRAIN_PROCEED | CMD_MSG(STR_8862_CAN_T_MAKE_TRAIN_PASS_SIGNAL));
			break;
		case 9: /* reverse direction */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_REVERSE_TRAIN_DIRECTION | CMD_MSG(STR_8869_CAN_T_REVERSE_DIRECTION));
			break;
		case 10: /* show train orders */
			ShowOrdersWindow(v);
			break;
		case 11: /* show train details */
			ShowTrainDetailsWindow(v);
			break;
		case 12:
			ShowVehicleRefitWindow(v, INVALID_VEH_ORDER_ID);
			break;
		case 13:
			DoCommandP(v->tile, v->index, _ctrl_pressed ? 1 : 0, NULL, CMD_CLONE_VEHICLE | CMD_MSG(STR_882B_CAN_T_BUILD_RAILROAD_VEHICLE));
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
		bool train_stopped = CheckTrainStoppedInDepot(v)  >= 0;

		/* Widget 7 (send to depot) must be hidden if the train is already stopped in hangar.
		 * Widget 13 (clone) should then be shown, since cloning is allowed only while in depot and stopped.
		 * This sytem allows to have two buttons, on top of each other.
		 * The same system applies to widget 9 and 12, reverse direction and refit*/
		if (train_stopped != IsWindowWidgetHidden(w, 7) || train_stopped == IsWindowWidgetHidden(w, 13)) {
			SetWindowWidgetHiddenState(w,  7, train_stopped);  // send to depot
			SetWindowWidgetHiddenState(w,  9, train_stopped);  // reverse direction
			SetWindowWidgetHiddenState(w, 12, !train_stopped); // refit
			SetWindowWidgetHiddenState(w, 13, !train_stopped); // clone
			SetWindowDirty(w);
		}
		break;
	}

	}
}

static const WindowDesc _train_view_desc = {
	WDP_AUTO, WDP_AUTO, 250, 134,
	WC_VEHICLE_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_train_view_widgets,
	TrainViewWndProc
};

void ShowTrainViewWindow(const Vehicle *v)
{
	Window *w = AllocateWindowDescFront(&_train_view_desc,v->index);

	if (w != NULL) {
		w->caption_color = v->owner;
		AssignWindowViewport(w, 3, 17, 0xE2, 0x66, w->window_number | (1 << 31), ZOOM_LVL_TRAIN);
	}
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
		SetDParam(0, GetCustomEngineName(v->engine_type));
		SetDParam(1, v->value);
		DrawString(x, y, STR_882D_VALUE, 0x10);
	} else {
		SetDParam(0, GetCustomEngineName(v->engine_type));
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

	SetDParam(2, v->u.rail.cached_max_speed * 10 / 16);
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
					DrawSprite(GetTrainImage(u, DIR_W), pal, x + WagonLengthToPixels(4 + dx), y + 6 + (is_custom_sprite(RailVehInfo(u->engine_type)->image_index) ? _traininfo_vehicle_pitch : 0));
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
	WDP_AUTO, WDP_AUTO, 370, 164,
	WC_VEHICLE_DETAILS,WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_train_details_widgets,
	TrainDetailsWndProc
};


static void ShowTrainDetailsWindow(const Vehicle *v)
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
