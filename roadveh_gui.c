#include "stdafx.h"
#include "ttd.h"
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

void Set_DPARAM_Road_Veh_Build_Window(uint16 engine_number)
{
	const RoadVehicleInfo *rvi = RoadVehInfo(engine_number);
	Engine *e;
	YearMonthDay ymd;

	SetDParam(0, rvi->base_cost * (_price.roadveh_base>>3)>>5);
	SetDParam(1, rvi->max_speed * 10 >> 5);
	SetDParam(2, rvi->running_cost * _price.roadveh_running >> 8);
	SetDParam(4, rvi->capacity);
	SetDParam(3, _cargoc.names_long_p[rvi->cargo_type]);

	e = &_engines[engine_number];
	SetDParam(6, e->lifelength);
	SetDParam(7, e->reliability * 100 >> 16);
	ConvertDayToYMD(&ymd, e->intro_date);
	SetDParam(5, ymd.year + 1920);
}

static void DrawRoadVehImage(Vehicle *v, int x, int y, VehicleID selection)
{
	int image = GetRoadVehImage(v, 6);
	uint32 ormod = SPRITE_PALETTE(PLAYER_SPRITE_COLOR(v->owner));
	if (v->vehstatus & VS_CRASHED) ormod = 0x3248000;
	DrawSprite(image | ormod, x+14, y+6);

	if (v->index == selection) {
		DrawFrameRect(x-1, y-1, x+28, y+12, 15, 0x10);
	}
}

static void RoadVehDetailsWndProc(Window *w, WindowEvent *e)
{
	Vehicle *v = &_vehicles[w->window_number];
	StringID str;
	int mod;

	switch(e->event) {
	case WE_PAINT:
		w->disabled_state = v->owner == _local_player ? 0 : (1 << 2);
		if (!_patches.servint_roadveh) // disable service-scroller when interval is set to disabled
			w->disabled_state |= (1 << 5) | (1 << 6);

		SetDParam(0, v->string_id);
		SetDParam(1, v->unitnumber);
		DrawWindowWidgets(w);

		/* Draw running cost */
		{
			int year = v->age / 366;
			StringID str;

			SetDParam(1, year);

			str = STR_0199_YEAR;
			if (year != 1) {
				str++;
				if (v->max_age - 366 < v->age)
					str++;
			}
			SetDParam(0, str);
			SetDParam(2, v->max_age / 366);
			SetDParam(3, RoadVehInfo(v->engine_type)->running_cost * _price.roadveh_running >> 8);
			DrawString(2, 15, STR_900D_AGE_RUNNING_COST_YR, 0);
		}

		/* Draw max speed */
		{
			SetDParam(0, v->max_speed * 10 >> 5);
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
		SetDParam(1, 1920 + v->build_year);
		SetDParam(2, v->value);
		DrawString(34, 57, STR_9011_BUILT_VALUE, 0);

		SetDParam(0, _cargoc.names_long_p[v->cargo_type]);
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
		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 2: /* rename */
			SetDParam(0, v->unitnumber);
			ShowQueryString(v->string_id, STR_902C_NAME_ROAD_VEHICLE, 31, 150, w->window_class, w->window_number);
			break;

		case 5: /* increase int */
			mod = _ctrl_pressed? 5 : 10;
			goto change_int;
		case 6: /* decrease int */
			mod = _ctrl_pressed? -5 : -10;
change_int:
			mod += v->service_interval;

			/*	%-based service interval max 5%-90%
					day-based service interval max 30-800 days */
			mod = _patches.servint_ispercent ? clamp(mod, MIN_SERVINT_PERCENT, MAX_SERVINT_PERCENT) : clamp(mod, MIN_SERVINT_DAYS, MAX_SERVINT_DAYS+1);
			if (mod == v->service_interval)
				return;

			DoCommandP(v->tile, v->index, mod, NULL, CMD_CHANGE_ROADVEH_SERVICE_INT | CMD_MSG(STR_018A_CAN_T_CHANGE_SERVICING));
			break;
		}
		break;

	case WE_4:
		if (FindWindowById(WC_VEHICLE_VIEW, w->window_number) == NULL)
			DeleteWindow(w);
		break;

	case WE_ON_EDIT_TEXT: {
		byte *b = e->edittext.str;
		if (*b == 0)
			return;
		memcpy(_decode_parameters, b, 32);
		DoCommandP(0, w->window_number, 0, NULL, CMD_NAME_VEHICLE | CMD_MSG(STR_902D_CAN_T_NAME_ROAD_VEHICLE));
	} break;

	}
}

