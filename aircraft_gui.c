#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "window.h"
#include "gui.h"
#include "vehicle.h"
#include "gfx.h"
#include "station.h"
#include "command.h"
#include "engine.h"
#include "viewport.h"
#include "player.h"


static void DrawAircraftImage(Vehicle *v, int x, int y, VehicleID selection)
{
	int image = GetAircraftImage(v, 6);
	uint32 ormod = SPRITE_PALETTE(PLAYER_SPRITE_COLOR(v->owner));
	if (v->vehstatus & VS_CRASHED) ormod = 0x3248000;
	DrawSprite(image | ormod, x+25, y+10);
	if (v->subtype == 0)
		DrawSprite(0xF3D, x+25, y+5);
	if (v->index == selection) {
		DrawFrameRect(x-1, y-1, x+58, y+21, 0xF, 0x10);
	}
}

void CcBuildAircraft(bool success, uint tile, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (success) {
		v = &_vehicles[_new_aircraft_id];
		if (v->tile == _backup_orders_tile) {
			_backup_orders_tile = 0;
			RestoreVehicleOrders(v, _backup_orders_data);
		}
		ShowAircraftViewWindow(v);
	}
}


static void NewAircraftWndProc(Window *w, WindowEvent *e)
{
	YearMonthDay ymd;

	switch(e->event) {

	case WE_PAINT: {
		if (w->window_number == 0)
			SETBIT(w->disabled_state, 5);

		{
			int count = 0;
			int num = NUM_AIRCRAFT_ENGINES;
			Engine *e = &_engines[AIRCRAFT_ENGINES_INDEX];
			do {
				if (HASBIT(e->player_avail, _local_player))
					count++;
			} while (++e,--num);
			SetVScrollCount(w, count);
		}

		DrawWindowWidgets(w);

		{
			int num = NUM_AIRCRAFT_ENGINES;
			Engine *e = &_engines[AIRCRAFT_ENGINES_INDEX];
			int x = 2;
			int y = 15;
			int sel = WP(w,buildtrain_d).sel_index;
			int pos = w->vscroll.pos;
			int engine_id = AIRCRAFT_ENGINES_INDEX;
			int selected_id = -1;

			do {
				if (HASBIT(e->player_avail, _local_player)) {
					if (sel==0) selected_id = engine_id;
					if (IS_INT_INSIDE(--pos, -4, 0)) {
						DrawString(x+62, y+7, GetCustomEngineName(engine_id), sel==0 ? 0xC : 0x10);
						DrawAircraftEngine(x+29, y+10, engine_id, SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)));
						y += 24;
					}
					sel--;
				}
			} while (++engine_id, ++e,--num);

			WP(w,buildtrain_d).sel_engine = selected_id;

			if (selected_id != -1) {
				const AircraftVehicleInfo *avi = AircraftVehInfo(selected_id);
				Engine *e;

				SetDParam(0, avi->base_cost * (_price.aircraft_base>>3)>>5);
				SetDParam(1, avi->max_speed * 8);
				SetDParam(2, avi->passanger_capacity);
				SetDParam(3, avi->mail_capacity);
				SetDParam(4, avi->running_cost * _price.aircraft_running >> 8);

				e = &_engines[selected_id];
				SetDParam(6, e->lifelength);
				SetDParam(7, e->reliability * 100 >> 16);
				ConvertDayToYMD(&ymd, e->intro_date);
				SetDParam(5, ymd.year + 1920);

				DrawString(2, 111, STR_A007_COST_SPEED_CAPACITY_PASSENGERS, 0);
			}
		}
	} break;

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
				DoCommandP(w->window_number, sel_eng, 0, CcBuildAircraft, CMD_BUILD_AIRCRAFT | CMD_MSG(STR_A008_CAN_T_BUILD_AIRCRAFT));
		} break;

		case 6:	/* rename */
			WP(w,buildtrain_d).rename_engine = WP(w,buildtrain_d).sel_engine;
			ShowQueryString(
				GetCustomEngineName(WP(w,buildtrain_d).sel_engine),
				STR_A039_RENAME_AIRCRAFT_TYPE,
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
		DoCommandP(0, WP(w,buildtrain_d).rename_engine, 0, NULL, CMD_RENAME_ENGINE | CMD_MSG(STR_A03A_CAN_T_RENAME_AIRCRAFT_TYPE));
	} break;
	}
}

