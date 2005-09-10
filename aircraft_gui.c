/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
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
#include "vehicle_gui.h"

/**
 * Draw the purchase info details of an aircraft at a given location.
 * @param x,y location where to draw the info
 * @param engine_number the engine of which to draw the info of
 */
void DrawAircraftPurchaseInfo(int x, int y, EngineID engine_number)
{
	const AircraftVehicleInfo *avi = AircraftVehInfo(engine_number);
	const Engine* e = GetEngine(engine_number);
	YearMonthDay ymd;
	ConvertDayToYMD(&ymd, e->intro_date);

	/* Purchase cost - Max speed */
	SetDParam(0, avi->base_cost * (_price.aircraft_base>>3)>>5);
	SetDParam(1, avi->max_speed * 8);
	DrawString(x, y, STR_PURCHASE_INFO_COST_SPEED, 0);
	y += 10;

	/* Cargo capacity */
	SetDParam(0, avi->passenger_capacity);
	SetDParam(1, avi->mail_capacity);
	DrawString(x, y, STR_PURCHASE_INFO_AIRCRAFT_CAPACITY, 0);
	y += 10;

	/* Running cost */
	SetDParam(0, avi->running_cost * _price.aircraft_running >> 8);
	DrawString(x, y, STR_PURCHASE_INFO_RUNNINGCOST, 0);
	y += 10;

	/* Design date - Life length */
	SetDParam(0, ymd.year + 1920);
	SetDParam(1, e->lifelength);
	DrawString(x, y, STR_PURCHASE_INFO_DESIGNED_LIFE, 0);
	y += 10;

	/* Reliability */
	SetDParam(0, e->reliability * 100 >> 16);
	DrawString(x, y, STR_PURCHASE_INFO_RELIABILITY, 0);
	y += 10;
}

static void DrawAircraftImage(const Vehicle *v, int x, int y, VehicleID selection)
{
	int image = GetAircraftImage(v, 6);
	uint32 ormod = SPRITE_PALETTE(PLAYER_SPRITE_COLOR(v->owner));
	if (v->vehstatus & VS_CRASHED)
		ormod = PALETTE_CRASH;
	DrawSprite(image | ormod, x+25, y+10);
	if (v->subtype == 0)
		DrawSprite(0xF3D, x+25, y+5);
	if (v->index == selection) {
		DrawFrameRect(x-1, y-1, x+58, y+21, 0xF, FR_BORDERONLY);
	}
}

void CcBuildAircraft(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (success) {
		v = GetVehicle(_new_aircraft_id);
		if (v->tile == _backup_orders_tile) {
			_backup_orders_tile = 0;
			RestoreVehicleOrders(v, _backup_orders_data);
		}
		ShowAircraftViewWindow(v);
	}
}

void CcCloneAircraft(bool success, uint tile, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (success) {
		v = GetVehicle(_new_aircraft_id);
		ShowAircraftViewWindow(v);
	}
}

static void NewAircraftWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {

	case WE_PAINT: {
		if (w->window_number == 0)
			SETBIT(w->disabled_state, 5);

		{
			int count = 0;
			int num = NUM_AIRCRAFT_ENGINES;
			const Engine* e = GetEngine(AIRCRAFT_ENGINES_INDEX);

			do {
				if (HASBIT(e->player_avail, _local_player))
					count++;
			} while (++e,--num);
			SetVScrollCount(w, count);
		}

		DrawWindowWidgets(w);

		{
			int num = NUM_AIRCRAFT_ENGINES;
			const Engine* e = GetEngine(AIRCRAFT_ENGINES_INDEX);
			int x = 2;
			int y = 15;
			int sel = WP(w,buildtrain_d).sel_index;
			int pos = w->vscroll.pos;
			int engine_id = AIRCRAFT_ENGINES_INDEX;
			int selected_id = -1;

			do {
				if (HASBIT(e->player_avail, _local_player)) {
					if (sel==0) selected_id = engine_id;
					if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
						DrawString(x+62, y+7, GetCustomEngineName(engine_id), sel==0 ? 0xC : 0x10);
						DrawAircraftEngine(x+29, y+10, engine_id, SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)));
						y += 24;
					}
					sel--;
				}
			} while (++engine_id, ++e,--num);

			WP(w,buildtrain_d).sel_engine = selected_id;

			if (selected_id != -1) {
				DrawAircraftPurchaseInfo(2, w->widget[4].top + 1, selected_id);
			}
		}
	} break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 2: { /* listbox */
			uint i = (e->click.pt.y - 14) / 24;
			if (i < w->vscroll.cap) {
				WP(w,buildtrain_d).sel_index = i + w->vscroll.pos;
				SetWindowDirty(w);
			}
		} break;

		case 5: { /* build */
			int sel_eng = WP(w,buildtrain_d).sel_engine;
			if (sel_eng != -1)
				DoCommandP(w->window_number, sel_eng, 0, CcBuildAircraft, CMD_BUILD_AIRCRAFT | CMD_MSG(STR_A008_CAN_T_BUILD_AIRCRAFT));
		} break;

		case 6:	{ /* rename */
			int sel_eng = WP(w,buildtrain_d).sel_engine;
			if (sel_eng != -1) {
				WP(w,buildtrain_d).rename_engine = sel_eng;
				ShowQueryString(GetCustomEngineName(sel_eng),
					STR_A039_RENAME_AIRCRAFT_TYPE, 31, 160, w->window_class, w->window_number);
			}
		} break;
		}
		break;

	case WE_4:
		if (w->window_number != 0 && !FindWindowById(WC_VEHICLE_DEPOT, w->window_number)) {
			DeleteWindow(w);
		}
		break;

	case WE_ON_EDIT_TEXT: {
		if (e->edittext.str[0] != '\0') {
			_cmd_text = e->edittext.str;
			DoCommandP(0, WP(w, buildtrain_d).rename_engine, 0, NULL,
				CMD_RENAME_ENGINE | CMD_MSG(STR_A03A_CAN_T_RENAME_AIRCRAFT_TYPE));
		}
	} break;

	case WE_RESIZE:
		w->vscroll.cap += e->sizing.diff.y / 24;
		w->widget[2].unkA = (w->vscroll.cap << 8) + 1;
		break;
	}
}

