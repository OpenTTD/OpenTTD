#include "stdafx.h"
#include "ttd.h"

#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "vehicle.h"
#include "viewport.h"
#include "station.h"
#include "command.h"
#include "player.h"
//#include "town.h"
#include "engine.h"


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

		SET_DPARAM16(0, v->string_id);
		SET_DPARAM16(1, v->unitnumber);
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
				SET_DPARAM32(2, cost);
				SET_DPARAM16(0, _cargoc.names_long_p[cargo]);
				SET_DPARAM16(1, v->cargo_cap);
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
{    WWT_TEXTBTN,    14,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   239,     0,    13, STR_983B_REFIT, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,    14,     0,   239,    14,   135, 0x0, STR_983D_SELECT_TYPE_OF_CARGO_FOR},
{     WWT_IMGBTN,    14,     0,   239,   136,   157, 0x0},
{ WWT_PUSHTXTBTN,    14,     0,   239,   158,   169, STR_983C_REFIT_SHIP, STR_983E_REFIT_SHIP_TO_CARRY_HIGHLIGHTED},
{      WWT_LAST},
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
		SET_DPARAM16(0, v->string_id);
		SET_DPARAM16(1, v->unitnumber);
		DrawWindowWidgets(w);

		/* Draw running cost */
		{
			int year = v->age / 366;
			StringID str;

			SET_DPARAM16(1, year);
			
			str = STR_0199_YEAR;
			if (year != 1) {
				str++;
				if (v->max_age - 366 < v->age)
					str++;
			}
			SET_DPARAM16(0, str);
			SET_DPARAM16(2, v->max_age / 366);
			SET_DPARAM32(3, ship_vehicle_info(v->engine_type).running_cost * _price.ship_running >> 8);
			DrawString(2, 15, STR_9812_AGE_RUNNING_COST_YR, 0);
		}

		/* Draw max speed */
		{
			SET_DPARAM16(0, v->max_speed * 10 >> 5);
			DrawString(2, 25, STR_9813_MAX_SPEED, 0);
		}

		/* Draw profit */
		{
			SET_DPARAM32(0, v->profit_this_year);
			SET_DPARAM32(1, v->profit_last_year);
			DrawString(2, 35, STR_9814_PROFIT_THIS_YEAR_LAST_YEAR, 0);
		}

		/* Draw breakdown & reliability */
		{
			SET_DPARAM8(0, v->reliability * 100 >> 16);
			SET_DPARAM16(1, v->breakdowns_since_last_service);
			DrawString(2, 45, STR_9815_RELIABILITY_BREAKDOWNS, 0);
		}

		/* Draw service interval text */
		{
			SET_DPARAM16(0, v->service_interval);
			SET_DPARAM16(1, v->date_of_last_service);
			DrawString(13, 90, STR_883C_SERVICING_INTERVAL_DAYS, 0);
		}

		DrawShipImage(v, 3, 57, INVALID_VEHICLE);

		SET_DPARAM16(1, 1920 + v->build_year);
		SET_DPARAM16(0, GetCustomEngineName(v->engine_type));
		SET_DPARAM32(2, v->value);
		DrawString(74, 57, STR_9816_BUILT_VALUE, 0);
		
		SET_DPARAM16(0, _cargoc.names_long_p[v->cargo_type]);
		SET_DPARAM16(1, v->cargo_cap);
		DrawString(74, 67, STR_9817_CAPACITY, 0);

		str = STR_8812_EMPTY;
		if (v->cargo_count != 0) {
			SET_DPARAM8(0, v->cargo_type);
			SET_DPARAM16(1, v->cargo_count);
			SET_DPARAM16(2, v->cargo_source);
			str = STR_8813_FROM;
		}
		DrawString(74, 78, str, 0);
		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 2: /* rename */
			SET_DPARAM16(0, v->unitnumber);
			ShowQueryString(v->string_id, STR_9831_NAME_SHIP, 31, 150, w->window_class, w->window_number);
			break;
		case 5: /* increase int */
			mod = 10;
			goto change_int;
		case 6: /* decrease int */
			mod = -10;