static const Widget _new_aircraft_widgets[] = {
{    WWT_TEXTBTN,    14,     0,    10,     0,    13, STR_00C5,								STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   239,     0,    13, STR_A005_NEW_AIRCRAFT,		STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX,    14,     0,   228,    14,   109, 0x401,										STR_A025_AIRCRAFT_SELECTION_LIST},
{  WWT_SCROLLBAR,    14,   229,   239,    14,   109, 0x0,											STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_IMGBTN,    14,     0,   239,   110,   161, 0x0,											STR_NULL},
{ WWT_PUSHTXTBTN,    14,     0,   119,   162,   173, STR_A006_BUILD_AIRCRAFT,	STR_A026_BUILD_THE_HIGHLIGHTED_AIRCRAFT},
{ WWT_PUSHTXTBTN,    14,   120,   239,   162,   173, STR_A037_RENAME,					STR_A038_RENAME_AIRCRAFT_TYPE},
{   WIDGETS_END},
};

static const WindowDesc _new_aircraft_desc = {
	-1, -1, 240, 174,
	WC_BUILD_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_new_aircraft_widgets,
	NewAircraftWndProc
};

static void ShowBuildAircraftWindow(uint tile)
{
	Window *w;

	DeleteWindowById(WC_BUILD_VEHICLE, tile);

	w = AllocateWindowDesc(&_new_aircraft_desc);
	w->window_number = tile;
	w->vscroll.cap = 4;

	if (tile != 0) {
		w->caption_color = _map_owner[tile];
	} else {
		w->caption_color = _local_player;
	}
}

const byte _aircraft_refit_normal[] = { 0,1,4,5,6,7,8,9,10,0xFF };
const byte _aircraft_refit_arctic[] = { 0,1,4,5,6,7,9,11,10,0xFF };
const byte _aircraft_refit_desert[] = { 0,4,5,8,6,7,9,10,0xFF };
const byte _aircraft_refit_candy[] = { 0,1,3,5,7,8,9,6,4,10,11,0xFF };

const byte * const _aircraft_refit_types[4] = {
	_aircraft_refit_normal, _aircraft_refit_arctic, _aircraft_refit_desert, _aircraft_refit_candy
};

static void AircraftRefitWndProc(Window *w, WindowEvent *e)
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

		DrawString(1, 15, STR_A040_SELECT_CARGO_TYPE_TO_CARRY, 0);

		/* TODO: Support for custom GRFSpecial-specified refitting! --pasky */

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
			b = _aircraft_refit_types[_opt.landscape];
			do {
				show_cargo(*b);
			} while (*++b != 0xFF);
		}

#undef show_cargo

		WP(w,refit_d).cargo = cargo;

		if (cargo != -1) {
			int32 cost = DoCommandByTile(v->tile, v->index, cargo, DC_QUERY_COST, CMD_REFIT_AIRCRAFT);
			if (cost != CMD_ERROR) {
				SetDParam(2, cost);
				SetDParam(0, _cargoc.names_long_p[cargo]);
				SetDParam(1, _aircraft_refit_capacity);
				DrawString(1, 137, STR_A041_NEW_CAPACITY_COST_OF_REFIT, 0);
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
				if (DoCommandP(v->tile, v->index, WP(w,refit_d).cargo, NULL, CMD_REFIT_AIRCRAFT | CMD_MSG(STR_A042_CAN_T_REFIT_AIRCRAFT)))
					DeleteWindow(w);
			}
		  break;
		}
		break;
	}
}

