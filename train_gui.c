#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "vehicle.h"
#include "viewport.h"
#include "station.h"
#include "command.h"
#include "player.h"
#include "engine.h"
#include "vehicle_gui.h"


int _traininfo_vehicle_pitch = 0;


void CcBuildWagon(bool success, uint tile, uint32 p1, uint32 p2)
{
	Vehicle *v,*found;

	if (!success)
		return;

	// find a locomotive in the depot.
	found = NULL;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Train && v->subtype==0 &&
				v->tile == tile &&
				v->u.rail.track == 0x80) {
			if (found != NULL) // must be exactly one.
				return;
			found = v;
		}
	}

	// if we found a loco,
	if (found != NULL) {
		found = GetLastVehicleInChain(found);
		// put the new wagon at the end of the loco.
		DoCommandP(0, _new_wagon_id | (found->index<<16), 0, NULL, CMD_MOVE_RAIL_VEHICLE);
		RebuildVehicleLists();
	}
}

void CcBuildLoco(bool success, uint tile, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (!success)
		return;

	v = &_vehicles[_new_train_id];
	if (tile == _backup_orders_tile) {
		_backup_orders_tile = 0;
		RestoreVehicleOrders(v, _backup_orders_data);
	}
	ShowTrainViewWindow(v);
}

static void engine_drawing_loop(int *x, int *y, int *pos, int *sel,
	int *selected_id, byte railtype, bool is_engine)
{
	int i;

	for (i = 0; i < NUM_TRAIN_ENGINES; i++) {
		const Engine *e = DEREF_ENGINE(i);
		const RailVehicleInfo *rvi = RailVehInfo(i);

		if (e->railtype != railtype || !(rvi->flags & RVI_WAGON) != is_engine ||
				!HASBIT(e->player_avail, _local_player))
			continue;

		if (*sel == 0) *selected_id = i;

		if (IS_INT_INSIDE(--*pos, -8, 0)) {
			DrawString(*x + 59, *y + 2, GetCustomEngineName(i),
				*sel == 0 ? 0xC : 0x10);
			DrawTrainEngine(*x + 29, *y + 6 + _traininfo_vehicle_pitch, i,
				SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)));
			*y += 14;
		}
		--*sel;
	}
}

static void NewRailVehicleWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT:

		if (w->window_number == 0)
			SETBIT(w->disabled_state, 5);

		{
			int count = 0;
			byte railtype = WP(w,buildtrain_d).railtype;
			int i;

			for (i = 0; i < NUM_TRAIN_ENGINES; i++) {
				const Engine *e = DEREF_ENGINE(i);
				if (e->railtype == railtype
				    && HASBIT(e->player_avail, _local_player))
					count++;
			}
			SetVScrollCount(w, count);
		}

		SetDParam(0, WP(w,buildtrain_d).railtype + STR_881C_NEW_RAIL_VEHICLES);
		DrawWindowWidgets(w);

		{
			byte railtype = WP(w,buildtrain_d).railtype;
			int sel = WP(w,buildtrain_d).sel_index;
			int pos = w->vscroll.pos;
			int x = 1;
			int y = 15;
			int selected_id = -1;

			/* Ensure that custom engines which substituted wagons
			 * are sorted correctly.
			 * XXX - DO NOT EVER DO THIS EVER AGAIN! GRRR hacking in wagons as
			 * engines to get more types.. Stays here until we have our own format
			 * then it is exit!!! */
			engine_drawing_loop(&x, &y, &pos, &sel, &selected_id, railtype, true); // True engines
			engine_drawing_loop(&x, &y, &pos, &sel, &selected_id, railtype, false); // Feeble wagons

			WP(w,buildtrain_d).sel_engine = selected_id;

			if (selected_id != -1) {
				const RailVehicleInfo *rvi = RailVehInfo(selected_id);
				Engine *e;
				YearMonthDay ymd;

				if (!(rvi->flags & RVI_WAGON)) {
					/* it's an engine */
					int multihead = (rvi->flags&RVI_MULTIHEAD?1:0);

					SetDParam(0, rvi->base_cost * (_price.build_railvehicle >> 3) >> 5);
					SetDParam(2, rvi->max_speed * 10 >> 4);
					SetDParam(3, rvi->power << multihead);
					SetDParam(1, rvi->weight << multihead);
					SetDParam(4, (rvi->running_cost_base * _price.running_rail[rvi->engclass] >> 8) << multihead);

					SetDParam(5, STR_8838_N_A);
					if (rvi->capacity != 0) {
						SetDParam(6, rvi->capacity << multihead);
						SetDParam(5, _cargoc.names_long_p[rvi->cargo_type]);
					}

					e = &_engines[selected_id];

					SetDParam(8, e->lifelength);
					SetDParam(9, e->reliability * 100 >> 16);
					ConvertDayToYMD(&ymd, e->intro_date);
					SetDParam(7, ymd.year + 1920);

					DrawString(2, 0x7F, STR_8817_COST_WEIGHT_T_SPEED_POWER, 0);
				} else {
					/* it's a wagon */

					SetDParam(0,
						DoCommandByTile(w->window_number, selected_id, 0, DC_QUERY_COST, CMD_BUILD_RAIL_VEHICLE)
					);
					SetDParam(4, rvi->capacity);
					SetDParam(1, rvi->weight);
					SetDParam(3, _cargoc.names_long_p[rvi->cargo_type]);
					SetDParam(2, (_cargoc.weights[rvi->cargo_type] * rvi->capacity >> 4) + rvi->weight);
					DrawString(2, 0x7F, STR_8821_COST_WEIGHT_T_T_CAPACITY, 0);
				}
			}
		}
	break;

	case WE_CLICK: {
		switch(e->click.widget) {
		case 2: {
			uint i = (e->click.pt.y - 14) / 14;
			if (i < 8) {
				WP(w,buildtrain_d).sel_index = i + w->vscroll.pos;
				SetWindowDirty(w);
			}
		} break;
		case 5: {
			int sel_eng;
			sel_eng = WP(w,buildtrain_d).sel_engine;
			if (sel_eng != -1)
				DoCommandP(w->window_number, sel_eng, 0, (RailVehInfo(sel_eng)->flags & RVI_WAGON) ? CcBuildWagon : CcBuildLoco, CMD_BUILD_RAIL_VEHICLE | CMD_MSG(STR_882B_CAN_T_BUILD_RAILROAD_VEHICLE));
		}	break;
		case 6:
			WP(w,buildtrain_d).rename_engine = WP(w,buildtrain_d).sel_engine;
			ShowQueryString(
				GetCustomEngineName(WP(w,buildtrain_d).sel_engine),
				STR_886A_RENAME_TRAIN_VEHICLE_TYPE,
				31,
				160,
				w->window_class,
				w->window_number);
			break;
		}
	} break;

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
		DoCommandP(0, WP(w,buildtrain_d).rename_engine, 0, NULL, CMD_RENAME_ENGINE | CMD_MSG(STR_886B_CAN_T_RENAME_TRAIN_VEHICLE));
	} break;
	}
}