change_int:
			mod += v->service_interval;
			if (!IS_INT_INSIDE(mod, 30, 800+1))
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
{    WWT_TEXTBTN,    14,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   364,     0,    13, STR_9811_DETAILS,STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,    14,   365,   404,     0,    13, STR_01AA_NAME, STR_982F_NAME_SHIP},
{     WWT_IMGBTN,    14,     0,   404,    14,    55, 0x0},
{     WWT_IMGBTN,    14,     0,   404,    56,    88, 0x0},
{ WWT_PUSHTXTBTN,    14,     0,    10,    89,    94, STR_0188,STR_884D_INCREASE_SERVICING_INTERVAL},
{ WWT_PUSHTXTBTN,    14,     0,    10,    95,   100, STR_0189,STR_884E_DECREASE_SERVICING_INTERVAL},
{     WWT_IMGBTN,    14,    11,   404,    89,   100, 0x0},
{      WWT_LAST},
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

static void CcBuildShip(bool success, uint tile, uint32 p1, uint32 p2)
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
	YearMonthDay ymd;
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
					if (IS_INT_INSIDE(--pos, -4, 0)) {
						DrawString(x+75, y+7, GetCustomEngineName(engine_id), sel==0 ? 0xC : 0x10);
						DrawShipEngine(x+35, y+10, engine_id, SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)));
						y += 24;
					}
					sel--; 
				}
			} while (++engine_id, ++e,--num);

			WP(w,buildtrain_d).sel_engine = selected_id;

			if (selected_id != -1) {
				Engine *e;

				SET_DPARAM32(0, ship_vehicle_info(selected_id).base_cost * (_price.ship_base>>3)>>5);
				SET_DPARAM16(1, ship_vehicle_info(selected_id).max_speed * 10 >> 5);
				SET_DPARAM16(2, _cargoc.names_long_p[ship_vehicle_info(selected_id).cargo_type]);
				SET_DPARAM16(3, ship_vehicle_info(selected_id).capacity);
				SET_DPARAM16(4, ship_vehicle_info(selected_id).refittable ? STR_9842_REFITTABLE : STR_EMPTY);
				SET_DPARAM32(5, ship_vehicle_info(selected_id).running_cost * _price.ship_running >> 8);

				e = &_engines[selected_id];	
				SET_DPARAM16(7, e->lifelength);
				SET_DPARAM8(8, e->reliability * 100 >> 16);
				ConvertDayToYMD(&ymd, e->intro_date);
				SET_DPARAM16(6, ymd.year + 1920);
				
				DrawString(2, 111, STR_980A_COST_SPEED_CAPACITY_RUNNING, 0);
			}
		}
		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 2: { /* listbox */
			uint i = (e->click.pt.y - 14) / 24;
			if (i < 4) {
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

	}
}

