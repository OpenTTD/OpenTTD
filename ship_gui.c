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

void Set_DPARAM_Ship_Build_Window(uint16 engine_number)
{
	YearMonthDay ymd;
	const ShipVehicleInfo *svi = ShipVehInfo(engine_number);
	Engine *e;

	SetDParam(0, svi->base_cost * (_price.ship_base>>3)>>5);
	SetDParam(1, svi->max_speed * 10 >> 5);
	SetDParam(2, _cargoc.names_long_p[svi->cargo_type]);
	SetDParam(3, svi->capacity);
	SetDParam(4, svi->refittable ? STR_9842_REFITTABLE : STR_EMPTY);
	SetDParam(5, svi->running_cost * _price.ship_running >> 8);

	e = &_engines[engine_number];
	SetDParam(7, e->lifelength);
	SetDParam(8, e->reliability * 100 >> 16);
	ConvertDayToYMD(&ymd, e->intro_date);
	SetDParam(6, ymd.year + 1920);
}

static void DrawShipImage(Vehicle *v, int x, int y, VehicleID selection);


const byte _ship_refit_types[4][16] = {
	{CT_MAIL, CT_COAL, CT_LIVESTOCK, CT_GOODS, CT_GRAIN, CT_WOOD, CT_IRON_ORE, CT_STEEL, CT_VALUABLES, 255},
	{CT_MAIL, CT_COAL, CT_LIVESTOCK, CT_GOODS, CT_GRAIN, CT_WOOD, CT_PAPER, CT_FOOD, CT_VALUABLES, 255},
	{CT_MAIL, CT_FRUIT, CT_GOODS, CT_COPPER_ORE, CT_GRAIN, CT_WOOD, CT_WATER, CT_VALUABLES, 255},
	{CT_MAIL, CT_SUGAR, CT_TOYS, CT_CANDY, CT_COLA, CT_COTTON_CANDY, CT_BUBBLES, CT_TOFFEE, CT_BATTERIES, CT_PLASTIC, CT_FIZZY_DRINKS, 255},
};

static void ShipRefitWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		Vehicle *v = &_vehicles[w->window_number];
		const byte *b;
		int sel;
		int x,y;
		byte color;
		int cargo;

		SetDParam(0, v->string_id);
		SetDParam(1, v->unitnumber);
		DrawWindowWidgets(w);

		DrawString(1, 15, STR_983F_SELECT_CARGO_TYPE_TO_CARRY, 0);

		cargo = -1;
		x = 6;
		y = 25;
		sel = WP(w,refit_d).sel;

#define show_cargo(ctype) { \
		color = 16; \
		if (sel == 0) { \
			cargo = ctype; \
			color = 12; \
		} \
		sel--; \
		DrawString(x, y, _cargoc.names_s[ctype], color); \
		y += 10; \
		}

		if (_engine_refit_masks[v->engine_type]) {
			uint32 mask = _engine_refit_masks[v->engine_type];
			int cid = 0;

			for (; mask; mask >>= 1, cid++) {
				if (!(mask & 1)) // not this cid
					continue;
				if (!(_local_cargo_id_landscape[cid] & (1 << _opt.landscape))) // not in this landscape
					continue;

				show_cargo(_local_cargo_id_ctype[cid]);
			}

		} else { // generic refit list
			b = _ship_refit_types[_opt.landscape];
			do {
				show_cargo(*b);
			} while (*++b != 255);
		}

#undef show_cargo

		WP(w,refit_d).cargo = cargo;

		if (cargo != -1) {
			int32 cost = DoCommandByTile(v->tile, v->index, cargo, 0, CMD_REFIT_SHIP);
			if (cost != CMD_ERROR) {
				SetDParam(2, cost);
				SetDParam(0, _cargoc.names_long_p[cargo]);
				SetDParam(1, v->cargo_cap);
				DrawString(1, 137, STR_9840_NEW_CAPACITY_COST_OF_REFIT, 0);
			}
		}

		break;
	}

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
			if (WP(w,refit_d).cargo != 0xFF) {
				Vehicle *v = &_vehicles[w->window_number];
				if (DoCommandP(v->tile, v->index, WP(w,refit_d).cargo, NULL, CMD_REFIT_SHIP | CMD_MSG(STR_9841_CAN_T_REFIT_SHIP)))
					DeleteWindow(w);
			}
		  break;
		}
		break;
	}
}