static const Widget _aircraft_refit_widgets[] = {
{    WWT_TEXTBTN,    14,     0,    10,     0,    13, STR_00C5,				STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   239,     0,    13, STR_A03C_REFIT,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,    14,     0,   239,    14,   135, 0x0,							STR_A03E_SELECT_TYPE_OF_CARGO_FOR},
{     WWT_IMGBTN,    14,     0,   239,   136,   157, 0x0,							STR_NULL},
{ WWT_PUSHTXTBTN,    14,     0,   239,   158,   169, STR_A03D_REFIT_AIRCRAFT, STR_A03F_REFIT_AIRCRAFT_TO_CARRY},
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
	int mod;
	Vehicle *v = &_vehicles[w->window_number], *u;

	switch(e->event) {
	case WE_PAINT:
		w->disabled_state = v->owner == _local_player ? 0 : (1 << 2);
		if (!_patches.servint_aircraft) // disable service-scroller when interval is set to disabled
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
			int y = 57;

			do {
				if (v->subtype <= 2) {
					SetDParam(0, GetCustomEngineName(v->engine_type));
					SetDParam(1, 1920 + v->build_year);
					SetDParam(2, v->value);
					DrawString(60, y, STR_A011_BUILT_VALUE, 0);
					y += 10;

					SetDParam(0, _cargoc.names_long_p[v->cargo_type]);
					SetDParam(1, v->cargo_cap);
					u = v->next;
					SetDParam(2, _cargoc.names_long_p[u->cargo_type]);
					SetDParam(3, u->cargo_cap);
					DrawString(60, y, STR_A019_CAPACITY + (u->cargo_cap == 0), 0);
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
		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 2: /* rename */
			SetDParam(0, v->unitnumber);
			ShowQueryString(v->string_id, STR_A030_NAME_AIRCRAFT, 31, 150, w->window_class, w->window_number);
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

			DoCommandP(v->tile, v->index, mod, NULL,
				CMD_CHANGE_AIRCRAFT_SERVICE_INT | CMD_MSG(STR_018A_CAN_T_CHANGE_SERVICING));
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
		DoCommandP(0, w->window_number, 0, NULL, CMD_NAME_VEHICLE | CMD_MSG(STR_A031_CAN_T_NAME_AIRCRAFT));
	} break;

	}
}


static const Widget _aircraft_details_widgets[] = {
{    WWT_TEXTBTN,    14,     0,    10,     0,    13, STR_00C5,					STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   349,     0,    13, STR_A00C_DETAILS,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,    14,   350,   389,     0,    13, STR_01AA_NAME,			STR_A032_NAME_AIRCRAFT},
{     WWT_IMGBTN,    14,     0,   389,    14,    55, 0x0,								STR_NULL},
{     WWT_IMGBTN,    14,     0,   389,    56,   101, 0x0,								STR_NULL},
{ WWT_PUSHTXTBTN,    14,     0,    10,   102,   107, STR_0188,					STR_884D_INCREASE_SERVICING_INTERVAL},
{ WWT_PUSHTXTBTN,    14,     0,    10,   108,   113, STR_0189,					STR_884E_DECREASE_SERVICING_INTERVAL},
{     WWT_IMGBTN,    14,    11,   389,   102,   113, 0x0,								STR_NULL},
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
{    WWT_TEXTBTN,    14,     0,    10,     0,    13, STR_00C5,	STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   249,     0,    13, STR_A00A,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,    14,     0,   231,    14,   103, 0x0,				STR_NULL},
{          WWT_6,    14,     2,   229,    16,   101, 0x0,				STR_NULL},
{ WWT_PUSHIMGBTN,    14,     0,   249,   104,   115, 0x0,				STR_A027_CURRENT_AIRCRAFT_ACTION},
{ WWT_PUSHIMGBTN,    14,   232,   249,    14,    31, 0x2AB,			STR_A029_CENTER_MAIN_VIEW_ON_AIRCRAFT},
{ WWT_PUSHIMGBTN,    14,   232,   249,    32,    49, 0x2AF,			STR_A02A_SEND_AIRCRAFT_TO_HANGAR},
{ WWT_PUSHIMGBTN,    14,   232,   249,    50,    67, 0x2B4,			STR_A03B_REFIT_AIRCRAFT_TO_CARRY},
{ WWT_PUSHIMGBTN,    14,   232,   249,    68,    85, 0x2B2,			STR_A028_SHOW_AIRCRAFT_S_ORDERS},
{ WWT_PUSHIMGBTN,    14,   232,   249,    86,   103, 0x2B3,			STR_A02B_SHOW_AIRCRAFT_DETAILS},
{   WIDGETS_END},
};

static void AircraftViewWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		Vehicle *v = &_vehicles[w->window_number];
		uint32 disabled = 1<<7;
		StringID str;

		{
			uint tile = v->tile;
			if (IS_TILETYPE(tile, MP_STATION) &&
					(_map5[tile] == 32 || _map5[tile] == 65) &&
					v->vehstatus&VS_STOPPED)
						disabled = 0;
		}

		if (v->owner != _local_player)
			disabled |= 1<<7 | 1<<6;
		w->disabled_state = disabled;

		/* draw widgets & caption */
		SetDParam(0, v->string_id);
		SetDParam(1, v->unitnumber);
		DrawWindowWidgets(w);

		/* draw the flag */
		DrawSprite((v->vehstatus & VS_STOPPED) ? 0xC12  : 0xC13, 2, 105);

		if (v->vehstatus & VS_CRASHED) {
			str = STR_8863_CRASHED;
		} else if (v->vehstatus & VS_STOPPED) {
			str = STR_8861_STOPPED;
		} else {
			switch(v->next_order & OT_MASK) {
			case OT_GOTO_STATION: {
				SetDParam(0, v->next_order_param);
				SetDParam(1, v->cur_speed * 8);
				str = STR_HEADING_FOR_STATION + _patches.vehicle_speed;
			} break;

			case OT_GOTO_DEPOT: {
				SetDParam(0, v->next_order_param);
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

		DrawStringCentered(125, 105, str, 0);
		DrawWindowViewport(w);
	} break;

	case WE_CLICK: {
		Vehicle *v = &_vehicles[w->window_number];

		switch(e->click.widget) {
		case 4: /* start stop */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_START_STOP_AIRCRAFT | CMD_MSG(STR_A016_CAN_T_STOP_START_AIRCRAFT));
			break;
		case 5: /* center main view */
			ScrollMainWindowTo(v->x_pos, v->y_pos);
			break;
		case 6: /* goto hangar */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_SEND_AIRCRAFT_TO_HANGAR | CMD_MSG(STR_A012_CAN_T_SEND_AIRCRAFT_TO));
			break;
		case 7: /* refit */
			ShowAircraftRefitWindow(v);
			break;
		case 8: /* show orders */
			ShowOrdersWindow(v);
			break;
		case 9: /* show details */
			ShowAircraftDetailsWindow(v);
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


static const WindowDesc _aircraft_view_desc = {
	-1,-1, 250, 116,
	WC_VEHICLE_VIEW ,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
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
	uint tile;
	Vehicle *v;
	int num,x,y;

	tile = w->window_number;

	/* setup disabled buttons */
	w->disabled_state = (_map_owner[tile]==_local_player) ? 0 : ((1<<3)|(1<<5));

	/* determine amount of items for scroller */
	num = 0;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Aircraft && v->subtype <= 2 && v->vehstatus&VS_HIDDEN &&
				v->tile == (TileIndex)tile)
					num++;
	}
	SetVScrollCount(w, (num + 3) >> 2);

	SetDParam(0, _map2[tile]);
	DrawWindowWidgets(w);

	x = 2;
	y = 15;
	num = w->vscroll.pos * 4;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Aircraft &&
				v->subtype <= 2 &&
				v->vehstatus&VS_HIDDEN &&
				v->tile == (TileIndex)tile &&
				--num < 0 && num >=	-8) {

			DrawAircraftImage(v, x+12, y, WP(w,traindepot_d).sel);

			SetDParam(0, v->unitnumber);
			DrawString(x, y+2, (uint16)(v->max_age-366) >= v->age ? STR_00E2 : STR_00E3, 0);

			DrawSprite( (v->vehstatus & VS_STOPPED) ? 0xC12 : 0xC13, x, y+12);

			if ((x+=74) == 2+74*4) {
				x -= 74*4;
				y += 24;
			}
		}
	}
}