static const Widget _new_rail_vehicle_widgets[] = {
{   WWT_CLOSEBOX,    14,     0,    10,     0,    13, STR_00C5,	STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   227,     0,    13, STR_0315,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX,    14,     0,   216,    14,   125, 0x801,			STR_8843_TRAIN_VEHICLE_SELECTION},
{  WWT_SCROLLBAR,    14,   217,   227,    14,   125, 0x0,				STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,    14,     0,   227,   126,   187, 0x0,				STR_NULL},
{ WWT_PUSHTXTBTN,    14,     0,   113,   188,   199, STR_881F_BUILD_VEHICLE,	STR_8844_BUILD_THE_HIGHLIGHTED_TRAIN},
{ WWT_PUSHTXTBTN,    14,   114,   227,   188,   199, STR_8820_RENAME,					STR_8845_RENAME_TRAIN_VEHICLE_TYPE},
{   WIDGETS_END},
};

static const WindowDesc _new_rail_vehicle_desc = {
	-1, -1, 228, 200,
	WC_BUILD_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_new_rail_vehicle_widgets,
	NewRailVehicleWndProc
};

static void ShowBuildTrainWindow(uint tile)
{
	Window *w;

	DeleteWindowById(WC_BUILD_VEHICLE, tile);

	w = AllocateWindowDesc(&_new_rail_vehicle_desc);
	w->window_number = tile;
	w->vscroll.cap = 8;

	if (tile != 0) {
		w->caption_color = _map_owner[tile];
		WP(w,buildtrain_d).railtype = _map3_lo[tile] & 0xF;
	} else {
		w->caption_color = _local_player;
		WP(w,buildtrain_d).railtype = DEREF_PLAYER(_local_player)->max_railtype - 1;
	}
}

static void DrawTrainImage(Vehicle *v, int x, int y, int count, int skip, VehicleID selection)
{
	do {
		if (--skip < 0) {
			int image = GetTrainImage(v, 6);
			uint32 ormod = SPRITE_PALETTE(PLAYER_SPRITE_COLOR(v->owner));
			if (v->vehstatus & VS_CRASHED) ormod = 0x3248000;
			DrawSprite(image | ormod, x+14, y+6+_traininfo_vehicle_pitch);
			if (v->index == selection) DrawFrameRect(x-1, y-1, x+28, y+12, 15, 0x10);
			x += 29;
			count--;
		}

		if (!(v = v->next))
			break;
	} while (count);
}

static void DrawTrainDepotWindow(Window *w)
{
	uint tile;
	Vehicle *v, *u;
	int num,x,y,i, hnum;
	Depot *d;

	tile = w->window_number;

	/* setup disabled buttons */
	w->disabled_state = (_map_owner[tile]==_local_player) ? 0 : ((1<<3)|(1<<4)|(1<<6));

	/* determine amount of items for scroller */
	num = 1;
	hnum = 0;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Train &&
				  (v->subtype == 0 || v->subtype == 4) &&
				v->tile == (TileIndex)tile &&
				v->u.rail.track == 0x80) {
					num++;
					// determine number of items in the X direction.
					if (v->subtype == 0) {
						i = -1;
						u = v;
						do i++; while ( (u=u->next) != NULL);
						if (i > hnum) hnum = i;
					}
		}
	}
	SetVScrollCount(w, num);
	SetHScrollCount(w, hnum);

	/* locate the depot struct */
	for(d=_depots; d->xy != (TileIndex)tile; d++) {}

	SetDParam(0,d->town_index);
	DrawWindowWidgets(w);

	x = 2;
	y = 15;
	num = w->vscroll.pos;

	// draw all trains
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Train &&
				v->subtype == 0 &&
				v->tile == (TileIndex)tile &&
				v->u.rail.track == 0x80 &&
				--num < 0 && num >= -6) {

			DrawTrainImage(v, x+21, y, 10, w->hscroll.pos, WP(w,traindepot_d).sel);
			/* Draw the train number */
			SetDParam(0, v->unitnumber);
			DrawString(x, y, (v->max_age - 366 < v->age) ? STR_00E3 : STR_00E2, 0);
			/* Draw the pretty flag */
			DrawSprite(v->vehstatus&VS_STOPPED ? 0xC12 : 0xC13, x+15, y);

			y += 14;
		}
	}

	// draw all remaining vehicles
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Train &&
				v->subtype == 4 &&
				v->tile == (TileIndex)tile &&
				v->u.rail.track == 0x80 &&
				--num < 0 && num >= -6) {

			DrawTrainImage(v, x+50, y, 9, 0, WP(w,traindepot_d).sel);
			DrawString(x, y+2, STR_8816, 0);
			y += 14;
		}
	}
}

typedef struct GetDepotVehiclePtData {
	Vehicle *head;
	Vehicle *wagon;
} GetDepotVehiclePtData;