static const Widget _ship_refit_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,						STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   239,     0,    13, STR_983B_REFIT,			STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   239,    14,   135, 0x0,									STR_983D_SELECT_TYPE_OF_CARGO_FOR},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   239,   136,   157, 0x0,									STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,   239,   158,   169, STR_983C_REFIT_SHIP,	STR_983E_REFIT_SHIP_TO_CARRY_HIGHLIGHTED},
{   WIDGETS_END},
};

static const WindowDesc _ship_refit_desc = {
	-1,-1, 240, 170,
	WC_VEHICLE_REFIT,WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_ship_refit_widgets,
	ShipRefitWndProc,
};

static void ShowShipRefitWindow(Vehicle *v)
{
	Window *w;

	DeleteWindowById(WC_VEHICLE_REFIT, v->index);

	_alloc_wnd_parent_num = v->index;
	w = AllocateWindowDesc(&_ship_refit_desc);
	w->window_number = v->index;
	w->caption_color = v->owner;
	WP(w,refit_d).sel = -1;
}

static void ShipDetailsWndProc(Window *w, WindowEvent *e)
{
	Vehicle *v = &_vehicles[w->window_number];
	StringID str;
	int mod;

	switch(e->event) {
	case WE_PAINT:
		w->disabled_state = v->owner == _local_player ? 0 : (1 << 2);
		if (!_patches.servint_ships) // disable service-scroller when interval is set to disabled
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
			SetDParam(3, ShipVehInfo(v->engine_type)->running_cost * _price.ship_running >> 8);
			DrawString(2, 15, STR_9812_AGE_RUNNING_COST_YR, 0);
		}

		/* Draw max speed */
		{
			SetDParam(0, v->max_speed * 10 >> 5);
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
			DrawString(13, 90, _patches.servint_ispercent?STR_SERVICING_INTERVAL_PERCENT:STR_883C_SERVICING_INTERVAL_DAYS, 0);
		}

		DrawShipImage(v, 3, 57, INVALID_VEHICLE);

		SetDParam(1, 1920 + v->build_year);
		SetDParam(0, GetCustomEngineName(v->engine_type));
		SetDParam(2, v->value);
		DrawString(74, 57, STR_9816_BUILT_VALUE, 0);

		SetDParam(0, _cargoc.names_long_p[v->cargo_type]);
		SetDParam(1, v->cargo_cap);
		DrawString(74, 67, STR_9817_CAPACITY, 0);

		str = STR_8812_EMPTY;
		if (v->cargo_count != 0) {
			SetDParam(0, v->cargo_type);
			SetDParam(1, v->cargo_count);
			SetDParam(2, v->cargo_source);
			str = STR_8813_FROM;
		}
		DrawString(74, 78, str, 0);
		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 2: /* rename */
			SetDParam(0, v->unitnumber);
			ShowQueryString(v->string_id, STR_9831_NAME_SHIP, 31, 150, w->window_class, w->window_number);
			break;
		case 5: /* increase int */
			mod = _ctrl_pressed? 5 : 10;
			goto change_int;
		case 6: /* decrease int */
			mod = _ctrl_pressed?- 5 : -10;
change_int:
			mod += v->service_interval;

			/*	%-based service interval max 5%-90%
					day-based service interval max 30-800 days */
			mod = _patches.servint_ispercent ? clamp(mod, MIN_SERVINT_PERCENT, MAX_SERVINT_PERCENT) : clamp(mod, MIN_SERVINT_DAYS, MAX_SERVINT_DAYS+1);
			if (mod == v->service_interval)
				return;

			DoCommandP(v->tile, v->index, mod, NULL, CMD_CHANGE_SHIP_SERVICE_INT | CMD_MSG(STR_018A_CAN_T_CHANGE_SERVICING));
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
		DoCommandP(0, w->window_number, 0, NULL, CMD_NAME_VEHICLE | CMD_MSG(STR_9832_CAN_T_NAME_SHIP));
	} break;

	}
}