static const Widget _new_ship_widgets[] = {
{    WWT_TEXTBTN,    14,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   254,     0,    13, STR_9808_NEW_SHIPS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX,    14,     0,   243,    14,   109, 0x401, STR_9825_SHIP_SELECTION_LIST_CLICK},
{  WWT_SCROLLBAR,    14,   244,   254,    14,   109, 0x0, STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_IMGBTN,    14,     0,   254,   110,   161, 0x0},
{ WWT_PUSHTXTBTN,    14,     0,   126,   162,   173, STR_9809_BUILD_SHIP, STR_9826_BUILD_THE_HIGHLIGHTED_SHIP},
{ WWT_PUSHTXTBTN,    14,   127,   254,   162,   173, STR_9836_RENAME, STR_9837_RENAME_SHIP_TYPE},
{      WWT_LAST},
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
		uint32 disabled = 1<<7;
		StringID str;

		// Possible to refit?
		if (ship_vehicle_info(v->engine_type).refittable &&
				v->vehstatus&VS_STOPPED &&
				v->u.ship.state == 0x80 &&
				IsShipDepotTile(v->tile))
			disabled = 0;
					
		if (v->owner != _local_player)
			disabled |= 1<<7 | 1<<6;
		w->disabled_state = disabled;

		/* draw widgets & caption */
		SET_DPARAM16(0, v->string_id);
		SET_DPARAM16(1, v->unitnumber);
		DrawWindowWidgets(w);

		/* draw the flag */
		DrawSprite((v->vehstatus & VS_STOPPED) ? 0xC12  : 0xC13, 2, 105);

		if (v->breakdown_ctr == 1) {
			str = STR_885C_BROKEN_DOWN;
		} else if (v->vehstatus & VS_STOPPED) {
			str = STR_8861_STOPPED;
		} else {
			switch(v->next_order & OT_MASK) {
			case OT_GOTO_STATION: {
				SET_DPARAM16(0, v->next_order_param);
				SET_DPARAM16(1, v->cur_speed * 10 >> 5);
				str = STR_HEADING_FOR_STATION + _patches.vehicle_speed;
			} break;

			case OT_GOTO_DEPOT: {
				Depot *dep = &_depots[v->next_order_param];
				SET_DPARAM16(0, dep->town_index);
				SET_DPARAM16(1, v->cur_speed * 10 >> 5);
				str = STR_HEADING_FOR_SHIP_DEPOT + _patches.vehicle_speed;
			} break;

			case OT_LOADING:
			case OT_LEAVESTATION:
				str = STR_882F_LOADING_UNLOADING;
				break;

			default:
				if (v->num_orders == 0) {
					str = STR_NO_ORDERS + _patches.vehicle_speed;
					SET_DPARAM16(0, v->cur_speed * 10 >> 5);
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
		case 4: /* start stop */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_START_STOP_SHIP | CMD_MSG(STR_9818_CAN_T_STOP_START_SHIP)); 
			break;
		case 5: /* center main view */
			ScrollMainWindowTo(v->x_pos, v->y_pos);
			break;
		case 6: /* goto hangar */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_SEND_SHIP_TO_DEPOT | CMD_MSG(STR_9819_CAN_T_SEND_SHIP_TO_DEPOT)); 
			break;
		case 7: /* refit */
			ShowShipRefitWindow(v);
			break;
		case 8: /* show orders */
			ShowOrdersWindow(v);
			break;
		case 9: /* show details */
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
{    WWT_TEXTBTN,    14,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   249,     0,    13, STR_980F, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,    14,     0,   231,    14,   103, 0x0},
{          WWT_6,    14,     2,   229,    16,   101, 0},
{ WWT_PUSHIMGBTN,    14,     0,   249,   104,   115, 0x0, STR_9827_CURRENT_SHIP_ACTION_CLICK},
{ WWT_PUSHIMGBTN,    14,   232,   249,    14,    31, 0x2AB, STR_9829_CENTER_MAIN_VIEW_ON_SHIP},
{ WWT_PUSHIMGBTN,    14,   232,   249,    32,    49, 0x2B0, STR_982A_SEND_SHIP_TO_DEPOT},
{ WWT_PUSHIMGBTN,    14,   232,   249,    50,    67, 0x2B4, STR_983A_REFIT_CARGO_SHIP_TO_CARRY},
{ WWT_PUSHIMGBTN,    14,   232,   249,    68,    85, 0x2B2, STR_9828_SHOW_SHIP_S_ORDERS},
{ WWT_PUSHIMGBTN,    14,   232,   249,    86,   103, 0x2B3, STR_982B_SHOW_SHIP_DETAILS},
{      WWT_LAST},
};

static const WindowDesc _ship_view_desc = {
	-1,-1, 250, 116,
	WC_VEHICLE_VIEW,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
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
	w->disabled_state = (_map_owner[tile]==_local_player) ? 0 : ((1<<3)|(1<<5));

	/* determine amount of items for scroller */
	num = 0;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Ship && v->u.ship.state == 0x80 &&
				v->tile == (TileIndex)tile)
					num++;
	}
	SetVScrollCount(w, (num+2) / 3);

	/* locate the depot struct */
	for(d=_depots; d->xy != (TileIndex)tile; d++) {}

	SET_DPARAM16(0, d->town_index);
	DrawWindowWidgets(w);

	x = 2;
	y = 15;
	num = w->vscroll.pos * 3;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Ship &&
				v->u.ship.state == 0x80 && 
				v->tile == (TileIndex)tile &&
				--num < 0 && num >=	-6) {

			DrawShipImage(v, x+19, y, WP(w,traindepot_d).sel);
			
			SET_DPARAM16(0, v->unitnumber);
			DrawString(x, y, (uint16)(v->max_age-366) >= v->age ? STR_00E2 : STR_00E3, 0);

			DrawSprite( (v->vehstatus & VS_STOPPED) ? 0xC12 : 0xC13, x, y + 9);
			
			if ((x+=90) == 2+90*3) {
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
		case 2:
			ShipDepotClick(w, e->click.pt.x, e->click.pt.y);
			break;
			
		case 5:
			ShowBuildShipWindow(w->window_number);
			break;

		case 6: /* scroll to tile */
			ScrollMainWindowToTile(w->window_number);
			break;
		}
		break;

	case WE_DESTROY:
		DeleteWindowById(WC_BUILD_VEHICLE, w->window_number);
		break;

	case WE_DRAGDROP: {
		switch(e->click.widget) {
		case 2: {
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
					
		case 3:
			if (!HASBIT(w->disabled_state, 3) &&
					WP(w,traindepot_d).sel != INVALID_VEHICLE)	{
				Vehicle *v;
				
				HandleButtonClick(w, 3);

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
	}
}

static const Widget _ship_depot_widgets[] = {
{    WWT_TEXTBTN,    14,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   304,     0,    13, STR_9803_SHIP_DEPOT, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX,    14,     0,   269,    14,    61, 0x203, STR_981F_SHIPS_CLICK_ON_SHIP_FOR},
{     WWT_IMGBTN,    14,   270,   293,    14,    61, 0x2A9, STR_9821_DRAG_SHIP_TO_HERE_TO_SELL},
{  WWT_SCROLLBAR,    14,   294,   304,    14,    61, 0x0, STR_0190_SCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,    14,     0,   151,    62,    73, STR_9804_NEW_SHIPS, STR_9820_BUILD_NEW_SHIP},
{ WWT_PUSHTXTBTN,    14,   152,   304,    62,    73, STR_00E4_LOCATION, STR_9822_CENTER_MAIN_VIEW_ON_SHIP},
{      WWT_LAST},
};

static const WindowDesc _ship_depot_desc = {
	-1, -1, 305, 74,
	WC_VEHICLE_DEPOT,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
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
		WP(w,traindepot_d).sel = INVALID_VEHICLE;
		_backup_orders_tile = 0;
	}
}


static void DrawSmallShipSchedule(Vehicle *v, int x, int y) {
	uint16 *sched;
	int sel;
	uint ord;
	Station *st;
	int i = 0;

	sched = v->schedule_ptr;
	sel = v->cur_order_index;

	while ((ord=*sched++) != 0) {
		if (sel == 0) {
			_stringwidth_base = 0xE0;
			DoDrawString( "\xAF", x-6, y, 16);
			_stringwidth_base = 0;
		}
		sel--;

		if ((ord & OT_MASK) == OT_GOTO_STATION) {
			st = DEREF_STATION(ord >> 8);
			
			if (!(st->had_vehicle_of_type & HVOT_BUOY)) {
				SET_DPARAM16(0, ord >> 8);
				DrawString(x, y, STR_A036, 0);

				y += 6;
				if (++i == 4)
					break;
			}
		}
	}
}

static void PlayerShipsWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT:
		/* determine amount of items for scroller */
		{
			Vehicle *v;
			int num = 0;
			byte owner = (byte)w->window_number;

			FOR_ALL_VEHICLES(v) {
				if (v->type == VEH_Ship && v->owner == owner)
					num++;
			}
			SetVScrollCount(w, num);
		}

		/* draw the widgets */
		{
			Player *p = DEREF_PLAYER(w->window_number);
			SET_DPARAM16(0, p->name_1);
			SET_DPARAM32(1, p->name_2);
			DrawWindowWidgets(w);
		}

		/* draw the ships vehicles */
		{
			Vehicle *v;
			int pos = w->vscroll.pos;
			byte owner = (byte)w->window_number;
			int x = 2;
			int y = 15;
			
			FOR_ALL_VEHICLES(v) {
				if (v->type == VEH_Ship && v->owner == owner &&
						--pos < 0 && pos >= -4) {
					StringID str;
					
					DrawShipImage(v, x + 19, y + 6, INVALID_VEHICLE);
					DrawVehicleProfitButton(v, x, y+13);
					
					SET_DPARAM16(0, v->unitnumber);
					if (IsShipDepotTile(v->tile)) {
						str = STR_021F;
					} else {
						str = v->age > v->max_age - 366 ? STR_00E3 : STR_00E2;
					}
					DrawString(x, y+2, str, 0);

					SET_DPARAM32(0, v->profit_this_year);
					SET_DPARAM32(1, v->profit_last_year);
					DrawString(x + 12, y + 28, STR_0198_PROFIT_THIS_YEAR_LAST_YEAR, 0);
					
					if (v->string_id != STR_SV_SHIP_NAME) {
						SET_DPARAM16(0, v->string_id);
						DrawString(x+12, y, STR_01AB, 0);
					}

					DrawSmallShipSchedule(v, x+138, y);

					y += 36;
				}				
			}
		}
		break;
	case WE_CLICK:
		switch(e->click.widget) {
		case 2: { /* click ship */
			int sel;
			Vehicle *v;
			byte owner;
	
			sel = (e->click.pt.y - 14) / 36;

			if ((uint)sel >= 4)
				break;
			sel += w->vscroll.pos;
			owner = (byte)w->window_number;
			FOR_ALL_VEHICLES(v) {
				if (v->type == VEH_Ship && v->owner == owner &&	--sel < 0) {	
					ShowShipViewWindow(v);
					break;
				}
			}
			break;
		}
		case 4: {/* click buy */
			uint tile;

			tile = _last_built_ship_depot_tile;
			do {
				if (_map_owner[tile] == _local_player &&
						IsShipDepotTile(tile)) {
					
					ShowShipDepotWindow(tile);
					ShowBuildShipWindow(tile);
					return;
				}
				
				tile = TILE_MASK(tile + 1);
			} while(tile != _last_built_ship_depot_tile);
			
			ShowBuildShipWindow(0);
		} break;
		}
		break;
	}
}