static const Widget _new_aircraft_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,								STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   239,     0,    13, STR_A005_NEW_AIRCRAFT,		STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   227,    14,   109, 0x401,										STR_A025_AIRCRAFT_SELECTION_LIST},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   228,   239,    14,   109, 0x0,											STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_IMGBTN,     RESIZE_TB,    14,     0,   239,   110,   161, 0x0,											STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   114,   162,   173, STR_A006_BUILD_AIRCRAFT,	STR_A026_BUILD_THE_HIGHLIGHTED_AIRCRAFT},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   115,   227,   162,   173, STR_A037_RENAME,					STR_A038_RENAME_AIRCRAFT_TYPE},
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   228,   239,   162,   173, 0x0,											STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const WindowDesc _new_aircraft_desc = {
	-1, -1, 240, 174,
	WC_BUILD_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_new_aircraft_widgets,
	NewAircraftWndProc
};

static void ShowBuildAircraftWindow(TileIndex tile)
{
	Window *w;

	DeleteWindowById(WC_BUILD_VEHICLE, tile);

	w = AllocateWindowDesc(&_new_aircraft_desc);
	w->window_number = tile;
	w->vscroll.cap = 4;
	w->widget[2].unkA = (w->vscroll.cap << 8) + 1;

	w->resize.step_height = 24;

	if (tile != 0) {
		w->caption_color = GetTileOwner(tile);
	} else {
		w->caption_color = _local_player;
	}
}

static void AircraftRefitWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		const Vehicle *v = GetVehicle(w->window_number);

		SetDParam(0, v->string_id);
		SetDParam(1, v->unitnumber);
		DrawWindowWidgets(w);

		DrawString(1, 15, STR_A040_SELECT_CARGO_TYPE_TO_CARRY, 0);

		/* TODO: Support for custom GRFSpecial-specified refitting! --pasky */
		WP(w,refit_d).cargo = DrawVehicleRefitWindow(v, WP(w, refit_d).sel);

		if (WP(w,refit_d).cargo != CT_INVALID) {
			int32 cost = DoCommandByTile(v->tile, v->index, WP(w,refit_d).cargo, DC_QUERY_COST, CMD_REFIT_AIRCRAFT);
			if (!CmdFailed(cost)) {
				SetDParam(2, cost);
				SetDParam(0, _cargoc.names_long[WP(w,refit_d).cargo]);
				SetDParam(1, _aircraft_refit_capacity);
				DrawString(1, 137, STR_A041_NEW_CAPACITY_COST_OF_REFIT, 0);
			}
		}
	}	break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 2: { /* listbox */
			int y = e->click.pt.y - 25;
			if (y >= 0) {
				WP(w,refit_d).sel = y / 10;
				SetWindowDirty(w);
			}
		} break;
		case 4: /* refit button */
			if (WP(w,refit_d).cargo != CT_INVALID) {
				const Vehicle *v = GetVehicle(w->window_number);
				if (DoCommandP(v->tile, v->index, WP(w,refit_d).cargo, NULL, CMD_REFIT_AIRCRAFT | CMD_MSG(STR_A042_CAN_T_REFIT_AIRCRAFT)))
					DeleteWindow(w);
			}
		  break;
		}
		break;
	}
}

static const Widget _aircraft_refit_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,				STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   239,     0,    13, STR_A03C_REFIT,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   239,    14,   135, 0x0,							STR_A03E_SELECT_TYPE_OF_CARGO_FOR},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   239,   136,   157, 0x0,							STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,   239,   158,   169, STR_A03D_REFIT_AIRCRAFT, STR_A03F_REFIT_AIRCRAFT_TO_CARRY},
{   WIDGETS_END},
};