static const Widget _ship_details_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,				STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   364,     0,    13, STR_9811_DETAILS,STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   365,   404,     0,    13, STR_01AA_NAME,		STR_982F_NAME_SHIP},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   404,    14,    55, 0x0,							STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   404,    56,    88, 0x0,							STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    10,    89,    94, STR_0188,				STR_884D_INCREASE_SERVICING_INTERVAL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    10,    95,   100, STR_0189,				STR_884E_DECREASE_SERVICING_INTERVAL},
{     WWT_IMGBTN,   RESIZE_NONE,    14,    11,   404,    89,   100, 0x0,							STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _ship_details_desc = {
	-1,-1, 405, 101,
	WC_VEHICLE_DETAILS,WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_ship_details_widgets,
	ShipDetailsWndProc
};

static void ShowShipDetailsWindow(Vehicle *v)
{
	Window *w;
	VehicleID veh = v->index;
	DeleteWindowById(WC_VEHICLE_ORDERS, veh);
	DeleteWindowById(WC_VEHICLE_DETAILS, veh);
	_alloc_wnd_parent_num = veh;
	w = AllocateWindowDesc(&_ship_details_desc);
	w->window_number = veh;
	w->caption_color = v->owner;
}

void CcBuildShip(bool success, uint tile, uint32 p1, uint32 p2)
{
	Vehicle *v;
	if (!success) return;

	v = &_vehicles[_new_ship_id];
	if (v->tile == _backup_orders_tile) {
		_backup_orders_tile = 0;
		RestoreVehicleOrders(v, _backup_orders_data);
	}
	ShowShipViewWindow(v);
}

static void NewShipWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT:
		if (w->window_number == 0)
			w->disabled_state = 1 << 5;

		// Setup scroll count
		{
			int count = 0;
			int num = NUM_SHIP_ENGINES;
			Engine *e = &_engines[SHIP_ENGINES_INDEX];
			do {
				if (HASBIT(e->player_avail, _local_player))
					count++;
			} while (++e,--num);
			SetVScrollCount(w, count);
		}

		DrawWindowWidgets(w);

		{
			int num = NUM_SHIP_ENGINES;
			Engine *e = &_engines[SHIP_ENGINES_INDEX];
			int x = 2;
			int y = 15;
			int sel = WP(w,buildtrain_d).sel_index;
			int pos = w->vscroll.pos;
			int engine_id = SHIP_ENGINES_INDEX;
			int selected_id = -1;

			do {
				if (HASBIT(e->player_avail, _local_player)) {
					if (sel==0) selected_id = engine_id;
					if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
						DrawString(x+75, y+7, GetCustomEngineName(engine_id), sel==0 ? 0xC : 0x10);
						DrawShipEngine(x+35, y+10, engine_id, SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)));
						y += 24;
					}
					sel--;
				}
			} while (++engine_id, ++e,--num);

			WP(w,buildtrain_d).sel_engine = selected_id;

			if (selected_id != -1) {
				Set_DPARAM_Ship_Build_Window(selected_id);

				DrawString(2, w->widget[4].top + 1, STR_980A_COST_SPEED_CAPACITY_RUNNING, 0);
			}
		}
		break;

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
				DoCommandP(w->window_number, sel_eng, 0, CcBuildShip, CMD_BUILD_SHIP | CMD_MSG(STR_980D_CAN_T_BUILD_SHIP));
		} break;

		case 6:	/* rename */
			WP(w,buildtrain_d).rename_engine = WP(w,buildtrain_d).sel_engine;
			ShowQueryString(
				GetCustomEngineName(WP(w,buildtrain_d).sel_engine),
				STR_9838_RENAME_SHIP_TYPE,
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
		DoCommandP(0, WP(w,buildtrain_d).rename_engine, 0, NULL, CMD_RENAME_ENGINE | CMD_MSG(STR_9839_CAN_T_RENAME_SHIP_TYPE));
	} break;

	case WE_RESIZE:
		w->vscroll.cap += e->sizing.diff.y / 24;
		w->widget[2].unkA = (w->vscroll.cap << 8) + 1;
		break;

	}
}