static int GetVehicleFromAircraftDepotWndPt(Window *w, int x, int y, Vehicle **veh) {
	uint xt,yt,xm,ym;
	Vehicle *v;
	uint tile;
	int pos;

	xt = x / 74;
	xm = x % 74;
	if (xt >= 4)
		return 1;

	yt = (y - 14) / 24;
	ym = (y - 14) % 24;
	if (yt >= 2)
		return 1;

	pos = (yt + w->vscroll.pos) * 4 + xt;

	tile = w->window_number;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Aircraft &&
				v->subtype <= 2 &&
				v->vehstatus&VS_HIDDEN &&
				v->tile == (TileIndex)tile &&
				--pos < 0) {
					*veh = v;
					if (xm >= 12)
						return 0;

					if (ym <= 12)
						return -1; /* show window */

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

static const Widget _aircraft_depot_widgets[] = {
{    WWT_TEXTBTN,    14,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   330,     0,    13, STR_A002_AIRCRAFT_HANGAR, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX,    14,     0,   295,    14,    61, 0x204, STR_A021_AIRCRAFT_CLICK_ON_AIRCRAFT},
{     WWT_IMGBTN,    14,   296,   319,    14,    61, 0x2A9, STR_A023_DRAG_AIRCRAFT_TO_HERE_TO},
{  WWT_SCROLLBAR,    14,   320,   330,    14,    61, 0x0, STR_0190_SCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,    14,     0,   164,    62,    73, STR_A003_NEW_AIRCRAFT, STR_A022_BUILD_NEW_AIRCRAFT},
{ WWT_PUSHTXTBTN,    14,   165,   330,    62,    73, STR_00E4_LOCATION, STR_A024_CENTER_MAIN_VIEW_ON_HANGAR},
{   WIDGETS_END},
};


static void AircraftDepotWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT:
		DrawAircraftDepotWindow(w);
		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 2: /* click aircraft */
			AircraftDepotClickAircraft(w, e->click.pt.x, e->click.pt.y);
			break;
		case 5: /* show build aircraft window */
			ShowBuildAircraftWindow(w->window_number);
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

			if (GetVehicleFromAircraftDepotWndPt(w, e->dragdrop.pt.x, e->dragdrop.pt.y, &v) == 0 &&
					v != NULL &&
					sel == v->index) {
				ShowAircraftViewWindow(v);
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
	}
}