static const WindowDesc _aircraft_refit_desc = {
	-1,-1, 240, 170,
	WC_VEHICLE_REFIT,WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_aircraft_refit_widgets,
	AircraftRefitWndProc
};

static void ShowAircraftRefitWindow(Vehicle *v)
{
	Window *w;

	DeleteWindowById(WC_VEHICLE_REFIT, v->index);

	_alloc_wnd_parent_num = v->index;
	w = AllocateWindowDesc(&_aircraft_refit_desc);
	w->window_number = v->index;
	w->caption_color = v->owner;
	WP(w,refit_d).sel = -1;
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
			SetDParam(0, v->max_speed * 8);
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
					SetDParam(1, 1920 + v->build_year);
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
		switch (e->click.widget) {
		case 2: /* rename */
			v = GetVehicle(w->window_number);
			SetDParam(0, v->unitnumber);
			ShowQueryString(v->string_id, STR_A030_NAME_AIRCRAFT, 31, 150, w->window_class, w->window_number);
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

			DoCommandP(v->tile, v->index, mod, NULL, CMD_CHANGE_AIRCRAFT_SERVICE_INT | CMD_MSG(STR_018A_CAN_T_CHANGE_SERVICING));
			break;
		}
	} break;

	case WE_4:
		if (FindWindowById(WC_VEHICLE_VIEW, w->window_number) == NULL)
			DeleteWindow(w);
		break;

	case WE_ON_EDIT_TEXT: {
		if (e->edittext.str[0] != '\0') {
			_cmd_text = e->edittext.str;
			DoCommandP(0, w->window_number, 0, NULL,
				CMD_NAME_VEHICLE | CMD_MSG(STR_A031_CAN_T_NAME_AIRCRAFT));
		}
	} break;

	}
}


static const Widget _aircraft_details_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,					STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   349,     0,    13, STR_A00C_DETAILS,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   350,   389,     0,    13, STR_01AA_NAME,			STR_A032_NAME_AIRCRAFT},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   389,    14,    55, 0x0,								STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   389,    56,   101, 0x0,								STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    10,   102,   107, STR_0188,					STR_884D_INCREASE_SERVICING_INTERVAL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    10,   108,   113, STR_0189,					STR_884E_DECREASE_SERVICING_INTERVAL},
{     WWT_IMGBTN,   RESIZE_NONE,    14,    11,   389,   102,   113, 0x0,								STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _aircraft_details_desc = {
	-1,-1, 390, 114,
	WC_VEHICLE_DETAILS,WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_aircraft_details_widgets,
	AircraftDetailsWndProc
};


static void ShowAircraftDetailsWindow(Vehicle *v)
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
{ WWT_TEXTBTN,    RESIZE_NONE,  14,   0,  10,   0,  13, STR_00C5, STR_018B_CLOSE_WINDOW },
{ WWT_CAPTION,    RESIZE_RIGHT, 14,  11, 237,   0,  13, STR_A00A, STR_018C_WINDOW_TITLE_DRAG_THIS },
{ WWT_STICKYBOX,  RESIZE_LR,    14, 238, 249,   0,  13, 0x0,      STR_STICKY_BUTTON },
{ WWT_IMGBTN,     RESIZE_RB,    14,   0, 231,  14, 103, 0x0,      STR_NULL },
{ WWT_6,          RESIZE_RB,    14,   2, 229,  16, 101, 0x0,      STR_NULL },
{ WWT_PUSHIMGBTN, RESIZE_RTB,   14,   0, 237, 104, 115, 0x0,      STR_A027_CURRENT_AIRCRAFT_ACTION },
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  14,  31, 0x2AB,    STR_A029_CENTER_MAIN_VIEW_ON_AIRCRAFT },
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  32,  49, 0x2AF,    STR_A02A_SEND_AIRCRAFT_TO_HANGAR },
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  50,  67, 0x2B4,    STR_A03B_REFIT_AIRCRAFT_TO_CARRY },
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  68,  85, 0x2B2,    STR_A028_SHOW_AIRCRAFT_S_ORDERS },
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  86, 103, 0x2B3,    STR_A02B_SHOW_AIRCRAFT_DETAILS },
{ WWT_PUSHIMGBTN, RESIZE_LR,    14, 232, 249,  32,  49, SPR_CLONE_AIRCRAFT,      STR_CLONE_AIRCRAFT_INFO },
{ WWT_PANEL,      RESIZE_LRB,   14, 232, 249, 104, 103, 0x0,      STR_NULL },
{ WWT_RESIZEBOX,  RESIZE_LRTB,  14, 238, 249, 104, 115, 0x0,      STR_NULL },
{ WIDGETS_END }
};