static const Widget _roadveh_details_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,				STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   339,     0,    13, STR_900C_DETAILS,STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   340,   379,     0,    13, STR_01AA_NAME,		STR_902E_NAME_ROAD_VEHICLE},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   379,    14,    55, 0x0,							STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   379,    56,    88, 0x0,							STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    10,    89,    94, STR_0188,				STR_884D_INCREASE_SERVICING_INTERVAL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    10,    95,   100, STR_0189,				STR_884E_DECREASE_SERVICING_INTERVAL},
{     WWT_IMGBTN,   RESIZE_NONE,    14,    11,   379,    89,   100, 0x0,							STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _roadveh_details_desc = {
	-1,-1, 380, 101,
	WC_VEHICLE_DETAILS,WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_roadveh_details_widgets,
	RoadVehDetailsWndProc
};

static void ShowRoadVehDetailsWindow(Vehicle *v)
{
	Window *w;
	VehicleID veh = v->index;
	DeleteWindowById(WC_VEHICLE_ORDERS, veh);
	DeleteWindowById(WC_VEHICLE_DETAILS, veh);
	_alloc_wnd_parent_num = veh;
	w = AllocateWindowDesc(&_roadveh_details_desc);
	w->window_number = veh;
	w->caption_color = v->owner;
}

static void RoadVehViewWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		Vehicle *v = &_vehicles[w->window_number];
		StringID str;

		w->disabled_state = (v->owner != _local_player) ? (1<<8 | 1<<7) : 0;

		/* draw widgets & caption */
		SetDParam(0, v->string_id);
		SetDParam(1, v->unitnumber);
		DrawWindowWidgets(w);

		/* draw the flag */
		DrawSprite((v->vehstatus & VS_STOPPED) ? 0xC12  : 0xC13, 2, 105);

		if (v->u.road.crashed_ctr != 0) {
			str = STR_8863_CRASHED;
		} else if (v->breakdown_ctr == 1) {
			str = STR_885C_BROKEN_DOWN;
		} else if (v->vehstatus & VS_STOPPED) {
			str = STR_8861_STOPPED;
		} else {
			switch (v->current_order.type) {
			case OT_GOTO_STATION: {
				SetDParam(0, v->current_order.station);
				SetDParam(1, v->cur_speed * 10 >> 5);
				str = STR_HEADING_FOR_STATION + _patches.vehicle_speed;
			} break;

			case OT_GOTO_DEPOT: {
				Depot *dep = &_depots[v->current_order.station];
				SetDParam(0, dep->town_index);
				SetDParam(1, v->cur_speed * 10 >> 5);
				str = STR_HEADING_FOR_ROAD_DEPOT + _patches.vehicle_speed;
			} break;

			case OT_LOADING:
			case OT_LEAVESTATION:
				str = STR_882F_LOADING_UNLOADING;
				break;

			default:
				if (v->num_orders == 0) {
					str = STR_NO_ORDERS + _patches.vehicle_speed;
					SetDParam(0, v->cur_speed * 10 >> 5);
				} else
					str = STR_EMPTY;
				break;
			}
		}

		DrawStringCentered(125, 105, str, 0);
		DrawWindowViewport(w);
	} break;

	case WE_CLICK: {
		Vehicle *v = &_vehicles[w->window_number];

		switch(e->click.widget) {
		case 5: /* start stop */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_START_STOP_ROADVEH | CMD_MSG(STR_9015_CAN_T_STOP_START_ROAD_VEHICLE));
			break;
		case 6: /* center main view */
			ScrollMainWindowTo(v->x_pos, v->y_pos);
			break;
		case 7: /* goto hangar */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_SEND_ROADVEH_TO_DEPOT | CMD_MSG(STR_9018_CAN_T_SEND_VEHICLE_TO_DEPOT));
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
		}
	} break;

	case WE_DESTROY:
		DeleteWindowById(WC_VEHICLE_ORDERS, w->window_number);
		DeleteWindowById(WC_VEHICLE_DETAILS, w->window_number);
		break;
	}
}