static const Widget _new_ship_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,						STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   254,     0,    13, STR_9808_NEW_SHIPS,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   243,    14,   109, 0x401,								STR_9825_SHIP_SELECTION_LIST_CLICK},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   244,   254,    14,   109, 0x0,									STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_IMGBTN,     RESIZE_TB,    14,     0,   254,   110,   161, 0x0,									STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   121,   162,   173, STR_9809_BUILD_SHIP,	STR_9826_BUILD_THE_HIGHLIGHTED_SHIP},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   122,   243,   162,   173, STR_9836_RENAME,			STR_9837_RENAME_SHIP_TYPE},
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   244,   254,   162,   173, 0x0,											STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const WindowDesc _new_ship_desc = {
	-1, -1, 255, 174,
	WC_BUILD_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_new_ship_widgets,
	NewShipWndProc
};


static void ShowBuildShipWindow(TileIndex tile)
{
	Window *w;

	DeleteWindowById(WC_BUILD_VEHICLE, tile);

	w = AllocateWindowDesc(&_new_ship_desc);
	w->window_number = tile;
	w->vscroll.cap = 4;
	w->widget[2].unkA = (w->vscroll.cap << 8) + 1;

	w->resize.step_height = 24;

	if (tile != 0) {
		w->caption_color = _map_owner[tile];
	} else {
		w->caption_color = _local_player;
	}

}


static void ShipViewWndProc(Window *w, WindowEvent *e) {
	switch(e->event) {
	case WE_PAINT: {
		Vehicle *v = &_vehicles[w->window_number];
		uint32 disabled = 1<<8;
		StringID str;

		// Possible to refit?
		if (ShipVehInfo(v->engine_type)->refittable &&
				v->vehstatus&VS_STOPPED &&
				v->u.ship.state == 0x80 &&
				IsShipDepotTile(v->tile))
			disabled = 0;

		if (v->owner != _local_player)
			disabled |= 1<<8 | 1<<7;
		w->disabled_state = disabled;

		/* draw widgets & caption */
		SetDParam(0, v->string_id);
		SetDParam(1, v->unitnumber);
		DrawWindowWidgets(w);

		/* draw the flag */
		DrawSprite((v->vehstatus & VS_STOPPED) ? 0xC12  : 0xC13, 2, 105);

		if (v->breakdown_ctr == 1) {
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
				str = STR_HEADING_FOR_SHIP_DEPOT + _patches.vehicle_speed;
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
			DoCommandP(v->tile, v->index, 0, NULL, CMD_START_STOP_SHIP | CMD_MSG(STR_9818_CAN_T_STOP_START_SHIP));
			break;
		case 6: /* center main view */
			ScrollMainWindowTo(v->x_pos, v->y_pos);
			break;
		case 7: /* goto hangar */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_SEND_SHIP_TO_DEPOT | CMD_MSG(STR_9819_CAN_T_SEND_SHIP_TO_DEPOT));
			break;
		case 8: /* refit */
			ShowShipRefitWindow(v);
			break;
		case 9: /* show orders */
			ShowOrdersWindow(v);
			break;
		case 10: /* show details */
			ShowShipDetailsWindow(v);
			break;
		}
	} break;

	case WE_DESTROY:
		DeleteWindowById(WC_VEHICLE_ORDERS, w->window_number);
		DeleteWindowById(WC_VEHICLE_REFIT, w->window_number);
		DeleteWindowById(WC_VEHICLE_DETAILS, w->window_number);
		break;
	}
}