static int GetVehicleFromTrainDepotWndPt(Window *w, int x, int y, GetDepotVehiclePtData *d)
{
	int area_x;
	int row;
	Vehicle *v;

	x = x - 23;
	if (x < 0) {
		area_x = (x >= -10) ? -2 : -1;
	} else {
		area_x = x / 29;
	}

	row = (y - 14) / 14;
	if ( (uint) row >= 6)
		return 1; /* means err */

	row += w->vscroll.pos;

	/* go through all the locomotives */
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Train &&
				v->subtype == 0 &&
				v->tile == w->window_number &&
				v->u.rail.track == 0x80 &&
				--row < 0) {
					if (area_x >= 0) area_x += w->hscroll.pos;
					goto found_it;
		}
	}

	area_x--; /* free wagons don't have an initial loco. */

	/* and then the list of free wagons */
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Train &&
				v->subtype == 4 &&
				v->tile == w->window_number &&
				v->u.rail.track == 0x80 &&
				--row < 0)
					goto found_it;
	}

	d->head = NULL;
	d->wagon = NULL;

	/* didn't find anything, get out */
	return 0;

found_it:
	d->head = d->wagon = v;

	/* either pressed the flag or the number, but only when it's a loco */
	if (area_x < 0 && v->subtype==0)
		return area_x;

	/* find the vehicle in this row that was clicked */
	while (--area_x >= 0) {
		v = v->next;
		if (v == NULL) break;
	}

	d->wagon = v;

	return 0;
}

static void TrainDepotMoveVehicle(Vehicle *wagon, int sel, Vehicle *head)
{
	Vehicle *v;

	v = &_vehicles[sel];

	if (/*v->subtype == 0 ||*/ v == wagon)
		return;

	if (wagon == NULL) {
		if (head != NULL)
			wagon = GetLastVehicleInChain(head);
	} else  {
		wagon = GetPrevVehicleInChain(wagon);
		if (wagon == NULL)
			return;
	}

	if (wagon == v)
		return;

	DoCommandP(v->tile, v->index + ((wagon==NULL ? (uint32)-1 : wagon->index) << 16), _ctrl_pressed ? 1 : 0, NULL, CMD_MOVE_RAIL_VEHICLE | CMD_MSG(STR_8837_CAN_T_MOVE_VEHICLE));
}

static void TrainDepotClickTrain(Window *w, int x, int y)
{
	GetDepotVehiclePtData gdvp;
	int mode, sel;
	Vehicle *v;

	mode = GetVehicleFromTrainDepotWndPt(w, x, y, &gdvp);

	// share / copy orders
	if (_thd.place_mode && mode <= 0) { _place_clicked_vehicle = gdvp.head; return; }

	v = gdvp.wagon;

	switch(mode) {
	case 0: // start dragging of vehicle
		sel = (int16)WP(w,traindepot_d).sel;
		if (sel != -1) {
			WP(w,traindepot_d).sel = INVALID_VEHICLE;
			TrainDepotMoveVehicle(v, sel, gdvp.head);
		} else if (v != NULL) {
			WP(w,traindepot_d).sel = v->index;
			SetObjectToPlaceWnd( SPRITE_PALETTE(PLAYER_SPRITE_COLOR(v->owner)) + GetTrainImage(v, 6), 4, w);
			SetWindowDirty(w);
		}
		break;

	case -1: // show info window
		ShowTrainViewWindow(v);
		break;

	case -2: // click start/stop flag
		DoCommandP(v->tile, v->index, 0, NULL, CMD_START_STOP_TRAIN | CMD_MSG(STR_883B_CAN_T_STOP_START_TRAIN));
		break;
	}
}

static void TrainDepotWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT:
		DrawTrainDepotWindow(w);
		break;

	case WE_CLICK: {
		switch(e->click.widget) {
		case 6:
			ShowBuildTrainWindow(w->window_number);
			break;
		case 7:
			ScrollMainWindowToTile(w->window_number);
			break;
		case 2:
			TrainDepotClickTrain(w, e->click.pt.x, e->click.pt.y);
			break;
		}
	} break;

	case WE_DESTROY:
		DeleteWindowById(WC_BUILD_VEHICLE, w->window_number);
		break;

	case WE_DRAGDROP: {
		switch(e->click.widget) {
		case 4:
		case 3: {
			Vehicle *v;
			int sell_cmd;

			/* sell vehicle */
			if (w->disabled_state & (1 << e->click.widget))
				return;

			if (WP(w,traindepot_d).sel == INVALID_VEHICLE)
				return;

			v = &_vehicles[WP(w,traindepot_d).sel];

			WP(w,traindepot_d).sel = INVALID_VEHICLE;
			SetWindowDirty(w);

			HandleButtonClick(w, e->click.widget);

			sell_cmd = (e->click.widget == 4 || _ctrl_pressed) ? 1 : 0;

			if (v->subtype != 0) {
				DoCommandP(v->tile, v->index, sell_cmd, NULL, CMD_SELL_RAIL_WAGON | CMD_MSG(STR_8839_CAN_T_SELL_RAILROAD_VEHICLE));
			} else {
				_backup_orders_tile = v->tile;
				BackupVehicleOrders(v, _backup_orders_data);
				if (!DoCommandP(v->tile, v->index, sell_cmd, NULL, CMD_SELL_RAIL_WAGON | CMD_MSG(STR_8839_CAN_T_SELL_RAILROAD_VEHICLE)))
					_backup_orders_tile = 0;
			}
		}	break;

		case 2: {
				GetDepotVehiclePtData gdvp;
				VehicleID sel = WP(w,traindepot_d).sel;

				WP(w,traindepot_d).sel = INVALID_VEHICLE;
				SetWindowDirty(w);

				if (GetVehicleFromTrainDepotWndPt(w, e->dragdrop.pt.x, e->dragdrop.pt.y, &gdvp) == 0 &&
						sel != INVALID_VEHICLE) {
					if (gdvp.wagon == NULL || gdvp.wagon->index != sel) {
						TrainDepotMoveVehicle(gdvp.wagon, sel, gdvp.head);
					} else if (gdvp.head != NULL && gdvp.head->subtype==0) {
						ShowTrainViewWindow(gdvp.head);
					}
				}
			} break;

		default:
			WP(w,traindepot_d).sel = INVALID_VEHICLE;
			SetWindowDirty(w);
			break;
		}
		} break;
	}
}