static const Widget _roadveh_view_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,	STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   237,     0,    13, STR_9002,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   238,   249,     0,    13, 0x0,       STR_STICKY_BUTTON},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   231,    14,   103, 0x0,				STR_NULL},
{          WWT_6,   RESIZE_NONE,    14,     2,   229,    16,   101, 0x0,				STR_NULL},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,     0,   249,   104,   115, 0x0,				STR_901C_CURRENT_VEHICLE_ACTION},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,   232,   249,    14,    31, 0x2AB,			STR_901E_CENTER_MAIN_VIEW_ON_VEHICLE},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,   232,   249,    32,    49, 0x2AE,			STR_901F_SEND_VEHICLE_TO_DEPOT},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,   232,   249,    50,    67, 0x2CB,			STR_9020_FORCE_VEHICLE_TO_TURN_AROUND},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,   232,   249,    68,    85, 0x2B2,			STR_901D_SHOW_VEHICLE_S_ORDERS},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,   232,   249,    86,   103, 0x2B3,			STR_9021_SHOW_ROAD_VEHICLE_DETAILS},
{   WIDGETS_END},
};

static const WindowDesc _roadveh_view_desc = {
	-1,-1, 250, 116,
	WC_VEHICLE_VIEW,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_roadveh_view_widgets,
	RoadVehViewWndProc,
};

void ShowRoadVehViewWindow(Vehicle *v)
{
	Window *w;

	w = AllocateWindowDescFront(&_roadveh_view_desc, v->index);
	if (w) {
		w->caption_color = v->owner;
		AssignWindowViewport(w, 3, 17, 0xE2, 0x54, w->window_number | (1 << 31), 0);
	}
}


static void DrawNewRoadVehWindow(Window *w)
{
	if (w->window_number == 0)
		w->disabled_state = 1 << 5;

	// setup scroller
	{
		int count = 0;
		int num = NUM_ROAD_ENGINES;
		Engine *e = &_engines[ROAD_ENGINES_INDEX];
		do {
			if (HASBIT(e->player_avail, _local_player))
				count++;
		} while (++e,--num);
		SetVScrollCount(w, count);
	}

	DrawWindowWidgets(w);

	{
		int num = NUM_ROAD_ENGINES;
		Engine *e = &_engines[ROAD_ENGINES_INDEX];
		int x = 1;
		int y = 15;
		int sel = WP(w,buildtrain_d).sel_index;
		int pos = w->vscroll.pos;
		int engine_id = ROAD_ENGINES_INDEX;
		int selected_id = -1;

		do {
			if (HASBIT(e->player_avail, _local_player)) {
				if (sel==0) selected_id = engine_id;
				if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
					DrawString(x+59, y+2, GetCustomEngineName(engine_id), sel==0 ? 0xC : 0x10);
					DrawRoadVehEngine(x+29, y+6, engine_id, SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)));
					y += 14;
				}
				sel--;
			}
		} while (++engine_id, ++e,--num);

		WP(w,buildtrain_d).sel_engine = selected_id;
		if (selected_id != -1) {
			Set_DPARAM_Road_Veh_Build_Window(selected_id);

			DrawString(2, w->widget[4].top + 1, STR_9008_COST_SPEED_RUNNING_COST, 0);
		}
	}
}

void CcBuildRoadVeh(bool success, uint tile, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (!success) return;

	v = &_vehicles[_new_roadveh_id];
	if (v->tile == _backup_orders_tile) {
		_backup_orders_tile = 0;
		RestoreVehicleOrders(v, _backup_orders_data);
	}
	ShowRoadVehViewWindow(v);
}