static const Widget _ship_view_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,	STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   237,     0,    13, STR_980F,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   238,   249,     0,    13, 0x0,       STR_STICKY_BUTTON},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   231,    14,   103, 0x0,				STR_NULL},
{          WWT_6,   RESIZE_NONE,    14,     2,   229,    16,   101, 0x0,				STR_NULL},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,     0,   249,   104,   115, 0x0,				STR_9827_CURRENT_SHIP_ACTION_CLICK},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,   232,   249,    14,    31, 0x2AB,			STR_9829_CENTER_MAIN_VIEW_ON_SHIP},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,   232,   249,    32,    49, 0x2B0,			STR_982A_SEND_SHIP_TO_DEPOT},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,   232,   249,    50,    67, 0x2B4,			STR_983A_REFIT_CARGO_SHIP_TO_CARRY},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,   232,   249,    68,    85, 0x2B2,			STR_9828_SHOW_SHIP_S_ORDERS},
{ WWT_PUSHIMGBTN,   RESIZE_NONE,    14,   232,   249,    86,   103, 0x2B3,			STR_982B_SHOW_SHIP_DETAILS},
{   WIDGETS_END},
};

static const WindowDesc _ship_view_desc = {
	-1,-1, 250, 116,
	WC_VEHICLE_VIEW,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_ship_view_widgets,
	ShipViewWndProc
};

void ShowShipViewWindow(Vehicle *v)
{
	Window *w;

	w = AllocateWindowDescFront(&_ship_view_desc, v->index);
	if (w) {
		w->caption_color = v->owner;
		AssignWindowViewport(w, 3, 17, 0xE2, 0x54, w->window_number | (1 << 31), 0);
	}
}


static void DrawShipImage(Vehicle *v, int x, int y, VehicleID selection)
{
	int image = GetShipImage(v, 6);
	uint32 ormod = SPRITE_PALETTE(PLAYER_SPRITE_COLOR(v->owner));
	DrawSprite(image | ormod, x+32, y+10);

	if (v->index == selection) {
		DrawFrameRect(x-5, y-1, x+67, y+21, 15, 0x10);
	}
}

static void DrawShipDepotWindow(Window *w)
{
	uint tile;
	Vehicle *v;
	int num,x,y;
	Depot *d;

	tile = w->window_number;

	/* setup disabled buttons */
	w->disabled_state = (_map_owner[tile]==_local_player) ? 0 : ((1<<4)|(1<<7));

	/* determine amount of items for scroller */
	num = 0;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Ship && v->u.ship.state == 0x80 &&
				v->tile == (TileIndex)tile)
					num++;
	}
	SetVScrollCount(w, (num + w->hscroll.cap - 1) / w->hscroll.cap);

	/* locate the depot struct */
	for(d=_depots; d->xy != (TileIndex)tile; d++) {}

	SetDParam(0, d->town_index);
	DrawWindowWidgets(w);

	x = 2;
	y = 15;
	num = w->vscroll.pos * w->hscroll.cap;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Ship &&
				v->u.ship.state == 0x80 &&
				v->tile == (TileIndex)tile &&
				--num < 0 && num >= -w->vscroll.cap * w->hscroll.cap) {

			DrawShipImage(v, x+19, y, WP(w,traindepot_d).sel);

			SetDParam(0, v->unitnumber);
			DrawString(x, y+2, (uint16)(v->max_age-366) >= v->age ? STR_00E2 : STR_00E3, 0);

			DrawSprite( (v->vehstatus & VS_STOPPED) ? 0xC12 : 0xC13, x, y + 9);

			if ((x+=90) == 2 + 90 * w->hscroll.cap) {
				x = 2;
				y += 24;
			}
		}
	}
}