bool CheckStoppedInHangar(Vehicle *v);

static void AircraftViewWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		Vehicle *v = GetVehicle(w->window_number);
		uint32 disabled = 1<<8;
		StringID str;

		{
			TileIndex tile = v->tile;

			if (IsTileType(tile, MP_STATION) &&
					(_m[tile].m5 == 32 || _m[tile].m5 == 65) &&
					v->vehstatus&VS_STOPPED)
						disabled = 0;
		}

		if (v->owner != _local_player)
			disabled |= 1<<8 | 1<<7;
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
				SetDParam(0, v->current_order.station);
				SetDParam(1, v->cur_speed * 8);
				str = STR_HEADING_FOR_STATION + _patches.vehicle_speed;
			} break;

			case OT_GOTO_DEPOT: {
				SetDParam(0, v->current_order.station);
				SetDParam(1, v->cur_speed * 8);
				str = STR_HEADING_FOR_HANGAR + _patches.vehicle_speed;
			} break;

			case OT_LOADING:
				str = STR_882F_LOADING_UNLOADING;
				break;

			default:
				if (v->num_orders == 0) {
					str = STR_NO_ORDERS + _patches.vehicle_speed;
					SetDParam(0, v->cur_speed * 8);
				} else
					str = STR_EMPTY;
				break;
			}
		}

		/* draw the flag plus orders */
		DrawSprite(v->vehstatus & VS_STOPPED ? 0xC12 : 0xC13, 2, w->widget[5].top + 1);
		DrawStringCenteredTruncated(w->widget[5].left + 8, w->widget[5].right, w->widget[5].top + 1, str, 0);
		DrawWindowViewport(w);
	} break;

	case WE_CLICK: {
		Vehicle *v = GetVehicle(w->window_number);

		switch(e->click.widget) {
		case 5: /* start stop */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_START_STOP_AIRCRAFT | CMD_MSG(STR_A016_CAN_T_STOP_START_AIRCRAFT));
			break;
		case 6: /* center main view */
			ScrollMainWindowTo(v->x_pos, v->y_pos);
			break;
		case 7: /* goto hangar */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_SEND_AIRCRAFT_TO_HANGAR | CMD_MSG(STR_A012_CAN_T_SEND_AIRCRAFT_TO));
			break;
		case 8: /* refit */
			ShowAircraftRefitWindow(v);
			break;
		case 9: /* show orders */
			ShowOrdersWindow(v);
			break;
		case 10: /* show details */
			ShowAircraftDetailsWindow(v);
			break;
		case 11: {
			/* clone vehicle */
			Vehicle *v;
			v = GetVehicle(w->window_number);
			DoCommandP(v->tile, v->index, _ctrl_pressed ? 1 : 0, CcCloneAircraft, CMD_CLONE_VEHICLE | CMD_MSG(STR_A008_CAN_T_BUILD_AIRCRAFT));
		} break;
		}
	} break;

	case WE_RESIZE:
		w->viewport->width  += e->sizing.diff.x;
		w->viewport->height += e->sizing.diff.y;
		w->viewport->virtual_width  += e->sizing.diff.x;
		w->viewport->virtual_height += e->sizing.diff.y;
		break;

	case WE_DESTROY:
		DeleteWindowById(WC_VEHICLE_ORDERS, w->window_number);
		DeleteWindowById(WC_VEHICLE_REFIT, w->window_number);
		DeleteWindowById(WC_VEHICLE_DETAILS, w->window_number);
		break;

		       case WE_MOUSELOOP:
               {
                       Vehicle *v;
                       uint32 h;
                       v = GetVehicle(w->window_number);
                       h = CheckStoppedInHangar(v) ? (1<< 7) : (1 << 11);
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


void ShowAircraftViewWindow(Vehicle *v)
{
	Window *w;

	w = AllocateWindowDescFront(&_aircraft_view_desc, v->index);
	if (w) {
		w->caption_color = v->owner;
		AssignWindowViewport(w, 3, 17, 0xE2, 0x54, w->window_number | (1 << 31), 0);
	}
}

static void DrawAircraftDepotWindow(Window *w)
{
	TileIndex tile;
	Vehicle *v;
	int num,x,y;

	tile = w->window_number;

	/* setup disabled buttons */
	w->disabled_state =
		IsTileOwner(tile, _local_player) ? 0 : ((1<<4) | (1<<7) | (1<<8));

	/* determine amount of items for scroller */
	num = 0;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Aircraft && v->subtype <= 2 && v->vehstatus&VS_HIDDEN &&
				v->tile == tile)
					num++;
	}
	SetVScrollCount(w, (num + w->hscroll.cap - 1) / w->hscroll.cap);

	SetDParam(0, _m[tile].m2);
	DrawWindowWidgets(w);

	x = 2;
	y = 15;
	num = w->vscroll.pos * w->hscroll.cap;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Aircraft &&
				v->subtype <= 2 &&
				v->vehstatus&VS_HIDDEN &&
				v->tile == tile &&
				--num < 0 && num >= -w->vscroll.cap * w->hscroll.cap) {

			DrawAircraftImage(v, x+12, y, WP(w,traindepot_d).sel);

			SetDParam(0, v->unitnumber);
			DrawString(x, y+2, (uint16)(v->max_age-366) >= v->age ? STR_00E2 : STR_00E3, 0);

			DrawSprite( (v->vehstatus & VS_STOPPED) ? 0xC12 : 0xC13, x, y+12);

			if ((x+=74) == 2 + 74 * w->hscroll.cap) {
				x = 2;
				y += 24;
			}
		}
	}
}