static void NewRoadVehWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT:
		DrawNewRoadVehWindow(w);
		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 2: { /* listbox */
			uint i = (e->click.pt.y - 14) / 14;
			if (i < w->vscroll.cap) {
				WP(w,buildtrain_d).sel_index = i + w->vscroll.pos;
				SetWindowDirty(w);
			}
		} break;

		case 5: { /* build */
			int sel_eng = WP(w,buildtrain_d).sel_engine;
			if (sel_eng != -1)
				DoCommandP(w->window_number, sel_eng, 0, CcBuildRoadVeh, CMD_BUILD_ROAD_VEH | CMD_MSG(STR_9009_CAN_T_BUILD_ROAD_VEHICLE));
		} break;

		case 6: /* rename */
			WP(w,buildtrain_d).rename_engine = WP(w,buildtrain_d).sel_engine;
			ShowQueryString(
				GetCustomEngineName(WP(w,buildtrain_d).sel_engine),
				STR_9036_RENAME_ROAD_VEHICLE_TYPE,
				31,
				160,
				w->window_class,
				w->window_number);
			break;
		}
		break;

	case WE_4:
		if (w->window_number != 0 && !FindWindowById(WC_VEHICLE_DEPOT, w->window_number)) {
			DeleteWindow(w);
		}
		break;

	case WE_ON_EDIT_TEXT: {
		byte *b = e->edittext.str;
		if (*b == 0)
			return;
		memcpy(_decode_parameters, b, 32);
		DoCommandP(0, WP(w,buildtrain_d).rename_engine, 0, NULL, CMD_RENAME_ENGINE | CMD_MSG(STR_9037_CAN_T_RENAME_ROAD_VEHICLE));
	} break;

	case WE_RESIZE: {
		if (e->sizing.diff.y == 0)
			break;

		w->vscroll.cap += e->sizing.diff.y / 14;
		w->widget[2].unkA = (w->vscroll.cap << 8) + 1;
	} break;

	}
}

static const Widget _new_road_veh_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,							STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   247,     0,    13, STR_9006_NEW_ROAD_VEHICLES, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   235,    14,   125, 0x801,									STR_9026_ROAD_VEHICLE_SELECTION},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   236,   247,    14,   125, 0x0,										STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_IMGBTN,     RESIZE_TB,    14,     0,   247,   126,   177, 0x0,										STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   117,   178,   189, STR_9007_BUILD_VEHICLE,STR_9027_BUILD_THE_HIGHLIGHTED_ROAD},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   118,   235,   178,   189, STR_9034_RENAME,				STR_9035_RENAME_ROAD_VEHICLE_TYPE},
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   236,   247,   178,   189, 0x0,										STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const WindowDesc _new_road_veh_desc = {
	-1, -1, 248, 190,
	WC_BUILD_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_new_road_veh_widgets,
	NewRoadVehWndProc
};

static void ShowBuildRoadVehWindow(TileIndex tile)
{
	Window *w;

	DeleteWindowById(WC_BUILD_VEHICLE, tile);

	w = AllocateWindowDesc(&_new_road_veh_desc);
	w->window_number = tile;
	w->vscroll.cap = 8;
	w->widget[2].unkA = (w->vscroll.cap << 8) + 1;

	w->resize.step_height = 14;
	w->resize.height = w->height - 14 * 4; /* Minimum of 4 vehicles in the display */

	if (tile != 0) {
		w->caption_color = _map_owner[tile];
	} else {
		w->caption_color = _local_player;
	}
}