static const Widget _train_depot_widgets[] = {
{   WWT_CLOSEBOX,    14,     0,    10,     0,    13, STR_00C5,							STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   348,     0,    13, STR_8800_TRAIN_DEPOT,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX,    14,     0,   313,    14,    97, 0x601,									STR_883F_TRAINS_CLICK_ON_TRAIN_FOR},
{      WWT_PANEL,    14,   314,   337,    14,    54, 0x2A9,									STR_8841_DRAG_TRAIN_VEHICLE_TO_HERE},
{      WWT_PANEL,    14,   314,   337,    55,   108, 0x2BF,									STR_DRAG_WHOLE_TRAIN_TO_SELL_TIP},

{  WWT_SCROLLBAR,    14,   338,   348,    14,   108, 0x0,										STR_0190_SCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,    14,     0,   173,    109,  120, STR_8815_NEW_VEHICLES,	STR_8840_BUILD_NEW_TRAIN_VEHICLE},
{ WWT_PUSHTXTBTN,    14,   174,   348,    109,  120, STR_00E4_LOCATION,			STR_8842_CENTER_MAIN_VIEW_ON_TRAIN},
{ WWT_HSCROLLBAR,    14,     0,   313,    98,   108, 0x0,										STR_0190_SCROLL_BAR_SCROLLS_LIST},
{   WIDGETS_END},
};

static const WindowDesc _train_depot_desc = {
	-1, -1, 349, 121,
	WC_VEHICLE_DEPOT,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_train_depot_widgets,
	TrainDepotWndProc
};


void ShowTrainDepotWindow(uint tile)
{
	Window *w;

	w = AllocateWindowDescFront(&_train_depot_desc, tile);
	if (w) {
		w->caption_color = _map_owner[w->window_number];
		w->vscroll.cap = 6;
		w->hscroll.cap = 9;
		WP(w,traindepot_d).sel = INVALID_VEHICLE;
		_backup_orders_tile = 0;
	}
}

const byte _rail_vehicle_refit_types[4][16] = {
	{ 0,1,2,4,5,6,7,8,9,10,0xFF },	// normal
	{ 0,1,4,5,6,7,9,11,10,0xFF },		// arctic
	{ 0,4,5,8,6,7,9,10,0xFF },			// desert
	{ 0,1,3,5,7,8,9,6,4,10,11,0xFF }// candy
};

static void RailVehicleRefitWndProc(Window *w, WindowEvent *e)
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
			b = _rail_vehicle_refit_types[_opt.landscape];
			do {
				show_cargo(*b);
			} while (*++b != 255);
		}

#undef show_cargo

		WP(w,refit_d).cargo = cargo;

		if (cargo != -1) {
			int32 cost = DoCommandByTile(v->tile, v->index, cargo, 0, CMD_REFIT_RAIL_VEHICLE);
			if (cost != CMD_ERROR) {
				SetDParam(2, cost);
				SetDParam(0, _cargoc.names_long_p[cargo]);
				SetDParam(1, _returned_refit_amount);
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
				if (DoCommandP(v->tile, v->index, WP(w,refit_d).cargo, NULL, CMD_REFIT_RAIL_VEHICLE | CMD_MSG(STR_RAIL_CAN_T_REFIT_VEHICLE)))
					DeleteWindow(w);
			}
			break;
		}
		break;
	}
}


static const Widget _rail_vehicle_refit_widgets[] = {
{    WWT_TEXTBTN,    14,     0,    10,     0,    13, STR_00C5,							STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   239,     0,    13, STR_983B_REFIT,				STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,    14,     0,   239,    14,   135, 0x0,										STR_RAIL_SELECT_TYPE_OF_CARGO_FOR},
{     WWT_IMGBTN,    14,     0,   239,   136,   157, 0x0,										STR_NULL},
{ WWT_PUSHTXTBTN,    14,     0,   239,   158,   169, STR_RAIL_REFIT_VEHICLE,STR_RAIL_REFIT_TO_CARRY_HIGHLIGHTED},
{   WIDGETS_END},
};

static const WindowDesc _rail_vehicle_refit_desc = {
	-1,-1, 240, 170,
	WC_VEHICLE_REFIT,WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_rail_vehicle_refit_widgets,
	RailVehicleRefitWndProc,
};

static void ShowRailVehicleRefitWindow(Vehicle *v)
{
	Window *w;
	DeleteWindowById(WC_VEHICLE_REFIT, v->index);
	_alloc_wnd_parent_num = v->index;
	w = AllocateWindowDesc(&_rail_vehicle_refit_desc);
	w->window_number = v->index;
	w->caption_color = v->owner;
	WP(w,refit_d).sel = -1;
}

static Widget _train_view_widgets[] = {
{   WWT_CLOSEBOX,    14,     0,    10,     0,    13, STR_00C5,STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   249,     0,    13, STR_882E,STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,    14,     0,   231,    14,   121, 0x0,			STR_NULL},
{          WWT_6,    14,     2,   229,    16,   119, 0x0,			STR_NULL},
{ WWT_PUSHIMGBTN,    14,     0,   249,   122,   133, 0x0,			STR_8846_CURRENT_TRAIN_ACTION_CLICK},
{ WWT_PUSHIMGBTN,    14,   232,   249,    14,    31, 0x2AB,		STR_8848_CENTER_MAIN_VIEW_ON_TRAIN},
{ WWT_PUSHIMGBTN,    14,   232,   249,    32,    49, 0x2AD,		STR_8849_SEND_TRAIN_TO_DEPOT},
{ WWT_PUSHIMGBTN,    14,   232,   249,    50,    67, 0x2B1,		STR_884A_FORCE_TRAIN_TO_PROCEED},
{ WWT_PUSHIMGBTN,    14,   232,   249,    68,    85, 0x2CB,		STR_884B_REVERSE_DIRECTION_OF_TRAIN},
{ WWT_PUSHIMGBTN,    14,   232,   249,    86,   103, 0x2B2,		STR_8847_SHOW_TRAIN_S_ORDERS},
{ WWT_PUSHIMGBTN,    14,   232,   249,   104,   121, 0x2B3,		STR_884C_SHOW_TRAIN_DETAILS},
{ WWT_PUSHIMGBTN,    14,   232,   249,    68,    85, 0x2B4,		STR_RAIL_REFIT_VEHICLE_TO_CARRY},
{   WIDGETS_END},
};