static int GetVehicleFromShipDepotWndPt(Window *w, int x, int y, Vehicle **veh)
{
	uint xt,yt,xm,ym;
	TileIndex tile;
	Vehicle *v;
	int pos;

	xt = x / 90;
	xm = x % 90;
	if (xt >= 5)
		return 1;

	yt = (y - 14) / 24;
	ym = (y - 14) % 24;
	if (yt >= 2)
		return 1;

	pos = (yt + w->vscroll.pos) * 3 + xt;

	tile = w->window_number;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Ship &&
				v->vehstatus&VS_HIDDEN &&
				v->tile == (TileIndex)tile &&
				--pos < 0) {
					*veh = v;
					if (xm >= 19)
						return 0;
					if (ym <= 10)
						return -1; /* show window */
					return -2; /* start stop */
				}
	}

	return 1; /* outside */

}

static void ShipDepotClick(Window *w, int x, int y)
{
	Vehicle *v;
	int mode = GetVehicleFromShipDepotWndPt(w, x, y, &v);

	// share / copy orders
	if (_thd.place_mode && mode <= 0) { _place_clicked_vehicle = v; return; }

	switch (mode) {
	case 1: // invalid
		return;

	case 0: // start dragging of vehicle
		if (v != NULL) {
			WP(w,traindepot_d).sel = v->index;
			SetWindowDirty(w);
			SetObjectToPlaceWnd( SPRITE_PALETTE(PLAYER_SPRITE_COLOR(v->owner)) +
				GetShipImage(v, 6), 4, w);
		}
		break;

	case -1: // show info window
		ShowShipViewWindow(v);
		break;

	case -2: // click start/stop flag
		DoCommandP(v->tile, v->index, 0, NULL, CMD_START_STOP_SHIP | CMD_MSG(STR_9818_CAN_T_STOP_START_SHIP));
		break;

	default:
		NOT_REACHED();
	}
}

static void ShipDepotWndProc(Window *w, WindowEvent *e) {
	switch(e->event) {
	case WE_PAINT:
		DrawShipDepotWindow(w);
		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 3:
			ShipDepotClick(w, e->click.pt.x, e->click.pt.y);
			break;

		case 7:
			ShowBuildShipWindow(w->window_number);
			break;

		case 8: /* scroll to tile */
			ScrollMainWindowToTile(w->window_number);
			break;
		}
		break;

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

			if (GetVehicleFromShipDepotWndPt(w, e->dragdrop.pt.x, e->dragdrop.pt.y, &v) == 0 &&
					v != NULL &&
					sel == v->index) {
				ShowShipViewWindow(v);
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

				if (!DoCommandP(v->tile, v->index, 0, NULL, CMD_SELL_SHIP | CMD_MSG(STR_980C_CAN_T_SELL_SHIP)))
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
		w->hscroll.cap += e->sizing.diff.x / 90;
		w->widget[3].unkA = (w->vscroll.cap << 8) + w->hscroll.cap;
		break;
	}
}

static const Widget _ship_depot_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,								STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   292,     0,    13, STR_9803_SHIP_DEPOT,		STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   293,   304,     0,    13, 0x0,										STR_STICKY_BUTTON},
{     WWT_MATRIX,     RESIZE_RB,    14,     0,   269,    14,    61, 0x203,									STR_981F_SHIPS_CLICK_ON_SHIP_FOR},
{      WWT_PANEL,    RESIZE_LRB,    14,   270,   293,    14,    13, 0x0,													STR_NULL},
{     WWT_IMGBTN,   RESIZE_LRTB,    14,   270,   293,    14,    61, 0x2A9,									STR_9821_DRAG_SHIP_TO_HERE_TO_SELL},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   294,   304,    14,    61, 0x0,										STR_0190_SCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   146,    62,    73, STR_9804_NEW_SHIPS,			STR_9820_BUILD_NEW_SHIP},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   147,   293,    62,    73, STR_00E4_LOCATION,			STR_9822_CENTER_MAIN_VIEW_ON_SHIP},
{      WWT_PANEL,    RESIZE_RTB,    14,   294,   293,    62,    73, 0x0,													STR_NULL},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   294,   304,    62,    73, 0x0,										STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const WindowDesc _ship_depot_desc = {
	-1, -1, 305, 74,
	WC_VEHICLE_DEPOT,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_ship_depot_widgets,
	ShipDepotWndProc
};