static const WindowDesc _aircraft_depot_desc = {
	-1, -1, 331, 74,
	WC_VEHICLE_DEPOT,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_aircraft_depot_widgets,
	AircraftDepotWndProc
};


void ShowAircraftDepotWindow(uint tile)
{
	Window *w;

	w = AllocateWindowDescFront(&_aircraft_depot_desc, tile);
	if (w) {
		w->caption_color = _map_owner[tile];
		w->vscroll.cap = 2;
		WP(w,traindepot_d).sel = INVALID_VEHICLE;
		_backup_orders_tile = 0;
	}
}

static void DrawSmallSchedule(Vehicle *v, int x, int y) {
	uint16 *sched;
	int sel;
	uint ord;
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
			SetDParam(0, ord >> 8);
			DrawString(x, y, STR_A036, 0);

			y += 6;
			if (++i == 4)
				break;
		}
	}
}

// used to get a sorted list of the vehicles
static SortStruct _aircraft_sort[NUM_NORMAL_VEHICLES];
static uint16 _num_aircraft_sort[MAX_PLAYERS];

static void GlobalSortAircraftList()
{
	const Vehicle *v;
	uint16 *i;
	uint32 n = 0;

	// reset #-of aircraft to 0 because ++ is used for value-assignment
	for (i = _num_aircraft_sort; i != endof(_num_aircraft_sort); i++) {*i = 0;}

	FOR_ALL_VEHICLES(v) {
		if(v->type == VEH_Aircraft && v->subtype <= 2) {
			_aircraft_sort[n].index = v->index;
			_aircraft_sort[n++].owner = v->owner;
			_num_aircraft_sort[v->owner]++; // add number of aircraft of player
		}
	}

	// create cumulative aircraft-ownership
	// aircraft are stored as a cummulative index, eg 25, 41, 43. This means
	// Player0: 25; Player1: (41-25) 16; Player2: (43-41) 2
	for (i = &_num_aircraft_sort[1]; i != endof(_num_aircraft_sort); i++) {*i += *(i-1);}

	qsort(_aircraft_sort, n, sizeof(_aircraft_sort[0]), GeneralOwnerSorter); // sort by owner

	// since indexes are messed up after adding/removing a station, mark all lists dirty
	memset(_aircraft_sort_dirty, true, sizeof(_aircraft_sort_dirty));
	_vehicle_sort_dirty[VEHAIRCRAFT] = false;

	DEBUG(misc, 1) ("Resorting global aircraft list...");	
}