static void TrainViewWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		Vehicle *v;
		StringID str;

		v = &_vehicles[w->window_number];

		w->disabled_state = (v->owner == _local_player) ? 0 : 0x1C0;


		/* draw widgets & caption */
		SetDParam(0, v->string_id);
		SetDParam(1, v->unitnumber);
		DrawWindowWidgets(w);

		/* draw the flag */
		DrawSprite( (v->vehstatus&VS_STOPPED) ? 0xC12 : 0xC13, 2, 123);

		if (v->u.rail.crash_anim_pos != 0) {
			str = STR_8863_CRASHED;
		} else if (v->breakdown_ctr == 1) {
			str = STR_885C_BROKEN_DOWN;
		} else if (v->vehstatus & VS_STOPPED) {
			if (v->u.rail.last_speed == 0) {
				str = STR_8861_STOPPED;
			} else {
				SetDParam(0, v->u.rail.last_speed * 10 >> 4);
				str = STR_TRAIN_STOPPING + _patches.vehicle_speed;
			}
		} else {
			switch (v->current_order.type) {
			case OT_GOTO_STATION: {
				str = STR_HEADING_FOR_STATION + _patches.vehicle_speed;
				SetDParam(0, v->current_order.station);
				SetDParam(1, v->u.rail.last_speed * 10 >> 4);
			} break;

			case OT_GOTO_DEPOT: {
				Depot *dep = &_depots[v->current_order.station];
				SetDParam(0, dep->town_index);
				str = STR_HEADING_FOR_TRAIN_DEPOT + _patches.vehicle_speed;
				SetDParam(1, v->u.rail.last_speed * 10 >> 4);
			} break;

			case OT_LOADING:
			case OT_LEAVESTATION:
				str = STR_882F_LOADING_UNLOADING;
				break;

			case OT_GOTO_WAYPOINT: {
				SetDParam(0, v->current_order.station);
				str = STR_HEADING_FOR_WAYPOINT + _patches.vehicle_speed;
				SetDParam(1, v->u.rail.last_speed * 10 >> 4);
				break;
			}

			default:
				if (v->num_orders == 0) {
					str = STR_NO_ORDERS + _patches.vehicle_speed;
					SetDParam(0, v->u.rail.last_speed * 10 >> 4);
				} else
					str = STR_EMPTY;
				break;
			}
		}

		DrawStringCentered(125, 123, str, 0);
		DrawWindowViewport(w);
	}	break;

	case WE_CLICK: {
		int wid = e->click.widget;
		Vehicle *v = &_vehicles[w->window_number];

		switch(wid) {
		case 4: /* start/stop train */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_START_STOP_TRAIN | CMD_MSG(STR_883B_CAN_T_STOP_START_TRAIN));
			break;
		case 5:	/* center main view */
			ScrollMainWindowTo(v->x_pos, v->y_pos);
			break;
		case 6:	/* goto depot */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_TRAIN_GOTO_DEPOT | CMD_MSG(STR_8830_CAN_T_SEND_TRAIN_TO_DEPOT));
			break;
		case 7: /* force proceed */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_FORCE_TRAIN_PROCEED | CMD_MSG(STR_8862_CAN_T_MAKE_TRAIN_PASS_SIGNAL));
			break;
		case 8: /* reverse direction */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_REVERSE_TRAIN_DIRECTION | CMD_MSG(STR_8869_CAN_T_REVERSE_DIRECTION));
			break;
		case 9: /* show train orders */
			ShowOrdersWindow(v);
			break;
		case 10: /* show train details */
			ShowTrainDetailsWindow(v);
			break;
		case 11:
			ShowRailVehicleRefitWindow(v);
			break;
		}
	} break;

	case WE_DESTROY:
		DeleteWindowById(WC_VEHICLE_REFIT, w->window_number);
		DeleteWindowById(WC_VEHICLE_ORDERS, w->window_number);
		DeleteWindowById(WC_VEHICLE_DETAILS, w->window_number);
		break;

	case WE_MOUSELOOP: {
		Vehicle *v;
		uint32 h;

		v = &_vehicles[w->window_number];
		assert(v->type == VEH_Train);
		h = CheckStoppedInDepot(v) >= 0 ? (1 << 8) : (1 << 11);
		if (h != w->hidden_state) {
			w->hidden_state = h;
			SetWindowDirty(w);
		}
		break;
	}

	}
}

static const WindowDesc _train_view_desc = {
	-1,-1, 250, 134,
	WC_VEHICLE_VIEW,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_train_view_widgets,
	TrainViewWndProc
};

void ShowTrainViewWindow(Vehicle *v)
{
	Window *w;

	w = AllocateWindowDescFront(&_train_view_desc,v->index);
	if (w) {
		w->caption_color = v->owner;
		AssignWindowViewport(w, 3, 17, 0xE2, 0x66, w->window_number | (1 << 31), 0);
	}
}

static void TrainDetailsCargoTab(Vehicle *v, int x, int y)
{
	int num;
	StringID str;

	if (v->cargo_cap != 0) {
		num = v->cargo_count;
		str = STR_8812_EMPTY;
		if (num != 0) {
			SetDParam(0, v->cargo_type);
			SetDParam(1, num);
			SetDParam(2, v->cargo_source);
			str = STR_8813_FROM;
		}
		DrawString(x, y, str, 0);
	}
}

static void TrainDetailsInfoTab(Vehicle *v, int x, int y)
{
	const RailVehicleInfo *rvi = RailVehInfo(v->engine_type);

	if (!(rvi->flags & RVI_WAGON)) {
		SetDParam(0, GetCustomEngineName(v->engine_type));
		SetDParam(1, v->build_year + 1920);
		SetDParam(2, v->value);
		DrawString(x, y, STR_882C_BUILT_VALUE, 0x10);
	} else {
		SetDParam(0, GetCustomEngineName(v->engine_type));
		SetDParam(1, v->value);
		DrawString(x, y, STR_882D_VALUE, 0x10);
	}
}