void ShowShipDepotWindow(uint tile)
{
	Window *w;

	w = AllocateWindowDescFront(&_ship_depot_desc,tile);
	if (w) {
		w->caption_color = _map_owner[w->window_number];
		w->vscroll.cap = 2;
		w->hscroll.cap = 3;
		w->resize.step_width = 90;
		w->resize.step_height = 24;
		WP(w,traindepot_d).sel = INVALID_VEHICLE;
		_backup_orders_tile = 0;
	}
}


static void DrawSmallShipSchedule(Vehicle *v, int x, int y) {
	Order *sched;
	int sel;
	Station *st;
	int i = 0;

	sel = v->cur_order_index;

	for (sched = v->schedule_ptr; sched->type != OT_NOTHING; ++sched) {
		if (sel == 0) {
			_stringwidth_base = 0xE0;
			DoDrawString( "\xAF", x-6, y, 16);
			_stringwidth_base = 0;
		}
		sel--;

		if (sched->type == OT_GOTO_STATION) {
			st = DEREF_STATION(sched->station);

			if (!(st->had_vehicle_of_type & HVOT_BUOY)) {
				SetDParam(0, sched->station);
				DrawString(x, y, STR_A036, 0);

				y += 6;
				if (++i == 4)
					break;
			}
		}
	}
}