static void MakeSortedAircraftList(byte owner)
{
	SortStruct *firstelement;
	uint32 n = 0;

	if (owner == 0) { // first element starts at 0th element and has n elements as described above
		firstelement =	&_aircraft_sort[0];
		n =							_num_aircraft_sort[0];
	}	else { // nth element starts at the end of the previous one, and has n elements as described above
		firstelement =	&_aircraft_sort[_num_aircraft_sort[owner-1]];
		n =							_num_aircraft_sort[owner] - _num_aircraft_sort[owner-1];
	}

	_internal_sort_order			= _aircraft_sort_order[owner];
	_internal_name_sorter_id	= STR_SV_AIRCRAFT_NAME;
	_last_vehicle_idx = 0; // used for "cache" in namesorting
	qsort(firstelement, n, sizeof(_aircraft_sort[0]), _vehicle_sorter[_aircraft_sort_type[owner]]);

	_aircraft_sort_dirty[owner] = false;

	DEBUG(misc, 1) ("Resorting Aircraft list player %d...", owner+1);
}

static void PlayerAircraftWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		uint32 i;
		const byte window_number = (byte)w->window_number;

		if (_aircraft_sort_type[window_number] == SORT_BY_UNSORTED) // disable 'Sort By' tooltip on Unsorted sorting criteria
			w->disabled_state |= (1 << 2);

		// resort shipps window if roadvehicles have been added/removed
		if (_vehicle_sort_dirty[VEHAIRCRAFT])
			GlobalSortAircraftList();

		if (_aircraft_sort_dirty[window_number]) {
			MakeSortedAircraftList(window_number);
			/* reset sorting timeout */
			w->custom[0] = DAY_TICKS;
			w->custom[1] = PERIODIC_RESORT_DAYS;
		}

		// aircraft are stored as a cummulative index, eg 25, 41, 43. This means
		// Player0: 25; Player1: (41-25) 16; Player2: (43-41) 2 aircraft
		i = (window_number == 0) ? 0 : _num_aircraft_sort[window_number-1];
		SetVScrollCount(w, _num_aircraft_sort[window_number] - i);

		/* draw the widgets */
		{
			Player *p = DEREF_PLAYER(window_number);
			/* Company Name -- (###) Aircraft */
			SetDParam(0, p->name_1);
			SetDParam(1, p->name_2);
			SetDParam(2, w->vscroll.count);
			DrawWindowWidgets(w);
		}
		/* draw sorting criteria string */
		DrawString(85, 15, _vehicle_sort_listing[_aircraft_sort_type[window_number]], 0x10);
		/* draw arrow pointing up/down for ascending/descending soring */
		DoDrawString(_aircraft_sort_order[window_number] & 1 ? "\xAA" : "\xA0", 69, 15, 0x10);

		/* draw the aircraft */
		{
			Vehicle *v;
			int n = 0;
			const int x = 2;			// offset from left side of widget
			int y = PLY_WND_PRC__OFFSET_TOP_WIDGET;	// offset from top of widget
			i += w->vscroll.pos;	// offset from sorted aircraft list of current player

			while (i < _num_aircraft_sort[window_number]) {
				StringID str;
				v = DEREF_VEHICLE(_aircraft_sort[i].index);

				assert(v->type == VEH_Aircraft && v->subtype <= 2 && v->owner == window_number);

				DrawAircraftImage(v, x + 19, y + 6, INVALID_VEHICLE);
				DrawVehicleProfitButton(v, x, y+13);

				SetDParam(0, v->unitnumber);
				if (IsAircraftHangarTile(v->tile)) {
					str = STR_021F;
				} else {
					str = v->age > v->max_age - 366 ? STR_00E3 : STR_00E2;
				}
				DrawString(x, y+2, str, 0);


				SetDParam(0, v->profit_this_year);
				SetDParam(1, v->profit_last_year);
				DrawString(x+19, y + 28, STR_0198_PROFIT_THIS_YEAR_LAST_YEAR, 0);

				if (v->string_id != STR_SV_AIRCRAFT_NAME) {
					SetDParam(0, v->string_id);
					DrawString(x+19, y, STR_01AB, 0);
				}

				DrawSmallSchedule(v, x+136, y);

				y += PLY_WND_PRC__SIZE_OF_ROW_BIG;
				i++; // next aircraft
				if (++n == w->vscroll.cap) { break;} // max number of aircraft in the window
			}
		}
		}	break;

	case WE_CLICK: {
		switch(e->click.widget) {
		case 2: /* Flip sorting method ascending/descending */
			_aircraft_sort_order[(byte)w->window_number] ^= 1;
			_aircraft_sort_dirty[(byte)w->window_number] = true;
			SetWindowDirty(w);
			break;
		case 3: case 4:/* Select sorting criteria dropdown menu */
			ShowDropDownMenu(w, _vehicle_sort_listing, _aircraft_sort_type[(byte)w->window_number], 4, 0); // do it for widget 4
			return;
		case 6: { /* Matrix to show vehicles */
			uint32 id_v = (e->click.pt.y - PLY_WND_PRC__OFFSET_TOP_WIDGET) / PLY_WND_PRC__SIZE_OF_ROW_BIG;

			if (id_v >= w->vscroll.cap) { return;} // click out of bounds

			id_v += w->vscroll.pos;

			{
				const byte owner = (byte)w->window_number;
				Vehicle *v;
				id_v += (owner == 0) ? 0 : _num_aircraft_sort[owner - 1]; // first element in list

				if (id_v >= _num_aircraft_sort[owner]) { return;} // click out of vehicle bound

				v	= DEREF_VEHICLE(_aircraft_sort[id_v].index); // add the offset id_x to that

				assert(v->type == VEH_Aircraft && v->subtype <= 2 && v->owner == owner);

				ShowAircraftViewWindow(v);
			}
		} break;

		case 8: { /* Build new Vehicle */
			uint tile;

			tile = _last_built_aircraft_depot_tile;
			do {
				if (_map_owner[tile] == _local_player && IsAircraftHangarTile(tile)) {
					ShowAircraftDepotWindow(tile);
					ShowBuildAircraftWindow(tile);
					return;
				}

				tile = TILE_MASK(tile + 1);
			} while(tile != _last_built_aircraft_depot_tile);

			ShowBuildAircraftWindow(0);
		} break;
		}
	}	break;

	case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
		if (_aircraft_sort_type[(byte)w->window_number] != e->dropdown.index) // if value hasn't changed, dont resort list
			_aircraft_sort_dirty[(byte)w->window_number] = true;

		_aircraft_sort_type[(byte)w->window_number] = e->dropdown.index;

		if (_aircraft_sort_type[(byte)w->window_number] != SORT_BY_UNSORTED) // enable 'Sort By' if a sorter criteria is chosen
			w->disabled_state &= ~(1 << 2);

		SetWindowDirty(w);
		break;
	case WE_CREATE: /* set up resort timer */
		w->custom[0] = DAY_TICKS;
		w->custom[1] = PERIODIC_RESORT_DAYS;
		break;
	case WE_TICK: /* resort the list every 20 seconds orso (10 days) */
		if (--w->custom[0] == 0) {
			w->custom[0] = DAY_TICKS;
			if (--w->custom[1] == 0) {
				w->custom[1] = PERIODIC_RESORT_DAYS;
				_aircraft_sort_dirty[(byte)w->window_number] = true;
				DEBUG(misc, 1) ("Periodic resort Aircraft list player %d...", w->window_number+1);
				SetWindowDirty(w);
			}
		}
		break;
	}
}