static void TrainDetailsCapacityTab(Vehicle *v, int x, int y)
{
	if (v->cargo_cap != 0) {
		SetDParam(1, v->cargo_cap);
		SetDParam(0, _cargoc.names_long_p[v->cargo_type]);
		DrawString(x, y, STR_013F_CAPACITY, 0);
	}
}

typedef void TrainDetailsDrawerProc(Vehicle *v, int x, int y);

static TrainDetailsDrawerProc * const _train_details_drawer_proc[3] = {
	TrainDetailsCargoTab,
	TrainDetailsInfoTab,
	TrainDetailsCapacityTab,
};

static void DrawTrainDetailsWindow(Window *w)
{
	Vehicle *v, *u;
	uint16 tot_cargo[NUM_CARGO][2];	// count total cargo ([0]-actual cargo, [1]-total cargo)
	int i,num,x,y,sel;
	StringID str;
	byte det_tab = WP(w, traindetails_d).tab;

	/* Count number of vehicles */
	num = 0;

	// det_tab == 3 <-- Total Cargo tab
	if (det_tab == 3)	// reset tot_cargo array to 0 values
		memset(tot_cargo, 0, sizeof(tot_cargo));

	u = v = &_vehicles[w->window_number];
	do {
		if (det_tab != 3)
			num++;
		else {
			tot_cargo[u->cargo_type][0] += u->cargo_count;
			tot_cargo[u->cargo_type][1] += u->cargo_cap;
		}
	} while ( (u = u->next) != NULL);

	/*	set scroll-amount seperately from counting, as to not
			compute num double for more carriages of the same type
	*/
	if (det_tab == 3) {
		for (i = 0; i != NUM_CARGO; i++) {
			if (tot_cargo[i][1] > 0)	// only count carriages that the train has
				num++;
		}
		num++;	// needs one more because first line is description string
	}

	SetVScrollCount(w, num);

	w->disabled_state = 1 << (det_tab + 9);
	if (v->owner != _local_player)
		w->disabled_state |= (1 << 2);

	if (!_patches.servint_trains) // disable service-scroller when interval is set to disabled
		w->disabled_state |= (1 << 6) | (1 << 7);

	SetDParam(0, v->string_id);
	SetDParam(1, v->unitnumber);
	DrawWindowWidgets(w);

	num = v->age / 366;
	SetDParam(1, num);

	x = 2;

	str = STR_0199_YEAR;
	if (num != 1) {
		str += STR_019A_YEARS - STR_0199_YEAR;
		if ((uint16)(v->max_age - 366) < v->age)
			str += STR_019B_YEARS - STR_019A_YEARS;
	}
	SetDParam(0, str);
	SetDParam(2, v->max_age / 366);
	SetDParam(3, GetTrainRunningCost(v) >> 8);
	DrawString(x, 15, STR_885D_AGE_RUNNING_COST_YR, 0);

	SetDParam(2, v->max_speed * 10 >> 4);
	SetDParam(1, v->u.rail.cached_power);
	SetDParam(0, v->u.rail.cached_weight);
	DrawString(x, 25, STR_885E_WEIGHT_T_POWER_HP_MAX_SPEED, 0);

	SetDParam(0, v->profit_this_year);
	SetDParam(1, v->profit_last_year);
	DrawString(x, 35, STR_885F_PROFIT_THIS_YEAR_LAST_YEAR, 0);

	SetDParam(0, 100 * (v->reliability>>8) >> 8);
	SetDParam(1, v->breakdowns_since_last_service);
	DrawString(x, 45, STR_8860_RELIABILITY_BREAKDOWNS, 0);

	SetDParam(0, v->service_interval);
	SetDParam(1, v->date_of_last_service);
	DrawString(x + 11, 141, _patches.servint_ispercent?STR_SERVICING_INTERVAL_PERCENT:STR_883C_SERVICING_INTERVAL_DAYS, 0);

	x = 1;
	y = 57;
	sel = w->vscroll.pos;

	// draw the first 3 details tabs
	if (det_tab != 3) {
		for(;;) {
			if (--sel < 0 && sel >= -6) {
				DrawTrainImage(v, x, y, 1, 0, INVALID_VEHICLE);
				_train_details_drawer_proc[WP(w,traindetails_d).tab](v, x + 30, y + 2);
				y += 14;
			}
			if ( (v=v->next) == NULL)
				return;
		}
	}
	else {	// draw total cargo tab
		i = 0;
		DrawString(x, y + 2, STR_013F_TOTAL_CAPACITY_TEXT, 0);
		do {
			if (tot_cargo[i][1] > 0 && --sel < 0 && sel >= -5) {
				y += 14;
				// STR_013F_TOTAL_CAPACITY			:{LTBLUE}- {CARGO} ({SHORTCARGO})
				SetDParam(0, i);								// {CARGO} #1
				SetDParam(1, tot_cargo[i][0]);	// {CARGO} #2
				SetDParam(2, i);								// {SHORTCARGO} #1
				SetDParam(3, tot_cargo[i][1]);	// {SHORTCARGO} #2
				DrawString(x, y, STR_013F_TOTAL_CAPACITY, 0);
			}
		} while (++i != NUM_CARGO);
	}
}