static const Widget _player_ships_widgets[] = {
{    WWT_TEXTBTN,    14,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   259,     0,    13, STR_9805_SHIPS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX,    14,     0,   248,    14,   157, 0x401, STR_9823_SHIPS_CLICK_ON_SHIP_FOR},
{  WWT_SCROLLBAR,    14,   249,   259,    14,   157, 0x0, STR_0190_SCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,    14,     0,   129,   158,   169, STR_9804_NEW_SHIPS, STR_9824_BUILD_NEW_SHIPS_REQUIRES},
{     WWT_IMGBTN,    14,   130,   259,   158,   169, 0x0},
{      WWT_LAST},
};
static const WindowDesc _player_ships_desc = {
	-1, -1, 260, 170,
	WC_SHIPS_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_player_ships_widgets,
	PlayerShipsWndProc
};

static const Widget _other_player_ships_widgets[] = {
{    WWT_TEXTBTN,    14,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   259,     0,    13, STR_9805_SHIPS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX,    14,     0,   248,    14,   157, 0x401, STR_9823_SHIPS_CLICK_ON_SHIP_FOR},
{  WWT_SCROLLBAR,    14,   249,   259,    14,   157, 0x0, STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_LAST},
};
static const WindowDesc _other_player_ships_desc = {
	-1, -1, 260, 158,
	WC_SHIPS_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_other_player_ships_widgets,
	PlayerShipsWndProc
};


void ShowPlayerShips(int player)
{
	Window *w;

	if ( player == _local_player) {
		w = AllocateWindowDescFront(&_player_ships_desc, player);
	} else  {
		w = AllocateWindowDescFront(&_other_player_ships_desc, player);
	}
	if (w) {
		w->caption_color = w->window_number;
		w->vscroll.cap = 4;
	}
}