static const Widget _player_aircraft_widgets[] = {
{   WWT_CLOSEBOX,    14,     0,    10,     0,    13, STR_00C5,							STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   259,     0,    13, STR_A009_AIRCRAFT,			STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,    14,     0,    80,    14,    25, SRT_SORT_BY,						STR_SORT_TIP},
{      WWT_PANEL,    14,    81,   237,    14,    25, 0x0,										STR_SORT_TIP},
{   WWT_CLOSEBOX,    14,   238,   248,    14,    25, STR_0225,							STR_SORT_TIP},
{      WWT_PANEL,    14,   249,   259,    14,    25, 0x0,										STR_NULL},
{     WWT_MATRIX,    14,     0,   248,    26,   169, 0x401,									STR_A01F_AIRCRAFT_CLICK_ON_AIRCRAFT},
{  WWT_SCROLLBAR,    14,   249,   259,    26,   169, 0x0,										STR_0190_SCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,    14,     0,   129,   170,   181, STR_A003_NEW_AIRCRAFT,	STR_A020_BUILD_NEW_AIRCRAFT_REQUIRES},
{      WWT_PANEL,    14,   130,   259,   170,   181, 0x0,										STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _player_aircraft_desc = {
	-1, -1, 260, 182,
	WC_AIRCRAFT_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_player_aircraft_widgets,
	PlayerAircraftWndProc
};

static const Widget _other_player_aircraft_widgets[] = {
{   WWT_CLOSEBOX,    14,     0,    10,     0,    13, STR_00C5,							STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   259,     0,    13, STR_A009_AIRCRAFT,			STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,    14,     0,    80,    14,    25, SRT_SORT_BY,						STR_SORT_TIP},
{      WWT_PANEL,    14,    81,   237,    14,    25, 0x0,										STR_SORT_TIP},
{   WWT_CLOSEBOX,    14,   238,   248,    14,    25, STR_0225,							STR_SORT_TIP},
{      WWT_PANEL,    14,   249,   259,    14,    25, 0x0,										STR_NULL},
{     WWT_MATRIX,    14,     0,   248,    26,   169, 0x401,									STR_A01F_AIRCRAFT_CLICK_ON_AIRCRAFT},
{  WWT_SCROLLBAR,    14,   249,   259,    26,   169, 0x0,										STR_0190_SCROLL_BAR_SCROLLS_LIST},
{   WIDGETS_END},
};

static const WindowDesc _other_player_aircraft_desc = {
	-1, -1, 260, 170,
	WC_AIRCRAFT_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_other_player_aircraft_widgets,
	PlayerAircraftWndProc
};

void ShowPlayerAircraft(int player)
{
	Window *w;

	if (player == _local_player) {
		w = AllocateWindowDescFront(&_player_aircraft_desc, player);
	} else  {
		w = AllocateWindowDescFront(&_other_player_aircraft_desc, player);
	}

	if (w) {
		w->caption_color = w->window_number;
		w->vscroll.cap = 4; // maximum number of vehicles shown
	}
}