static int GetVehicleFromAircraftDepotWndPt(Window *w, int x, int y, Vehicle **veh) {
	uint xt,row,xm,ym;
	Vehicle *v;
	TileIndex tile;
	int pos;

	xt = x / 74;
	xm = x % 74;
	if (xt >= w->hscroll.cap)
		return 1;

	row = (y - 14) / 24;
	ym = (y - 14) % 24;
	if (row >= w->vscroll.cap)
		return 1;

	pos = (row + w->vscroll.pos) * w->hscroll.cap + xt;

	tile = w->window_number;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Aircraft && v->subtype <= 2 &&
				v->vehstatus & VS_HIDDEN && v->tile == tile &&
				--pos < 0) {
			*veh = v;
			if (xm >= 12) return 0;
			if (ym <= 12) return -1; /* show window */
			return -2; /* start stop */
		}
	}
	return 1; /* outside */
}

static void AircraftDepotClickAircraft(Window *w, int x, int y)
{
	Vehicle *v;
	int mode = GetVehicleFromAircraftDepotWndPt(w, x, y, &v);

	// share / copy orders
	if (_thd.place_mode && mode <= 0) { _place_clicked_vehicle = v; return; }

	switch(mode) {
	case 1:
		return;

	case 0: // start dragging of vehicle
		if (v != NULL) {
			WP(w,traindepot_d).sel = v->index;
			SetWindowDirty(w);
			SetObjectToPlaceWnd( SPRITE_PALETTE(PLAYER_SPRITE_COLOR(v->owner)) + GetAircraftImage(v, 6), 4, w);
		}
		break;

	case -1: // show info window
		ShowAircraftViewWindow(v);
		break;

	case -2: // click start/stop flag
		DoCommandP(v->tile, v->index, 0, NULL, CMD_START_STOP_AIRCRAFT | CMD_MSG(STR_A016_CAN_T_STOP_START_AIRCRAFT));
		break;

	default:
		NOT_REACHED();
	}
}

/**
 * Clones an aircraft
 * @param *v is the original vehicle to clone
 * @param *w is the window of the hangar where the clone is build
 */
static bool HandleCloneVehClick(Vehicle *v, Window *w)
{

	if (!v){
		return false;
	}

	if (v->type != VEH_Aircraft) {
		// it's not an aircraft, do nothing
		return false;
	}


    DoCommandP(w->window_number, v->index, _ctrl_pressed ? 1 : 0,CcCloneAircraft,CMD_CLONE_VEHICLE | CMD_MSG(STR_A008_CAN_T_BUILD_AIRCRAFT));

	ResetObjectToPlace();

	return true;
}

static void ClonePlaceObj(uint tile, Window *w)
{
	Vehicle *v;


	v = CheckMouseOverVehicle();
	if (v && HandleCloneVehClick(v, w))
		return;
}


static void AircraftDepotWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT:
		DrawAircraftDepotWindow(w);
		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 5: /* click aircraft */
			AircraftDepotClickAircraft(w, e->click.pt.x, e->click.pt.y);
			break;
		case 7: /* show build aircraft window */
			ResetObjectToPlace();
			ShowBuildAircraftWindow(w->window_number);
			break;

				case 8: /* clone button */
			InvalidateWidget(w, 8);
				TOGGLEBIT(w->click_state, 8);

				if (HASBIT(w->click_state, 8)) {
					_place_clicked_vehicle = NULL;
					SetObjectToPlaceWnd(SPR_CURSOR_CLONE, VHM_RECT, w);
				} else {
					ResetObjectToPlace();
				}
					break;
		case 9: /* scroll to tile */
			ResetObjectToPlace();
			ScrollMainWindowToTile(w->window_number);
			break;
		}
		break;