static void DrawRoadDepotWindow(Window *w)
{
	uint tile;
	Vehicle *v;
	int num,x,y;
	Depot *d;

	tile = w->window_number;

	/* setup disabled buttons */
	w->disabled_state = (_map_owner[tile]==_local_player) ? 0 : ((1<<5)|(1<<7));

	/* determine amount of items for scroller */
	num = 0;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Road && v->u.road.state == 254 &&
				v->tile == (TileIndex)tile)
					num++;
	}
	SetVScrollCount(w, (num + w->hscroll.cap - 1) / w->hscroll.cap);

	/* locate the depot struct */
	for (d = _depots; d->xy != (TileIndex)tile; d++) {
		assert(d < endof(_depots));
	}

	SetDParam(0, d->town_index);
	DrawWindowWidgets(w);

	x = 2;
	y = 15;
	num = w->vscroll.pos * w->hscroll.cap;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Road &&
				v->u.road.state == 254 &&
				v->tile == (TileIndex)tile &&
				--num < 0 && num >=	-w->vscroll.cap * w->hscroll.cap) {

			DrawRoadVehImage(v, x+24, y, WP(w,traindepot_d).sel);

			SetDParam(0, v->unitnumber);
			DrawString(x, y+2, (uint16)(v->max_age-366) >= v->age ? STR_00E2 : STR_00E3, 0);

			DrawSprite( (v->vehstatus & VS_STOPPED) ? 0xC12 : 0xC13, x + 16, y);

			if ((x+=56) == 2 + 56 * w->hscroll.cap) {
				x = 2;
				y += 14;
			}
		}
	}
}

static int GetVehicleFromRoadDepotWndPt(Window *w, int x, int y, Vehicle **veh)
{
	uint xt,yt,xm;
	TileIndex tile;
	Vehicle *v;
	int pos;

	xt = x / 56;
	xm = x % 56;
	if (xt >= 5)
		return 1;

	yt = (y - 14) / 14;
	if (yt >= 3)
		return 1;

	pos = (yt + w->vscroll.pos) * 5 + xt;

	tile = w->window_number;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Road &&
				v->u.road.state == 254 &&
				v->tile == (TileIndex)tile &&
				--pos < 0) {
					*veh = v;
					if (xm >= 24)
						return 0;

					if (xm <= 16)
						return -1; /* show window */

					return -2; /* start stop */
				}
	}

	return 1; /* outside */
}

static void RoadDepotClickVeh(Window *w, int x, int y)
{
	Vehicle *v;
	int mode;

	mode = GetVehicleFromRoadDepotWndPt(w, x, y, &v);
	if (mode > 0) return;

	// share / copy orders
	if (_thd.place_mode && mode <= 0) { _place_clicked_vehicle = v; return; }

	switch (mode) {
	case 0: // start dragging of vehicle
		if (v != NULL) {
			WP(w,traindepot_d).sel = v->index;
			SetWindowDirty(w);
			SetObjectToPlaceWnd( SPRITE_PALETTE(PLAYER_SPRITE_COLOR(v->owner)) + GetRoadVehImage(v, 6), 4, w);
		}
		break;

	case -1: // show info window
		ShowRoadVehViewWindow(v);
		break;

	case -2: // click start/stop flag
		DoCommandP(v->tile, v->index, 0, NULL, CMD_START_STOP_ROADVEH | CMD_MSG(STR_9015_CAN_T_STOP_START_ROAD_VEHICLE));
		break;

	default:
		NOT_REACHED();
	}
}