static const Widget _player_ships_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,							STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   247,     0,    13, STR_9805_SHIPS,				STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   248,   259,     0,    13, 0x0,                   STR_STICKY_BUTTON},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    80,    14,    25, SRT_SORT_BY,						STR_SORT_ORDER_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,    81,   232,    14,    25, 0x0,										STR_SORT_CRITERIA_TIP},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   233,   243,    14,    25, STR_0225,							STR_SORT_CRITERIA_TIP},
{      WWT_PANEL,  RESIZE_RIGHT,    14,   244,   259,    14,    25, 0x0,										STR_NULL},
{     WWT_MATRIX,     RESIZE_RB,    14,     0,   248,    26,   169, 0x401,									STR_9823_SHIPS_CLICK_ON_SHIP_FOR},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   249,   259,    26,   169, 0x0,										STR_0190_SCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   124,   170,   181, STR_9804_NEW_SHIPS,		STR_9824_BUILD_NEW_SHIPS_REQUIRES},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   125,   248,   170,   181, STR_REPLACE_VEHICLES,					STR_REPLACE_HELP},
{      WWT_PANEL,    RESIZE_RTB,    14,   249,   248,   170,   181, 0x0,											STR_NULL},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   249,   259,   170,   181, 0x0,											STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const Widget _other_player_ships_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,							STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   247,     0,    13, STR_9805_SHIPS,				STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   248,   259,     0,    13, 0x0,                   STR_STICKY_BUTTON},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    80,    14,    25, SRT_SORT_BY,						STR_SORT_ORDER_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,    81,   232,    14,    25, 0x0,										STR_SORT_CRITERIA_TIP},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   233,   243,    14,    25, STR_0225,							STR_SORT_CRITERIA_TIP},
{      WWT_PANEL,  RESIZE_RIGHT,    14,   244,   259,    14,    25, 0x0,										STR_NULL},
{     WWT_MATRIX,     RESIZE_RB,    14,     0,   248,    26,   169, 0x401,									STR_9823_SHIPS_CLICK_ON_SHIP_FOR},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   249,   259,    26,   169, 0x0,										STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,    RESIZE_RTB,    14,   249,   248,   170,   181, 0x0,											STR_NULL},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   249,   259,   170,   181, 0x0,											STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static void PlayerShipsWndProc(Window *w, WindowEvent *e)
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

		BuildVehicleList(vl, VEH_Ship, owner, station);
		SortVehicleList(vl);

		SetVScrollCount(w, vl->list_length);

		// disable 'Sort By' tooltip on Unsorted sorting criteria
		if (vl->sort_type == SORT_BY_UNSORTED)
			w->disabled_state |= (1 << 3);

		/* draw the widgets */
		{
			const Player *p = DEREF_PLAYER(owner);
			if (station == -1) {
				/* Company Name -- (###) Trains */
				SetDParam(0, p->name_1);
				SetDParam(1, p->name_2);
				SetDParam(2, w->vscroll.count);
				w->widget[1].unkA = STR_9805_SHIPS;
			} else {
				/* Station Name -- (###) Trains */
				SetDParam(0, DEREF_STATION(station)->index);
				SetDParam(1, w->vscroll.count);
				w->widget[1].unkA = STR_SCHEDULED_SHIPS;
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

			assert(v->type == VEH_Ship);

			DrawShipImage(v, x + 19, y + 6, INVALID_VEHICLE);
			DrawVehicleProfitButton(v, x, y + 13);

			SetDParam(0, v->unitnumber);
			if (IsShipDepotTile(v->tile))
				str = STR_021F;
			else
				str = v->age > v->max_age - 366 ? STR_00E3 : STR_00E2;
			DrawString(x, y + 2, str, 0);

			SetDParam(0, v->profit_this_year);
			SetDParam(1, v->profit_last_year);
			DrawString(x + 12, y + 28, STR_0198_PROFIT_THIS_YEAR_LAST_YEAR, 0);

			if (v->string_id != STR_SV_SHIP_NAME) {
				SetDParam(0, v->string_id);
				DrawString(x + 12, y, STR_01AB, 0);
			}

			DrawSmallShipSchedule(v, x + 138, y);

			y += PLY_WND_PRC__SIZE_OF_ROW_BIG;
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
			uint32 id_v = (e->click.pt.y - PLY_WND_PRC__OFFSET_TOP_WIDGET) / PLY_WND_PRC__SIZE_OF_ROW_BIG;

			if (id_v >= w->vscroll.cap) { return;} // click out of bounds

			id_v += w->vscroll.pos;

			{
				Vehicle *v;

				if (id_v >= vl->list_length) return; // click out of list bound

				v	= DEREF_VEHICLE(vl->sort_list[id_v].index);

				assert(v->type == VEH_Ship);

				ShowShipViewWindow(v);
			}
		} break;

		case 9: { /* Build new Vehicle */
			uint tile;

			if (!IsWindowOfPrototype(w, _player_ships_widgets))
				break;


			tile = _last_built_ship_depot_tile;
			do {
				if (_map_owner[tile] == _local_player && IsShipDepotTile(tile)) {
					ShowShipDepotWindow(tile);
					ShowBuildShipWindow(tile);
					return;
				}

				tile = TILE_MASK(tile + 1);
			} while(tile != _last_built_ship_depot_tile);

			ShowBuildShipWindow(0);
		} break;

		case 10: {
			ShowReplaceVehicleWindow(VEH_Ship);
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
			DEBUG(misc, 1) ("Periodic resort ships list player %d station %d",
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

static const WindowDesc _player_ships_desc = {
	-1, -1, 260, 182,
	WC_SHIPS_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_player_ships_widgets,
	PlayerShipsWndProc
};

static const WindowDesc _other_player_ships_desc = {
	-1, -1, 260, 182,
	WC_SHIPS_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_other_player_ships_widgets,
	PlayerShipsWndProc
};


void ShowPlayerShips(int player, int station)
{
	Window *w;

	if ( player == _local_player) {
		w = AllocateWindowDescFront(&_player_ships_desc, (station << 16) | player);
	} else  {
		w = AllocateWindowDescFront(&_other_player_ships_desc, (station << 16) | player);
	}
	if (w) {
		w->caption_color = w->window_number;
		w->vscroll.cap = 4;
		w->widget[7].unkA = (w->vscroll.cap << 8) + 1;
		w->resize.step_height = PLY_WND_PRC__SIZE_OF_ROW_BIG;
	}
}