case WE_PLACE_OBJ: {
		ClonePlaceObj(e->place.tile, w);
	} break;

	case WE_ABORT_PLACE_OBJ: {
		CLRBIT(w->click_state, 8);
		InvalidateWidget(w, 8);
	} break;

	// check if a vehicle in a depot was clicked..
	case WE_MOUSELOOP: {
		Vehicle *v = _place_clicked_vehicle;
		// since OTTD checks all open depot windows, we will make sure that it triggers the one with a clicked clone button
		if (v != NULL && HASBIT(w->click_state, 8)) {
			_place_clicked_vehicle = NULL;
			HandleCloneVehClick( v, w);
		}
	} break;

	case WE_DESTROY:
		DeleteWindowById(WC_BUILD_VEHICLE, w->window_number);
		break;

	case WE_DRAGDROP: {
		switch(e->click.widget) {
		case 5: {
			Vehicle *v;
			VehicleID sel = WP(w,traindepot_d).sel;

			WP(w,traindepot_d).sel = INVALID_VEHICLE;
			SetWindowDirty(w);

			if (GetVehicleFromAircraftDepotWndPt(w, e->dragdrop.pt.x, e->dragdrop.pt.y, &v) == 0 &&
					v != NULL &&
					sel == v->index) {
				ShowAircraftViewWindow(v);
			}
		} break;

		case 4:
			if (!HASBIT(w->disabled_state, 4) &&
					WP(w,traindepot_d).sel != INVALID_VEHICLE)	{
				Vehicle *v;

				HandleButtonClick(w, 4);

				v = GetVehicle(WP(w,traindepot_d).sel);
				WP(w,traindepot_d).sel = INVALID_VEHICLE;

				_backup_orders_tile = v->tile;
				BackupVehicleOrders(v, _backup_orders_data);

				if (!DoCommandP(v->tile, v->index, 0, NULL,  CMD_SELL_AIRCRAFT | CMD_MSG(STR_A01C_CAN_T_SELL_AIRCRAFT)))
					_backup_orders_tile = 0;
			}
			break;
		default:
			WP(w,traindepot_d).sel = INVALID_VEHICLE;
			SetWindowDirty(w);
		}
		break;
	}
	break;

	case WE_RESIZE:
		w->vscroll.cap += e->sizing.diff.y / 24;
		w->hscroll.cap += e->sizing.diff.x / 74;
		w->widget[5].unkA = (w->vscroll.cap << 8) + w->hscroll.cap;
		break;
	}
}

static const Widget _aircraft_depot_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,									STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   318,     0,    13, STR_A002_AIRCRAFT_HANGAR,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   319,   330,     0,    13, 0x0,											STR_STICKY_BUTTON},
{      WWT_PANEL,    RESIZE_LRB,    14,   296,   318,    14,    13, 0x0,													STR_NULL},
{     WWT_IMGBTN,   RESIZE_LRTB,    14,   296,   318,    14,    61, 0x2A9,										STR_A023_DRAG_AIRCRAFT_TO_HERE_TO},

{     WWT_MATRIX,     RESIZE_RB,    14,     0,   295,    14,    61, 0x204,										STR_A021_AIRCRAFT_CLICK_ON_AIRCRAFT},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   319,   330,    14,    61, 0x0,											STR_0190_SCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   105,    62,    73, STR_A003_NEW_AIRCRAFT,		STR_A022_BUILD_NEW_AIRCRAFT},
{WWT_NODISTXTBTN,     RESIZE_TB,    14,   106,   212,    62,    73, STR_CLONE_AIRCRAFT,		STR_CLONE_AIRCRAFT_INFO_HANGAR_WINDOW},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   213,   318,    62,    73, STR_00E4_LOCATION,				STR_A024_CENTER_MAIN_VIEW_ON_HANGAR},
{      WWT_PANEL,    RESIZE_RTB,    14,   319,   318,    62,    73, 0x0,													STR_NULL},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   319,   330,    62,    73, 0x0,											STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const WindowDesc _aircraft_depot_desc = {
	-1, -1, 331, 74,
	WC_VEHICLE_DEPOT,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_aircraft_depot_widgets,
	AircraftDepotWndProc
};


void ShowAircraftDepotWindow(TileIndex tile)
{
	Window *w;

	w = AllocateWindowDescFront(&_aircraft_depot_desc, tile);
	if (w) {
		w->caption_color = GetTileOwner(tile);
		w->vscroll.cap = 2;
		w->hscroll.cap = 4;
		w->resize.step_width = 74;
		w->resize.step_height = 24;
		WP(w,traindepot_d).sel = INVALID_VEHICLE;
		_backup_orders_tile = 0;
	}
}

static void DrawSmallOrderList(Vehicle *v, int x, int y) {
	const Order *order;
	int sel, i = 0;

	sel = v->cur_order_index;

	FOR_VEHICLE_ORDERS(v, order) {
		if (sel == 0) {
			_stringwidth_base = 0xE0;
			DoDrawString( "\xAF", x-6, y, 16);
			_stringwidth_base = 0;
		}
		sel--;

		if (order->type == OT_GOTO_STATION) {
			SetDParam(0, order->station);
			DrawString(x, y, STR_A036, 0);

			y += 6;
			if (++i == 4)
				break;
		}
	}
}