static void RoadDepotWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT:
		DrawRoadDepotWindow(w);
		break;

	case WE_CLICK: {
		switch(e->click.widget) {
		case 3:
			RoadDepotClickVeh(w, e->click.pt.x, e->click.pt.y);
			break;

		case 7:
			ShowBuildRoadVehWindow(w->window_number);
			break;

		case 8: /* scroll to tile */
			ScrollMainWindowToTile(w->window_number);
			break;
		}
	} break;

	case WE_DESTROY:
		DeleteWindowById(WC_BUILD_VEHICLE, w->window_number);
		break;

	case WE_DRAGDROP: {
		switch(e->click.widget) {
		case 3: {
			Vehicle *v;
			VehicleID sel = WP(w,traindepot_d).sel;

			WP(w,traindepot_d).sel = INVALID_VEHICLE;
			SetWindowDirty(w);

			if (GetVehicleFromRoadDepotWndPt(w, e->dragdrop.pt.x, e->dragdrop.pt.y, &v) == 0 &&
					v != NULL &&
					sel == v->index) {
				ShowRoadVehViewWindow(v);
			}
		} break;

		case 5:
			if (!HASBIT(w->disabled_state, 5) &&
					WP(w,traindepot_d).sel != INVALID_VEHICLE)	{
				Vehicle *v;

				HandleButtonClick(w, 5);

				v = &_vehicles[WP(w,traindepot_d).sel];
				WP(w,traindepot_d).sel = INVALID_VEHICLE;

				_backup_orders_tile = v->tile;
				BackupVehicleOrders(v, _backup_orders_data);

				if (!DoCommandP(v->tile, v->index, 0, NULL, CMD_SELL_ROAD_VEH | CMD_MSG(STR_9014_CAN_T_SELL_ROAD_VEHICLE)))
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

	case WE_RESIZE: {
		/* Update the scroll + matrix */
		w->vscroll.cap += e->sizing.diff.y / 14;
		w->hscroll.cap += e->sizing.diff.x / 56;
		w->widget[3].unkA = (w->vscroll.cap << 8) + w->hscroll.cap;

	} break;

	}

}

static const Widget _road_depot_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5, 										STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   302,     0,    13, STR_9003_ROAD_VEHICLE_DEPOT,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   303,   314,     0,    13, 0x0,													STR_STICKY_BUTTON},
{     WWT_MATRIX,     RESIZE_RB,    14,     0,   279,    14,    55, 0x305,												STR_9022_VEHICLES_CLICK_ON_VEHICLE},
{      WWT_PANEL,    RESIZE_LRB,    14,   280,   302,    14,    13, 0x0,													STR_NULL},
{     WWT_IMGBTN,   RESIZE_LRTB,    14,   280,   302,    14,    55, 0x2A9,												STR_9024_DRAG_ROAD_VEHICLE_TO_HERE},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   303,   314,    14,    55, 0x0,													STR_0190_SCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   150,    56,    67, STR_9004_NEW_VEHICLES,				STR_9023_BUILD_NEW_ROAD_VEHICLE},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   151,   302,    56,    67, STR_00E4_LOCATION,						STR_9025_CENTER_MAIN_VIEW_ON_ROAD},
{      WWT_PANEL,    RESIZE_RTB,    14,   303,   302,    56,    67, 0x0,													STR_NULL},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   303,   314,    56,    67, 0x0,													STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const WindowDesc _road_depot_desc = {
	-1, -1, 315, 68,
	WC_VEHICLE_DEPOT,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_road_depot_widgets,
	RoadDepotWndProc
};

void ShowRoadDepotWindow(uint tile)
{
	Window *w;

	w = AllocateWindowDescFront(&_road_depot_desc, tile);
	if (w) {
		w->caption_color = _map_owner[w->window_number];
		w->hscroll.cap = 5;
		w->vscroll.cap = 3;
		w->resize.step_width = 56;
		w->resize.step_height = 14;
		WP(w,traindepot_d).sel = INVALID_VEHICLE;
		_backup_orders_tile = 0;
	}
}