static void TrainDetailsWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT:
		DrawTrainDetailsWindow(w);
		break;
	case WE_CLICK: {
		int mod;
		Vehicle *v;
		switch(e->click.widget) {
		case 2: /* name train */
			v = &_vehicles[w->window_number];
			SetDParam(0, v->unitnumber);
			ShowQueryString(v->string_id, STR_8865_NAME_TRAIN, 31, 150, w->window_class, w->window_number);
			break;
		case 6:	/* inc serv interval */
			mod = _ctrl_pressed? 5 : 10;
			goto do_change_service_int;

		case 7: /* dec serv interval */
			mod = _ctrl_pressed? -5 : -10;
do_change_service_int:
			v = &_vehicles[w->window_number];
			mod += v->service_interval;

				/*	%-based service interval max 5%-90%
						day-based service interval max 30-800 days */
				mod = _patches.servint_ispercent ? clamp(mod, MIN_SERVINT_PERCENT, MAX_SERVINT_PERCENT) : clamp(mod, MIN_SERVINT_DAYS, MAX_SERVINT_DAYS+1);
				if (mod == v->service_interval)
					return;

			DoCommandP(v->tile, v->index, mod, NULL, CMD_CHANGE_TRAIN_SERVICE_INT | CMD_MSG(STR_018A_CAN_T_CHANGE_SERVICING));
			break;
		/* details buttons*/
		case 9:		// Cargo
		case 10:	// Information
		case 11:	// Capacities
		case 12:	// Total cargo
			CLRBIT(w->disabled_state, 9);
			CLRBIT(w->disabled_state, 10);
			CLRBIT(w->disabled_state, 11);
			CLRBIT(w->disabled_state, 12);
			SETBIT(w->disabled_state, e->click.widget);
			WP(w,traindetails_d).tab = e->click.widget - 9;
			SetWindowDirty(w);
			break;
		}
	} break;

	case WE_4:
		if (FindWindowById(WC_VEHICLE_VIEW, w->window_number) == NULL)
			DeleteWindow(w);
		break;

	case WE_ON_EDIT_TEXT: {
		byte *b = e->edittext.str;
		if (*b == 0)
			return;
		memcpy(_decode_parameters, b, 32);
		DoCommandP(0, w->window_number, 0, NULL, CMD_NAME_VEHICLE | CMD_MSG(STR_8866_CAN_T_NAME_TRAIN));
	} break;
	}
}

static const Widget _train_details_widgets[] = {
{   WWT_CLOSEBOX,    14,     0,    10,     0,    13, STR_00C5,				STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   329,     0,    13, STR_8802_DETAILS,STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,    14,   330,   369,     0,    13, STR_01AA_NAME,		STR_8867_NAME_TRAIN},
{      WWT_PANEL,    14,     0,   369,    14,    55, 0x0,							STR_NULL},
{     WWT_MATRIX,    14,     0,   358,    56,   139, 0x601,						STR_NULL},
{  WWT_SCROLLBAR,    14,   359,   369,    56,   139, 0x0,							STR_0190_SCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,    14,     0,    10,   140,   145, STR_0188,				STR_884D_INCREASE_SERVICING_INTERVAL},
{ WWT_PUSHTXTBTN,    14,     0,    10,   146,   151, STR_0189,				STR_884E_DECREASE_SERVICING_INTERVAL},
{      WWT_PANEL,    14,    11,   369,   140,   151, 0x0,							STR_NULL},
{ WWT_PUSHTXTBTN,    14,     0,    92,   152,   163, STR_013C_CARGO,	STR_884F_SHOW_DETAILS_OF_CARGO_CARRIED},
{ WWT_PUSHTXTBTN,    14,    93,   184,   152,   163, STR_013D_INFORMATION,	STR_8850_SHOW_DETAILS_OF_TRAIN_VEHICLES},
{ WWT_PUSHTXTBTN,    14,   185,   277,   152,   163, STR_013E_CAPACITIES,		STR_8851_SHOW_CAPACITIES_OF_EACH},
{ WWT_PUSHTXTBTN,    14,   278,   369,   152,   163, STR_013E_TOTAL_CARGO,	STR_8852_SHOW_TOTAL_CARGO},
{   WIDGETS_END},
};


static const WindowDesc _train_details_desc = {
	-1,-1, 370, 164,
	WC_VEHICLE_DETAILS,WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_train_details_widgets,
	TrainDetailsWndProc
};


void ShowTrainDetailsWindow(Vehicle *v)
{
	Window *w;
	VehicleID veh = v->index;

	DeleteWindowById(WC_VEHICLE_ORDERS, veh);
	DeleteWindowById(WC_VEHICLE_DETAILS, veh);

	_alloc_wnd_parent_num = veh;
	w = AllocateWindowDesc(&_train_details_desc);

	w->window_number = veh;
	w->caption_color = v->owner;
	w->vscroll.cap = 6;
	WP(w,traindetails_d).tab = 0;
}


static Widget _player_trains_widgets[] = {
{   WWT_CLOSEBOX,    14,     0,    10,     0,    13, STR_00C5,							STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   324,     0,    13, STR_881B_TRAINS,				STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,    14,     0,    80,    14,    25, SRT_SORT_BY,           STR_SORT_TIP},
{      WWT_PANEL,    14,    81,   232,    14,    25, 0x0,			              STR_SORT_TIP},
{   WWT_CLOSEBOX,    14,   233,   243,    14,    25, STR_0225,              STR_SORT_TIP},
{      WWT_PANEL,    14,   244,   324,    14,    25, 0x0,										STR_NULL},
{     WWT_MATRIX,    14,     0,   313,    26,   207, 0x701,									STR_883D_TRAINS_CLICK_ON_TRAIN_FOR},
{  WWT_SCROLLBAR,    14,   314,   324,    26,   207, 0x0,										STR_0190_SCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN,    14,     0,   161,   208,   219, STR_8815_NEW_VEHICLES,	STR_883E_BUILD_NEW_TRAINS_REQUIRES},
{      WWT_PANEL,    14,   162,   324,   208,   219, 0x0,										STR_NULL},
{   WIDGETS_END},
};

static Widget _other_player_trains_widgets[] = {
{   WWT_CLOSEBOX,    14,     0,    10,     0,    13, STR_00C5,							STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,    14,    11,   324,     0,    13, STR_881B_TRAINS,				STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,    14,     0,    80,    14,    25, SRT_SORT_BY,           STR_SORT_TIP},
{      WWT_PANEL,    14,    81,   232,    14,    25, 0x0,										STR_SORT_TIP},
{   WWT_CLOSEBOX,    14,   233,   243,    14,    25, STR_0225,              STR_SORT_TIP},
{      WWT_PANEL,    14,   244,   324,    14,    25, 0x0,										STR_NULL},
{     WWT_MATRIX,    14,     0,   313,    26,   207, 0x701,									STR_883D_TRAINS_CLICK_ON_TRAIN_FOR},
{  WWT_SCROLLBAR,    14,   314,   324,    26,   207, 0x0,										STR_0190_SCROLL_BAR_SCROLLS_LIST},
{   WIDGETS_END},
};