static const Widget _player_aircraft_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,							STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   247,     0,    13, STR_A009_AIRCRAFT,			STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   248,   259,     0,    13, 0x0,                   STR_STICKY_BUTTON},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    80,    14,    25, SRT_SORT_BY,						STR_SORT_ORDER_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,    81,   235,    14,    25, 0x0,										STR_SORT_CRITERIA_TIP},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   236,   247,    14,    25, STR_0225,							STR_SORT_CRITERIA_TIP},
{      WWT_PANEL,  RESIZE_RIGHT,    14,   248,   259,    14,    25, 0x0,										STR_NULL},
{     WWT_MATRIX,     RESIZE_RB,    14,     0,   247,    26,   169, 0x401,									STR_A01F_AIRCRAFT_CLICK_ON_AIRCRAFT},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   248,   259,    26,   169, 0x0,										STR_0190_SCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   124,   170,   181, STR_A003_NEW_AIRCRAFT,	STR_A020_BUILD_NEW_AIRCRAFT_REQUIRES},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   125,   247,   170,   181, STR_REPLACE_VEHICLES,						STR_REPLACE_HELP},
{      WWT_PANEL,    RESIZE_RTB,    14,   248,   247,   170,   181, 0x0,											STR_NULL},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   248,   259,   170,   181, 0x0,											STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const Widget _other_player_aircraft_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,							STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   247,     0,    13, STR_A009_AIRCRAFT,			STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   248,   259,     0,    13, 0x0,                   STR_STICKY_BUTTON},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    80,    14,    25, SRT_SORT_BY,						STR_SORT_ORDER_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,    81,   235,    14,    25, 0x0,										STR_SORT_CRITERIA_TIP},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   236,   247,    14,    25, STR_0225,							STR_SORT_CRITERIA_TIP},
{      WWT_PANEL,  RESIZE_RIGHT,    14,   248,   259,    14,    25, 0x0,										STR_NULL},
{     WWT_MATRIX,     RESIZE_RB,    14,     0,   247,    26,   169, 0x401,									STR_A01F_AIRCRAFT_CLICK_ON_AIRCRAFT},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   248,   259,    26,   169, 0x0,										STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,    RESIZE_RTB,    14,     0,   247,   170,   181, 0x0,											STR_NULL},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   248,   259,   170,   181, 0x0,											STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static void PlayerAircraftWndProc(Window *w, WindowEvent *e)
{
	int station = (int)w->window_number >> 16;
	int owner = w->window_number & 0xff;
	vehiclelist_d *vl = &WP(w, vehiclelist_d);

	switch(e->event) {
	case WE_PAINT: {
		int x = 2;
		int y = PLY_WND_PRC__OFFSET_TOP_WIDGET;
		int max;
		int i;

		BuildVehicleList(vl, VEH_Aircraft, owner, station);
		SortVehicleList(vl);

		SetVScrollCount(w, vl->list_length);

		// disable 'Sort By' tooltip on Unsorted sorting criteria
		if (vl->sort_type == SORT_BY_UNSORTED)
			w->disabled_state |= (1 << 3);

		/* draw the widgets */
		{
			const Player *p = GetPlayer(owner);
			if (station == -1) {
				/* Company Name -- (###) Aircraft */
				SetDParam(0, p->name_1);
				SetDParam(1, p->name_2);
				SetDParam(2, w->vscroll.count);
				w->widget[1].unkA = STR_A009_AIRCRAFT;
			} else {
				/* Station Name -- (###) Aircraft */
				SetDParam(0, station);
				SetDParam(1, w->vscroll.count);
				w->widget[1].unkA = STR_SCHEDULED_AIRCRAFT;
			}
			DrawWindowWidgets(w);
		}
		/* draw sorting criteria string */
		DrawString(85, 15, _vehicle_sort_listing[vl->sort_type], 0x10);
		/* draw arrow pointing up/down for ascending/descending sorting */
		DoDrawString(vl->flags & VL_DESC ? DOWNARROW : UPARROW, 69, 15, 0x10);

		max = min(w->vscroll.pos + w->vscroll.cap, vl->list_length);
		for (i = w->vscroll.pos; i < max; ++i) {
			Vehicle *v = GetVehicle(vl->sort_list[i].index);
			StringID str;

			assert(v->type == VEH_Aircraft && v->subtype <= 2);

			DrawAircraftImage(v, x + 19, y + 6, INVALID_VEHICLE);
			DrawVehicleProfitButton(v, x, y + 13);

			SetDParam(0, v->unitnumber);
			if (IsAircraftHangarTile(v->tile) && (v->vehstatus & VS_HIDDEN))
				str = STR_021F;
			else
				str = v->age > v->max_age - 366 ? STR_00E3 : STR_00E2;
			DrawString(x, y + 2, str, 0);

			SetDParam(0, v->profit_this_year);
			SetDParam(1, v->profit_last_year);
			DrawString(x + 19, y + 28, STR_0198_PROFIT_THIS_YEAR_LAST_YEAR, 0);

			if (v->string_id != STR_SV_AIRCRAFT_NAME) {
				SetDParam(0, v->string_id);
				DrawString(x + 19, y, STR_01AB, 0);
			}

			DrawSmallOrderList(v, x + 136, y);

			y += PLY_WND_PRC__SIZE_OF_ROW_BIG;
		}
		}	break;

	case WE_CLICK: {
		switch(e->click.widget) {
		case 3: /* Flip sorting method ascending/descending */
			vl->flags ^= VL_DESC;
			vl->flags |= VL_RESORT;
			_sorting.aircraft.order = !!(vl->flags & VL_DESC);
			SetWindowDirty(w);
			break;

		case 4: case 5:/* Select sorting criteria dropdown menu */
			ShowDropDownMenu(w, _vehicle_sort_listing, vl->sort_type, 5, 0, 0);
			return;

		case 7: { /* Matrix to show vehicles */
			uint32 id_v = (e->click.pt.y - PLY_WND_PRC__OFFSET_TOP_WIDGET) / PLY_WND_PRC__SIZE_OF_ROW_BIG;

			if (id_v >= w->vscroll.cap) { return;} // click out of bounds

			id_v += w->vscroll.pos;

			{
				Vehicle *v;

				if (id_v >= vl->list_length) return; // click out of list bound

				v = GetVehicle(vl->sort_list[id_v].index);

				assert(v->type == VEH_Aircraft && v->subtype <= 2);

				ShowAircraftViewWindow(v);
			}
		} break;

		case 9: { /* Build new Vehicle */
			TileIndex tile;

			if (!IsWindowOfPrototype(w, _player_aircraft_widgets))
				break;

			tile = _last_built_aircraft_depot_tile;
			do {
				if (IsAircraftHangarTile(tile) && IsTileOwner(tile, _local_player)) {
					ShowAircraftDepotWindow(tile);
					ShowBuildAircraftWindow(tile);
					return;
				}

				tile = TILE_MASK(tile + 1);
			} while(tile != _last_built_aircraft_depot_tile);

			ShowBuildAircraftWindow(0);
		} break;

		case 10:
			if (!IsWindowOfPrototype(w, _player_aircraft_widgets))
				break;

			ShowReplaceVehicleWindow(VEH_Aircraft);
			break;

		}
	}	break;

	case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
		if (vl->sort_type != e->dropdown.index) {
			// value has changed -> resort
			vl->flags |= VL_RESORT;
			vl->sort_type = e->dropdown.index;
			_sorting.aircraft.criteria = vl->sort_type;

			// enable 'Sort By' if a sorter criteria is chosen
			if (vl->sort_type != SORT_BY_UNSORTED)
				CLRBIT(w->disabled_state, 3);
		}
		SetWindowDirty(w);
		break;

	case WE_CREATE: /* set up resort timer */
		vl->sort_list = NULL;
		vl->flags = VL_REBUILD | (_sorting.aircraft.order << (VL_DESC - 1));
		vl->sort_type = _sorting.aircraft.criteria;
		vl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
		break;

	case WE_DESTROY:
		free(vl->sort_list);
		break;

	case WE_TICK: /* resort the list every 20 seconds orso (10 days) */
		if (--vl->resort_timer == 0) {
			DEBUG(misc, 1) ("Periodic resort aircraft list player %d station %d",
				owner, station);
			vl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
			vl->flags |= VL_RESORT;
			SetWindowDirty(w);
		}
		break;

	case WE_RESIZE:
		/* Update the scroll + matrix */
		w->vscroll.cap += e->sizing.diff.y / PLY_WND_PRC__SIZE_OF_ROW_BIG;
		w->widget[7].unkA = (w->vscroll.cap << 8) + 1;
		break;
	}
}

static const WindowDesc _player_aircraft_desc = {
	-1, -1, 260, 182,
	WC_AIRCRAFT_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_player_aircraft_widgets,
	PlayerAircraftWndProc
};

static const WindowDesc _other_player_aircraft_desc = {
	-1, -1, 260, 182,
	WC_AIRCRAFT_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_other_player_aircraft_widgets,
	PlayerAircraftWndProc
};

void ShowPlayerAircraft(int player, int station)
{
	Window *w;

	if (player == _local_player) {
		w = AllocateWindowDescFront(&_player_aircraft_desc, (station << 16) | player);
	} else  {
		w = AllocateWindowDescFront(&_other_player_aircraft_desc, (station << 16) | player);
	}

	if (w) {
		w->caption_color = w->window_number;
		w->vscroll.cap = 4;
		w->widget[7].unkA = (w->vscroll.cap << 8) + 1;
		w->resize.step_height = PLY_WND_PRC__SIZE_OF_ROW_BIG;
	}
}