static const Widget _player_roadveh_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,								STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   247,     0,    13, STR_9001_ROAD_VEHICLES,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   248,   259,     0,    13, 0x0,                     STR_STICKY_BUTTON},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    80,    14,    25, SRT_SORT_BY,							STR_SORT_ORDER_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,    81,   235,    14,    25, 0x0,											STR_SORT_CRITERIA_TIP},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   236,   247,    14,    25, STR_0225,								STR_SORT_CRITERIA_TIP},
{      WWT_PANEL,  RESIZE_RIGHT,    14,   248,   259,    14,    25, 0x0,											STR_NULL},
{     WWT_MATRIX,     RESIZE_RB,    14,     0,   247,    26,   207, 0x701,										STR_901A_ROAD_VEHICLES_CLICK_ON},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   248,   259,    26,   207, 0x0,											STR_0190_SCROLL_BAR_SCROLLS_LIST},
/* only for our road list, a 'Build Vehicle' button that opens the depot of the last built depot */
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   124,   208,   219, STR_8815_NEW_VEHICLES,		STR_901B_BUILD_NEW_ROAD_VEHICLES},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   125,   247,   208,   219, STR_REPLACE_VEHICLES,    STR_REPLACE_HELP},
{      WWT_PANEL,    RESIZE_RTB,    14,   248,   247,   208,   219, 0x0,											STR_NULL},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   248,   259,   208,   219, 0x0,											STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const Widget _other_player_roadveh_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,								STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   247,     0,    13, STR_9001_ROAD_VEHICLES,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   248,   259,     0,    13, 0x0,                     STR_STICKY_BUTTON},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    80,    14,    25, SRT_SORT_BY,							STR_SORT_ORDER_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,    81,   235,    14,    25, 0x0,											STR_SORT_CRITERIA_TIP},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   236,   247,    14,    25, STR_0225,								STR_SORT_CRITERIA_TIP},
{      WWT_PANEL,  RESIZE_RIGHT,    14,   248,   259,    14,    25, 0x0,											STR_NULL},
{     WWT_MATRIX,     RESIZE_RB,    14,     0,   247,    26,   207, 0x701,										STR_901A_ROAD_VEHICLES_CLICK_ON},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   248,   259,    26,   207, 0x0,											STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,    RESIZE_RTB,    14,     0,   247,   208,   219, 0x0,											STR_NULL},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   248,   259,   208,   219, 0x0,											STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static void PlayerRoadVehWndProc(Window *w, WindowEvent *e)
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

		BuildVehicleList(vl, VEH_Road, owner, station);
		SortVehicleList(vl);

		SetVScrollCount(w, vl->list_length);

		// disable 'Sort By' tooltip on Unsorted sorting criteria
		if (vl->sort_type == SORT_BY_UNSORTED)
			w->disabled_state |= (1 << 3);

		/* draw the widgets */
		{
			const Player *p = DEREF_PLAYER(owner);
			if (station == -1) {
				/* Company Name -- (###) Road vehicles */
				SetDParam(0, p->name_1);
				SetDParam(1, p->name_2);
				SetDParam(2, w->vscroll.count);
				w->widget[1].unkA = STR_9001_ROAD_VEHICLES;
			} else {
				/* Station Name -- (###) Road vehicles */
				SetDParam(0, DEREF_STATION(station)->index);
				SetDParam(1, w->vscroll.count);
				w->widget[1].unkA = STR_SCHEDULED_ROAD_VEHICLES;
			}
			DrawWindowWidgets(w);
		}
		/* draw sorting criteria string */
		DrawString(85, 15, _vehicle_sort_listing[vl->sort_type], 0x10);
		/* draw arrow pointing up/down for ascending/descending sorting */
		DoDrawString(
			vl->flags & VL_DESC ? "\xAA" : "\xA0", 69, 15, 0x10);

		max = min(w->vscroll.pos + w->vscroll.cap, vl->list_length);
		for (i = w->vscroll.pos; i < max; ++i) {
			Vehicle *v = DEREF_VEHICLE(vl->sort_list[i].index);
			StringID str;

			assert(v->type == VEH_Road && v->owner == owner);

			DrawRoadVehImage(v, x + 22, y + 6, INVALID_VEHICLE);
			DrawVehicleProfitButton(v, x, y + 13);

			SetDParam(0, v->unitnumber);
			if (IsRoadDepotTile(v->tile))
				str = STR_021F;
			else
				str = v->age > v->max_age - 366 ? STR_00E3 : STR_00E2;
			DrawString(x, y + 2, str, 0);

			SetDParam(0, v->profit_this_year);
			SetDParam(1, v->profit_last_year);
			DrawString(x + 24, y + 18, STR_0198_PROFIT_THIS_YEAR_LAST_YEAR, 0);

			if (v->string_id != STR_SV_ROADVEH_NAME) {
				SetDParam(0, v->string_id);
				DrawString(x + 24, y, STR_01AB, 0);
			}

			y += PLY_WND_PRC__SIZE_OF_ROW_SMALL;
		}
		}	break;

	case WE_CLICK: {
		switch(e->click.widget) {
		case 3: /* Flip sorting method ascending/descending */
			vl->flags ^= VL_DESC;
			vl->flags |= VL_RESORT;
			SetWindowDirty(w);
			break;

		case 4: case 5:/* Select sorting criteria dropdown menu */
			ShowDropDownMenu(w, _vehicle_sort_listing, vl->sort_type, 5, 0, 0);
			return;
		case 7: { /* Matrix to show vehicles */
			uint32 id_v = (e->click.pt.y - PLY_WND_PRC__OFFSET_TOP_WIDGET) / PLY_WND_PRC__SIZE_OF_ROW_SMALL;

			if (id_v >= w->vscroll.cap) { return;} // click out of bounds

			id_v += w->vscroll.pos;

			{
				Vehicle *v;

				if (id_v >= vl->list_length) return; // click out of list bound

				v	= DEREF_VEHICLE(vl->sort_list[id_v].index);

				assert(v->type == VEH_Road && v->owner == owner);

				ShowRoadVehViewWindow(v);
			}
		} break;

		case 9: { /* Build new Vehicle */
			uint tile;

			if (!IsWindowOfPrototype(w, _player_roadveh_widgets))
				break;

			tile = _last_built_road_depot_tile;
			do {
				if (_map_owner[tile] == _local_player && IsRoadDepotTile(tile)) {
					ShowRoadDepotWindow(tile);
					ShowBuildRoadVehWindow(tile);
					return;
				}

				tile = TILE_MASK(tile + 1);
			} while(tile != _last_built_road_depot_tile);

			ShowBuildRoadVehWindow(0);
		} break;
		case 10: {
			if (!IsWindowOfPrototype(w, _player_roadveh_widgets))
				break;

			ShowReplaceVehicleWindow(VEH_Road);
			break;
		}
		}
	}	break;

	case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
		if (vl->sort_type != e->dropdown.index) {
			// value has changed -> resort
			vl->flags |= VL_RESORT;
			vl->sort_type = e->dropdown.index;

			// enable 'Sort By' if a sorter criteria is chosen
			if (vl->sort_type != SORT_BY_UNSORTED)
				w->disabled_state &= ~(1 << 3);
		}
		SetWindowDirty(w);
		break;

	case WE_CREATE: /* set up resort timer */
		vl->sort_list = NULL;
		vl->flags = VL_REBUILD;
		vl->sort_type = SORT_BY_UNSORTED;
		vl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
		break;

	case WE_DESTROY:
		free(vl->sort_list);
		break;

	case WE_TICK: /* resort the list every 20 seconds orso (10 days) */
		if (--vl->resort_timer == 0) {
			DEBUG(misc, 1) ("Periodic resort road vehicles list player %d station %d",
				owner, station);
			vl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
			vl->flags |= VL_RESORT;
			SetWindowDirty(w);
		}
		break;


	case WE_RESIZE:
		/* Update the scroll + matrix */
		w->vscroll.cap += e->sizing.diff.y / PLY_WND_PRC__SIZE_OF_ROW_SMALL;
		w->widget[7].unkA = (w->vscroll.cap << 8) + 1;
		break;
	}
}

static const WindowDesc _player_roadveh_desc = {
	-1, -1, 260, 220,
	WC_ROADVEH_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_player_roadveh_widgets,
	PlayerRoadVehWndProc
};

static const WindowDesc _other_player_roadveh_desc = {
	-1, -1, 260, 220,
	WC_ROADVEH_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_other_player_roadveh_widgets,
	PlayerRoadVehWndProc
};


void ShowPlayerRoadVehicles(int player, int station)
{
	Window *w;

	if ( player == _local_player) {
		w = AllocateWindowDescFront(&_player_roadveh_desc, (station << 16) | player);
	} else  {
		w = AllocateWindowDescFront(&_other_player_roadveh_desc, (station << 16) | player);
	}
	if (w) {
		w->caption_color = player;
		w->vscroll.cap = 7; // maximum number of vehicles shown
		w->widget[7].unkA = (w->vscroll.cap << 8) + 1;
		w->resize.step_height = PLY_WND_PRC__SIZE_OF_ROW_SMALL;
		w->resize.height = 220 - (PLY_WND_PRC__SIZE_OF_ROW_SMALL * 3); /* Minimum of 4 vehicles */
	}
}