static void PlayerTrainsWndProc(Window *w, WindowEvent *e)
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

		BuildVehicleList(vl, VEH_Train, owner, station);
		SortVehicleList(vl);

		SetVScrollCount(w, vl->list_length);

		// disable 'Sort By' tooltip on Unsorted sorting criteria
		if (vl->sort_type == SORT_BY_UNSORTED)
			w->disabled_state |= (1 << 2);

		/* draw the widgets */
		{
			const Player *p = DEREF_PLAYER(owner);
			/* XXX hack */
			if (station == -1) {
				/* Company Name -- (###) Trains */
				SetDParam(0, p->name_1);
				SetDParam(1, p->name_2);
				SetDParam(2, w->vscroll.count);
				_player_trains_widgets[1].unkA = STR_881B_TRAINS;
				_other_player_trains_widgets[1].unkA = STR_881B_TRAINS;
			} else {
				/* Station Name -- (###) Trains */
				SetDParam(0, DEREF_STATION(station)->index);
				SetDParam(1, w->vscroll.count);
				_player_trains_widgets[1].unkA = STR_SCHEDULED_TRAINS;
				_other_player_trains_widgets[1].unkA = STR_SCHEDULED_TRAINS;
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

			assert(v->type == VEH_Train && v->owner == owner);

			DrawTrainImage(
				v, x + 21, y + 6 + _traininfo_vehicle_pitch, 10, 0, INVALID_VEHICLE);
			DrawVehicleProfitButton(v, x, y + 13);

			SetDParam(0, v->unitnumber);
			if (IsTrainDepotTile(v->tile))
				str = STR_021F;
			else
				str = v->age > v->max_age - 366 ? STR_00E3 : STR_00E2;
			DrawString(x, y + 2, str, 0);

			SetDParam(0, v->profit_this_year);
			SetDParam(1, v->profit_last_year);
			DrawString(x + 21, y + 18, STR_0198_PROFIT_THIS_YEAR_LAST_YEAR, 0);

			if (v->string_id != STR_SV_TRAIN_NAME) {
				SetDParam(0, v->string_id);
				DrawString(x + 21, y, STR_01AB, 0);
			}

			y += PLY_WND_PRC__SIZE_OF_ROW_SMALL;
		}
		break;
	}

	case WE_CLICK: {
		switch(e->click.widget) {
		case 2: /* Flip sorting method ascending/descending */
			vl->flags ^= VL_DESC;
			vl->flags |= VL_RESORT;
			SetWindowDirty(w);
			break;

		case 3: case 4:/* Select sorting criteria dropdown menu */
			ShowDropDownMenu(w, _vehicle_sort_listing, vl->sort_type, 4, 0);
			return;

		case 6: { /* Matrix to show vehicles */
			uint32 id_v = (e->click.pt.y - PLY_WND_PRC__OFFSET_TOP_WIDGET) / PLY_WND_PRC__SIZE_OF_ROW_SMALL;

			if (id_v >= w->vscroll.cap) { return;} // click out of bounds

			id_v += w->vscroll.pos;

			{
				Vehicle *v;

				if (id_v >= vl->list_length) return; // click out of list bound

				v	= DEREF_VEHICLE(vl->sort_list[id_v].index);

				assert(v->type == VEH_Train && v->subtype == 0 && v->owner == owner);

				ShowTrainViewWindow(v);
			}
		} break;

		case 8: { /* Build new Vehicle */
			uint tile;

			tile = _last_built_train_depot_tile;
			do {
				if (_map_owner[tile] == _local_player && IsTrainDepotTile(tile)) {
					ShowTrainDepotWindow(tile);
					ShowBuildTrainWindow(tile);
					return;
				}

				tile = TILE_MASK(tile + 1);
			} while(tile != _last_built_train_depot_tile);

			ShowBuildTrainWindow(0);
		} break;
		}
	}	break;

	case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
		if (vl->sort_type != e->dropdown.index) {
			// value has changed -> resort
			vl->flags |= VL_RESORT;
			vl->sort_type = e->dropdown.index;

			// enable 'Sort By' if a sorter criteria is chosen
			if (vl->sort_type != SORT_BY_UNSORTED)
				w->disabled_state &= ~(1 << 2);
		}
		SetWindowDirty(w);
		break;

	case WE_CREATE: /* set up resort timer */
		vl->sort_list = NULL;
		vl->flags = VL_REBUILD;
		vl->sort_type = SORT_BY_UNSORTED;
		vl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
		break;

	case WE_TICK: /* resort the list every 20 seconds orso (10 days) */
		if (--vl->resort_timer == 0) {
			DEBUG(misc, 1) ("Periodic resort trains list player %d station %d",
				owner, station);
			vl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
			vl->flags |= VL_RESORT;
			SetWindowDirty(w);
		}
		break;
	}
}

static const WindowDesc _player_trains_desc = {
	-1, -1, 325, 220,
	WC_TRAINS_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_player_trains_widgets,
	PlayerTrainsWndProc
};

static const WindowDesc _other_player_trains_desc = {
	-1, -1, 325, 208,
	WC_TRAINS_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_other_player_trains_widgets,
	PlayerTrainsWndProc
};

void ShowPlayerTrains(int player, int station)
{
	Window *w;

	if (player == _local_player) {
		w = AllocateWindowDescFront(&_player_trains_desc, (station << 16) | player);
	} else {
		w = AllocateWindowDescFront(&_other_player_trains_desc, (station << 16) | player);
	}
	if (w) {
		w->caption_color = w->window_number;
		w->vscroll.cap = 7; // maximum number of vehicles shown
	}
}
