/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "train.h"
#include "roadveh.h"
#include "ship.h"
#include "aircraft.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "gui.h"
#include "gfx.h"
#include "vehicle.h"
#include "viewport.h"
#include "command.h"
#include "depot.h"
#include "vehicle_gui.h"
#include "station_map.h"
#include "newgrf_engine.h"
#include "resize_window_widgets.h"

/*
 * Since all depot window sizes aren't the same, we need to modify sizes a little.
 * It's done with the following arrays of widget indexes. Each of them tells if a widget side should be moved and in what direction.
 * How long they should be moved and for what window types are controlled in ShowDepotWindow()
 */

/* Names of the widgets. Keep them in the same order as in the widget array */
typedef enum DepotWindowWidgets {
	DEPOT_WIDGET_CLOSEBOX = 0,
	DEPOT_WIDGET_CAPTION,
	DEPOT_WIDGET_STICKY,
	DEPOT_WIDGET_STOP_ALL,
	DEPOT_WIDGET_START_ALL,
	DEPOT_WIDGET_SELL,
	DEPOT_WIDGET_SELL_CHAIN,
	DEPOT_WIDGET_SELL_ALL,
	DEPOT_WIDGET_MATRIX,
	DEPOT_WIDGET_V_SCROLL, // Vertical scrollbar
	DEPOT_WIDGET_H_SCROLL, // Horizontal scrollbar
	DEPOT_WIDGET_BUILD,
	DEPOT_WIDGET_CLONE,
	DEPOT_WIDGET_LOCATION,
	DEPOT_WIDGET_AUTOREPLACE,
	DEPOT_WIDGET_RESIZE,
	DEPOT_WIDGET_LAST, // used to assert if DepotWindowWidgets and widget_moves got different lengths. Due to this usage, it needs to be last
} DepotWindowWidget;

/* Define how to move each widget. The order is important */
static const byte widget_moves[] = {
	WIDGET_MOVE_NONE,               // DEPOT_WIDGET_CLOSEBOX
	WIDGET_STRETCH_RIGHT,           // DEPOT_WIDGET_CAPTION
	WIDGET_MOVE_RIGHT,              // DEPOT_WIDGET_STICKY
	WIDGET_MOVE_RIGHT,              // DEPOT_WIDGET_STOP_ALL
	WIDGET_MOVE_RIGHT,              // DEPOT_WIDGET_START_ALL
	WIDGET_MOVE_RIGHT,              // DEPOT_WIDGET_SELL
	WIDGET_MOVE_NONE,               // DEPOT_WIDGET_SELL_CHAIN
	WIDGET_MOVE_DOWN_RIGHT,         // DEPOT_WIDGET_SELL_ALL
	WIDGET_STRETCH_DOWN_RIGHT,      // DEPOT_WIDGET_MATRIX
	WIDGET_MOVE_RIGHT_STRETCH_DOWN, // DEPOT_WIDGET_V_SCROLL
	WIDGET_MOVE_NONE,               // DEPOT_WIDGET_H_SCROLL
	WIDGET_MOVE_DOWN,               // DEPOT_WIDGET_BUILD
	WIDGET_MOVE_DOWN,               // DEPOT_WIDGET_CLONE
	WIDGET_MOVE_DOWN,               // DEPOT_WIDGET_LOCATION
	WIDGET_MOVE_DOWN_RIGHT,         // DEPOT_WIDGET_AUTOREPLACE
	WIDGET_MOVE_DOWN_RIGHT,         // DEPOT_WIDGET_RESIZE
};

/* Widget array for all depot windows.
 * If a widget is needed in some windows only (like train specific), add it for all windows
 * and use w->hidden_state in ShowDepotWindow() to remove it in the windows where it should not be
 * Keep the widget numbers in sync with the enum or really bad stuff will happen!!! */

/* When adding widgets, place them as you would place them for the ship depot and define how you want it to move in widget_moves[]
 * If you want a widget for one window only, set it to be hidden in ShowDepotWindow() for the windows where you don't want it
 * NOTE: the train only widgets are moved/resized in ShowDepotWindow() so they follow certain other widgets if they are moved to ensure that they stick together.
 *    Changing the size of those here will not have an effect at all. It should be done in ShowDepotWindow()
 */
static const Widget _depot_widgets[] = {
	{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,            STR_018B_CLOSE_WINDOW},            // DEPOT_WIDGET_CLOSEBOX
	{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   292,     0,    13, 0x0,                 STR_018C_WINDOW_TITLE_DRAG_THIS},  // DEPOT_WIDGET_CAPTION
	{  WWT_STICKYBOX,     RESIZE_LR,    14,   293,   304,     0,    13, 0x0,                 STR_STICKY_BUTTON},                // DEPOT_WIDGET_STICKY

	{ WWT_PUSHIMGBTN,     RESIZE_LR,    14,   270,   280,    14,    25, SPR_FLAG_VEH_STOPPED,STR_MASS_STOP_DEPOT_TOOLTIP},      // DEPOT_WIDGET_STOP_ALL
	{ WWT_PUSHIMGBTN,     RESIZE_LR,    14,   281,   292,    14,    25, SPR_FLAG_VEH_RUNNING,STR_MASS_START_DEPOT_TOOLTIP},     // DEPOT_WIDGET_START_ALL
	{     WWT_IMGBTN,    RESIZE_LRB,    14,   270,   292,    26,    60, 0x2A9,               STR_NULL},                         // DEPOT_WIDGET_SELL
	{      WWT_PANEL,   RESIZE_LRTB,    14,   326,   348,     0,     0, 0x2BF,               STR_DRAG_WHOLE_TRAIN_TO_SELL_TIP}, // DEPOT_WIDGET_SELL_CHAIN, trains only
	{ WWT_PUSHIMGBTN,   RESIZE_LRTB,    14,   270,   292,    61,    83, 0x0,                 STR_DEPOT_SELL_ALL_BUTTON_TIP},    // DEPOT_WIDGET_SELL_ALL

	{     WWT_MATRIX,     RESIZE_RB,    14,     0,   269,    14,    83, 0x0,                 STR_NULL},                         // DEPOT_WIDGET_MATRIX
	{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   293,   304,    14,    83, 0x0,                 STR_0190_SCROLL_BAR_SCROLLS_LIST}, // DEPOT_WIDGET_V_SCROLL

	{ WWT_HSCROLLBAR,    RESIZE_RTB,    14,     0,   325,    98,   109, 0x0,                 STR_HSCROLL_BAR_SCROLLS_LIST},     // DEPOT_WIDGET_H_SCROLL, trains only

	/* The buttons in the bottom of the window. left and right is not important as they are later resized to be equal in size
	 * This calculation is based on right in DEPOT_WIDGET_LOCATION and it presumes left of DEPOT_WIDGET_BUILD is 0            */
	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,    96,    84,    95, 0x0,                 STR_NULL},                         // DEPOT_WIDGET_BUILD
	{WWT_NODISTXTBTN,     RESIZE_TB,    14,    97,   194,    84,    95, 0x0,                 STR_NULL},                         // DEPOT_WIDGET_CLONE
	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   195,   292,    84,    95, STR_00E4_LOCATION,   STR_NULL},                         // DEPOT_WIDGET_LOCATION
	{ WWT_PUSHIMGBTN,   RESIZE_LRTB,    14,   281,   292,    84,    95, 0x0,                 STR_DEPOT_AUTOREPLACE_TIP},        // DEPOT_WIDGET_AUTOREPLACE
	{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   293,   304,    84,    95, 0x0,                 STR_RESIZE_BUTTON},                // DEPOT_WIDGET_RESIZE
	{   WIDGETS_END},
};

static void DepotWndProc(Window *w, WindowEvent *e);

static const WindowDesc _train_depot_desc = {
	-1, -1, 361, 122,
	WC_VEHICLE_DEPOT,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_depot_widgets,
	DepotWndProc
};

static const WindowDesc _road_depot_desc = {
	-1, -1, 315, 82,
	WC_VEHICLE_DEPOT,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_depot_widgets,
	DepotWndProc
};

static const WindowDesc _ship_depot_desc = {
	-1, -1, 305, 96,
	WC_VEHICLE_DEPOT,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_depot_widgets,
	DepotWndProc
};

static const WindowDesc _aircraft_depot_desc = {
	-1, -1, 331, 96,
	WC_VEHICLE_DEPOT,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_depot_widgets,
	DepotWndProc
};

extern int WagonLengthToPixels(int len);

void CcCloneVehicle(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (!success) return;
	switch(GetVehicle(p1)->type) {
		case VEH_Train:    CcCloneTrain(   true, tile, p1, p2); break;
		case VEH_Road:     CcCloneRoadVeh( true, tile, p1, p2); break;
		case VEH_Ship:     CcCloneShip(    true, tile, p1, p2); break;
		case VEH_Aircraft: CcCloneAircraft(true, tile, p1, p2); break;
	}
}

static inline void ShowVehicleViewWindow(const Vehicle *v)
{
	switch (v->type) {
		case VEH_Train:    ShowTrainViewWindow(v);    break;
		case VEH_Road:     ShowRoadVehViewWindow(v);  break;
		case VEH_Ship:     ShowShipViewWindow(v);     break;
		case VEH_Aircraft: ShowAircraftViewWindow(v); break;
		default: NOT_REACHED();
	}
}

static void DepotSellAllWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT:
			if (WP(w, depot_d).type == VEH_Aircraft) {
				SetDParam(0, GetStationIndex(w->window_number)); // Airport name
			} else {
				Depot *depot = GetDepotByTile(w->window_number);
				assert(depot != NULL);

				SetDParam(0, depot->town_index);
			}
			DrawWindowWidgets(w);

			DrawStringCentered(150, 25, STR_DEPOT_SELL_ALL_VEHICLE_CONFIRM, 0);
			DrawStringCentered(150, 38, STR_ARE_YOU_SURE, 0);
			break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case 4:
					DoCommandP(w->window_number, WP(w, depot_d).type, 0, NULL, CMD_DEPOT_SELL_ALL_VEHICLES);
					/* Fallthought */
				case 3:
					DeleteWindow(w);
					break;
			}
			break;
	}
}

static const Widget _depot_sell_all_widgets[] = {
	{   WWT_CLOSEBOX,   RESIZE_NONE,     5,     0,    10,     0,    13, STR_00C5,        STR_018B_CLOSE_WINDOW},
	{    WWT_CAPTION,   RESIZE_NONE,     5,    11,   299,     0,    13, 0x0,             STR_018C_WINDOW_TITLE_DRAG_THIS},
	{      WWT_PANEL,   RESIZE_NONE,     5,     0,   299,    14,    71, 0x0,             STR_NULL},
	{ WWT_PUSHTXTBTN,   RESIZE_NONE,     5,    85,   144,    52,    63, STR_012E_CANCEL, STR_DEPOT_SELL_ALL_CANCEL_TIP},
	{ WWT_PUSHTXTBTN,   RESIZE_NONE,     4,   155,   214,    52,    63, STR_SELL,        STR_DEPOT_SELL_ALL_TIP},
	{   WIDGETS_END},
};

static const WindowDesc _depot_sell_all_desc = {
	WDP_CENTER, WDP_CENTER, 300, 72,
	WC_DEPOT_SELL_ALL,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_depot_sell_all_widgets,
	DepotSellAllWndProc
};

static void ShowDepotSellAllWindow(TileIndex tile, byte type)
{
	Window *w;

	w = AllocateWindowDescFront(&_depot_sell_all_desc, tile);

	if (w != NULL) {
		WP(w, depot_d).type = type;
		switch (type) {
			case VEH_Train:	   w->widget[1].data = STR_8800_TRAIN_DEPOT;        break;
			case VEH_Road:     w->widget[1].data = STR_9003_ROAD_VEHICLE_DEPOT; break;
			case VEH_Ship:     w->widget[1].data = STR_9803_SHIP_DEPOT;         break;
			case VEH_Aircraft:
				w->widget[1].data     = STR_A002_AIRCRAFT_HANGAR;
				w->widget[3].tooltips = STR_DEPOT_SELL_ALL_CANCEL_HANGAR_TIP;
				w->widget[4].tooltips = STR_DEPOT_SELL_ALL_HANGAR_TIP;
				break;
		}
	}
}

static void DrawDepotWindow(Window *w)
{
	Vehicle **vl = WP(w, depot_d).vehicle_list;
	TileIndex tile;
	int x, y, i, hnum, max;
	uint16 num = WP(w, depot_d).engine_count;

	tile = w->window_number;

	/* setup disabled buttons */
	w->disabled_state =
		IsTileOwner(tile, _local_player) ? 0 : ( (1 << DEPOT_WIDGET_STOP_ALL) | (1 << DEPOT_WIDGET_START_ALL) |
			(1 << DEPOT_WIDGET_SELL) | (1 << DEPOT_WIDGET_SELL_CHAIN) | (1 << DEPOT_WIDGET_SELL_ALL) |
			(1 << DEPOT_WIDGET_BUILD) | (1 << DEPOT_WIDGET_CLONE) | (1 << DEPOT_WIDGET_AUTOREPLACE));

	/* determine amount of items for scroller */
	if (WP(w, depot_d).type == VEH_Train) {
		hnum = 8;
		for (num = 0; num < WP(w, depot_d).engine_count; num++) {
			const Vehicle *v = vl[num];
			hnum = maxu(hnum, v->u.rail.cached_total_length);
		}
		/* Always have 1 empty row, so people can change the setting of the train */
		SetVScrollCount(w, WP(w, depot_d).engine_count + WP(w, depot_d).wagon_count + 1);
		SetHScrollCount(w, WagonLengthToPixels(hnum));
	} else {
		SetVScrollCount(w, (num + w->hscroll.cap - 1) / w->hscroll.cap);
	}

	/* locate the depot struct */
	if (WP(w, depot_d).type == VEH_Aircraft) {
		SetDParam(0, GetStationIndex(tile)); // Airport name
	} else {
		Depot *depot = GetDepotByTile(tile);
		assert(depot != NULL);

		SetDParam(0, depot->town_index);
	}

	DrawWindowWidgets(w);

	x = 2;
	y = 15;
	num = w->vscroll.pos * w->hscroll.cap;
	if (WP(w, depot_d).type == VEH_Train) {
		max = min(WP(w, depot_d).engine_count, w->vscroll.pos + w->vscroll.cap);
		num = w->vscroll.pos;
	} else {
		max = min(WP(w, depot_d).engine_count, num + (w->vscroll.cap * w->hscroll.cap));
		num = w->vscroll.pos * w->hscroll.cap;
	}

	for (; num < max; num++) {
		const Vehicle *v = vl[num];
		byte diff_x = 0, diff_y = 0;

		switch (WP(w, depot_d).type) {
			case VEH_Train:
				DrawTrainImage(v, x + 21, y, w->hscroll.cap + 4, w->hscroll.pos, WP(w,depot_d).sel);

				/* Number of wagons relative to a standard length wagon (rounded up) */
				SetDParam(0, (v->u.rail.cached_total_length + 7) / 8);
				DrawStringRightAligned(w->widget[DEPOT_WIDGET_MATRIX].right - 1, y + 4, STR_TINY_BLACK, 0); // Draw the counter
				break;

			case VEH_Road:     DrawRoadVehImage( v, x + 24, y, WP(w, depot_d).sel); break;
			case VEH_Ship:     DrawShipImage(    v, x + 19, y, WP(w, depot_d).sel); break;
			case VEH_Aircraft: DrawAircraftImage(v, x + 12, y, WP(w, depot_d).sel); break;
			default: NOT_REACHED();
		}

		if (w->resize.step_height == 14) {
			/* VEH_Train and VEH_Road, which are low */
			diff_x = 15;
		} else {
			/* VEH_Ship and VEH_Aircraft, which are tall */
			diff_y = 12;
		}

		DrawSprite((v->vehstatus & VS_STOPPED) ? SPR_FLAG_VEH_STOPPED : SPR_FLAG_VEH_RUNNING, x + diff_x, y + diff_y);

		SetDParam(0, v->unitnumber);
		DrawString(x, y + 2, (uint16)(v->max_age-366) >= v->age ? STR_00E2 : STR_00E3, 0);

		if (WP(w, depot_d).type == VEH_Train) {
			y += w->resize.step_height;
		} else {
			if ((x += w->resize.step_width) == 2 + (int)w->resize.step_width * w->hscroll.cap) {
				x = 2;
				y += w->resize.step_height;
			}
		}
	}

	max = min(WP(w, depot_d).engine_count + WP(w, depot_d).wagon_count, w->vscroll.pos + w->vscroll.cap);

	/* draw the train wagons, that do not have an engine in front */
	for (; num < max; num++) {
		const Vehicle *v = WP(w, depot_d).wagon_list[num - WP(w, depot_d).engine_count];
		const Vehicle *u;

		DrawTrainImage(v, x + 50, y, w->hscroll.cap - 29, 0, WP(w,depot_d).sel);
		DrawString(x, y + 2, STR_8816, 0);

		/*Draw the train counter */
		i = 0;
		u = v;
		do i++; while ( (u=u->next) != NULL); // Determine length of train
		SetDParam(0, i);                      // Set the counter
		DrawStringRightAligned(w->widget[DEPOT_WIDGET_MATRIX].right - 1, y + 4, STR_TINY_BLACK, 0); // Draw the counter
		y += 14;
	}
}

typedef struct GetDepotVehiclePtData {
	Vehicle *head;
	Vehicle *wagon;
} GetDepotVehiclePtData;

enum {
	MODE_ERROR        =  1,
	MODE_DRAG_VEHICLE =  0,
	MODE_SHOW_VEHICLE = -1,
	MODE_START_STOP   = -2,
};

static int GetVehicleFromDepotWndPt(const Window *w, int x, int y, Vehicle **veh, GetDepotVehiclePtData *d)
{
	Vehicle **vl = WP(w, depot_d).vehicle_list;
	uint xt, row, xm = 0, ym = 0;
	int pos, skip = 0;

	if (WP(w, depot_d).type == VEH_Train) {
		xt = 0;
		x -= 23;
	} else {
		xt = x / w->resize.step_width;
		xm = x % w->resize.step_width;
		if (xt >= w->hscroll.cap) return MODE_ERROR;

		ym = (y - 14) % w->resize.step_height;
	}

	row = (y - 14) / w->resize.step_height;
	if (row >= w->vscroll.cap) return MODE_ERROR;

	pos = (row + w->vscroll.pos) * (WP(w, depot_d).type == VEH_Train ? 1 : w->hscroll.cap) + xt;

	if (WP(w, depot_d).engine_count + WP(w, depot_d).wagon_count <= pos) {
		if (WP(w, depot_d).type == VEH_Train) {
			d->head  = NULL;
			d->wagon = NULL;
			return MODE_DRAG_VEHICLE;
		} else {
			return MODE_ERROR; // empty block, so no vehicle is selected
		}
	}

	if (WP(w, depot_d).engine_count > pos) {
		*veh = vl[pos];
		skip = w->hscroll.pos;
	} else {
		vl = WP(w, depot_d).wagon_list;
		pos -= WP(w, depot_d).engine_count;
		*veh = vl[pos];
		/* free wagons don't have an initial loco. */
		x -= _traininfo_vehicle_width;
	}

	switch (WP(w, depot_d).type) {
		case VEH_Train: {
			Vehicle *v = *veh;
			d->head = d->wagon = v;

			/* either pressed the flag or the number, but only when it's a loco */
			if (x < 0 && IsFrontEngine(v)) return (x >= -10) ? MODE_START_STOP : MODE_SHOW_VEHICLE;

			skip = (skip * 8) / _traininfo_vehicle_width;
			x = (x * 8) / _traininfo_vehicle_width;

			/* Skip vehicles that are scrolled off the list */
			x += skip;

			/* find the vehicle in this row that was clicked */
			while (v != NULL && (x -= v->u.rail.cached_veh_length) >= 0) v = v->next;

			/* if an articulated part was selected, find its parent */
			while (v != NULL && IsArticulatedPart(v)) v = GetPrevVehicleInChain(v);

			d->wagon = v;

			return MODE_DRAG_VEHICLE;
			}
			break;

		case VEH_Road:
			if (xm >= 24) return MODE_DRAG_VEHICLE;
			if (xm <= 16) return MODE_SHOW_VEHICLE;
			break;

		case VEH_Ship:
			if (xm >= 19) return MODE_DRAG_VEHICLE;
			if (ym <= 10) return MODE_SHOW_VEHICLE;
			break;

		case VEH_Aircraft:
			if (xm >= 12) return MODE_DRAG_VEHICLE;
			if (ym <= 12) return MODE_SHOW_VEHICLE;
			break;

		default: NOT_REACHED();
	}
	return MODE_START_STOP;
}

static void TrainDepotMoveVehicle(Vehicle *wagon, VehicleID sel, Vehicle *head)
{
	Vehicle *v;

	v = GetVehicle(sel);

	if (v == wagon) return;

	if (wagon == NULL) {
		if (head != NULL) wagon = GetLastVehicleInChain(head);
	} else  {
		wagon = GetPrevVehicleInChain(wagon);
		if (wagon == NULL) return;
	}

	if (wagon == v) return;

	DoCommandP(v->tile, v->index + ((wagon == NULL ? INVALID_VEHICLE : wagon->index) << 16), _ctrl_pressed ? 1 : 0, NULL, CMD_MOVE_RAIL_VEHICLE | CMD_MSG(STR_8837_CAN_T_MOVE_VEHICLE));
}

static void DepotClick(Window *w, int x, int y)
{
	GetDepotVehiclePtData gdvp;
	Vehicle *v = NULL;
	int mode = GetVehicleFromDepotWndPt(w, x, y, &v, &gdvp);

	/* share / copy orders */
	if (_thd.place_mode && mode <= 0) {
		_place_clicked_vehicle = (WP(w, depot_d).type == VEH_Train ? gdvp.head : v);
		return;
	}

	if (WP(w, depot_d).type == VEH_Train) v = gdvp.wagon;

	switch (mode) {
		case MODE_ERROR: // invalid
			return;

		case MODE_DRAG_VEHICLE: { // start dragging of vehicle
			VehicleID sel = WP(w, depot_d).sel;

			if (WP(w, depot_d).type == VEH_Train && sel != INVALID_VEHICLE) {
				WP(w,depot_d).sel = INVALID_VEHICLE;
				TrainDepotMoveVehicle(v, sel, gdvp.head);
			} else if (v != NULL) {
				int image;

				switch (WP(w, depot_d).type) {
					case VEH_Train:    image = GetTrainImage(v, DIR_W);    break;
					case VEH_Road:     image = GetRoadVehImage(v, DIR_W);  break;
					case VEH_Ship:     image = GetShipImage(v, DIR_W);     break;
					case VEH_Aircraft: image = GetAircraftImage(v, DIR_W); break;
					default: NOT_REACHED(); image = 0;
				}

				WP(w, depot_d).sel = v->index;
				SetWindowDirty(w);
				SetObjectToPlaceWnd(GetVehiclePalette(v) | image, 4, w);
			}
			}
			break;

		case MODE_SHOW_VEHICLE: // show info window
			ShowVehicleViewWindow(v);
			break;

		case MODE_START_STOP: { // click start/stop flag
			uint command;

			switch (WP(w, depot_d).type) {
				case VEH_Train:    command = CMD_START_STOP_TRAIN | CMD_MSG(STR_883B_CAN_T_STOP_START_TRAIN);          break;
				case VEH_Road:     command = CMD_START_STOP_ROADVEH | CMD_MSG(STR_9015_CAN_T_STOP_START_ROAD_VEHICLE); break;
				case VEH_Ship:     command = CMD_START_STOP_SHIP | CMD_MSG(STR_9818_CAN_T_STOP_START_SHIP);            break;
				case VEH_Aircraft: command = CMD_START_STOP_AIRCRAFT | CMD_MSG(STR_A016_CAN_T_STOP_START_AIRCRAFT);    break;
				default: NOT_REACHED(); command = 0;
			}
			DoCommandP(v->tile, v->index, 0, NULL, command);
			}
			break;

		default: NOT_REACHED();
	}
}

/**
 * Clones a vehicle
 * @param *v is the original vehicle to clone
 * @param *w is the window of the depot where the clone is build
 */
static void HandleCloneVehClick(const Vehicle *v, const Window *w)
{
	uint error_str;

	if (v == NULL) return;

	if (v->type == VEH_Train && !IsFrontEngine(v)) {
		v = GetFirstVehicleInChain(v);
		/* Do nothing when clicking on a train in depot with no loc attached */
		if (!IsFrontEngine(v)) return;
	}

	switch (v->type) {
		case VEH_Train:    error_str = CMD_MSG(STR_882B_CAN_T_BUILD_RAILROAD_VEHICLE); break;
		case VEH_Road:     error_str = CMD_MSG(STR_9009_CAN_T_BUILD_ROAD_VEHICLE);     break;
		case VEH_Ship:     error_str = CMD_MSG(STR_980D_CAN_T_BUILD_SHIP);             break;
		case VEH_Aircraft: error_str = CMD_MSG(STR_A008_CAN_T_BUILD_AIRCRAFT);         break;
		default: return;
	}

	DoCommandP(w->window_number, v->index, _ctrl_pressed ? 1 : 0, CcCloneVehicle, CMD_CLONE_VEHICLE | error_str);

	ResetObjectToPlace();
}

static void ClonePlaceObj(const Window *w)
{
	const Vehicle *v = CheckMouseOverVehicle();

	if (v != NULL) HandleCloneVehClick(v, w);
}

static void ResizeDepotButtons(Window *w)
{
	/* We got the widget moved around. Now we will make some widgets to fill the gab between some widgets in equal sizes */

	/* Make the buttons in the bottom equal in size */
	w->widget[DEPOT_WIDGET_LOCATION].right = w->widget[DEPOT_WIDGET_AUTOREPLACE].left - 1;
	w->widget[DEPOT_WIDGET_BUILD].right    = w->widget[DEPOT_WIDGET_LOCATION].right / 3;
	w->widget[DEPOT_WIDGET_LOCATION].left  = w->widget[DEPOT_WIDGET_BUILD].right * 2;
	w->widget[DEPOT_WIDGET_CLONE].left     = w->widget[DEPOT_WIDGET_BUILD].right + 1;
	w->widget[DEPOT_WIDGET_CLONE].right    = w->widget[DEPOT_WIDGET_LOCATION].left - 1;

	if (WP(w, depot_d).type == VEH_Train) {
		/* Divide the size of DEPOT_WIDGET_SELL into two equally big buttons so DEPOT_WIDGET_SELL and DEPOT_WIDGET_SELL_CHAIN will get the same size.
		* This way it will stay the same even if DEPOT_WIDGET_SELL_CHAIN is resized for some reason                                                  */
		w->widget[DEPOT_WIDGET_SELL_CHAIN].bottom = w->widget[DEPOT_WIDGET_SELL_ALL].top - 1;
		w->widget[DEPOT_WIDGET_SELL_CHAIN].top    = ((w->widget[DEPOT_WIDGET_SELL_CHAIN].bottom - w->widget[DEPOT_WIDGET_SELL].top) / 2) + w->widget[DEPOT_WIDGET_SELL].top;
		w->widget[DEPOT_WIDGET_SELL].bottom     = w->widget[DEPOT_WIDGET_SELL_CHAIN].top - 1;
	}
}

static void DepotWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_CREATE:
			WP(w, depot_d).vehicle_list = NULL;
			WP(w, depot_d).wagon_list   = NULL;
			WP(w, depot_d).engine_count = 0;
			WP(w, depot_d).wagon_count  = 0;
			break;

		case WE_PAINT:
			/* Generate the vehicle list
			 * It's ok to use the wagon pointers for non-trains as they will be ignored */
			BuildDepotVehicleList(WP(w, depot_d).type, w->window_number,
				&WP(w, depot_d).vehicle_list, &WP(w, depot_d).engine_list_length, &WP(w, depot_d).engine_count,
			    &WP(w, depot_d).wagon_list,   &WP(w, depot_d).wagon_list_length,  &WP(w, depot_d).wagon_count);
			DrawDepotWindow(w);
			break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case DEPOT_WIDGET_MATRIX: // List
					DepotClick(w, e->we.click.pt.x, e->we.click.pt.y);
					break;

				case DEPOT_WIDGET_BUILD: // Build vehicle
					ResetObjectToPlace();
					switch (WP(w, depot_d).type) {
						case VEH_Train:    ShowBuildTrainWindow(w->window_number);    break;
						case VEH_Road:     ShowBuildRoadVehWindow(w->window_number);  break;
						case VEH_Ship:     ShowBuildShipWindow(w->window_number);     break;
						case VEH_Aircraft: ShowBuildAircraftWindow(w->window_number); break;
					default: NOT_REACHED();
					}
					break;

				case DEPOT_WIDGET_CLONE: // Clone button
					InvalidateWidget(w, DEPOT_WIDGET_CLONE);
					TOGGLEBIT(w->click_state, DEPOT_WIDGET_CLONE);

					if (HASBIT(w->click_state, DEPOT_WIDGET_CLONE)) {
						_place_clicked_vehicle = NULL;
						SetObjectToPlaceWnd(SPR_CURSOR_CLONE, VHM_RECT, w);
					} else {
						ResetObjectToPlace();
					}
						break;

				case DEPOT_WIDGET_LOCATION: ScrollMainWindowToTile(w->window_number); break;

				case DEPOT_WIDGET_STOP_ALL:
				case DEPOT_WIDGET_START_ALL:
					DoCommandP(w->window_number, WP(w, depot_d).type, e->we.click.widget == DEPOT_WIDGET_START_ALL ? 1 : 0, NULL, CMD_MASS_START_STOP);
					break;

				case DEPOT_WIDGET_SELL_ALL:
					ShowDepotSellAllWindow(w->window_number, WP(w, depot_d).type);
					break;

				case DEPOT_WIDGET_AUTOREPLACE:
					DoCommandP(w->window_number, WP(w, depot_d).type, 0, NULL, CMD_DEPOT_MASS_AUTOREPLACE);
					break;

			}
			break;

		case WE_PLACE_OBJ: {
			ClonePlaceObj(w);
		} break;

		case WE_ABORT_PLACE_OBJ: {
			CLRBIT(w->click_state, DEPOT_WIDGET_CLONE);
			InvalidateWidget(w, DEPOT_WIDGET_CLONE);
		} break;

			/* check if a vehicle in a depot was clicked.. */
		case WE_MOUSELOOP: {
			const Vehicle *v = _place_clicked_vehicle;

			/* since OTTD checks all open depot windows, we will make sure that it triggers the one with a clicked clone button */
			if (v != NULL && HASBIT(w->click_state, DEPOT_WIDGET_CLONE)) {
				_place_clicked_vehicle = NULL;
				HandleCloneVehClick(v, w);
			}
		} break;

		case WE_DESTROY:
			DeleteWindowById(WC_BUILD_VEHICLE, w->window_number);
			free((void*)WP(w, depot_d).vehicle_list);
			free((void*)WP(w, depot_d).wagon_list);
			break;

		case WE_DRAGDROP:
			switch (e->we.click.widget) {
				case DEPOT_WIDGET_MATRIX: {
					Vehicle *v;
					VehicleID sel = WP(w, depot_d).sel;

					WP(w, depot_d).sel = INVALID_VEHICLE;
					SetWindowDirty(w);

					if (WP(w, depot_d).type == VEH_Train) {
						GetDepotVehiclePtData gdvp;

						if (GetVehicleFromDepotWndPt(w, e->we.dragdrop.pt.x, e->we.dragdrop.pt.y, &v, &gdvp) == MODE_DRAG_VEHICLE &&
							sel != INVALID_VEHICLE) {
							if (gdvp.wagon != NULL && gdvp.wagon->index == sel && _ctrl_pressed) {
								DoCommandP(GetVehicle(sel)->tile, GetVehicle(sel)->index, true, NULL, CMD_REVERSE_TRAIN_DIRECTION | CMD_MSG(STR_9033_CAN_T_MAKE_VEHICLE_TURN));
							} else if (gdvp.wagon == NULL || gdvp.wagon->index != sel) {
								TrainDepotMoveVehicle(gdvp.wagon, sel, gdvp.head);
							} else if (gdvp.head != NULL && IsFrontEngine(gdvp.head)) {
								ShowTrainViewWindow(gdvp.head);
							}
						}
					} else if (GetVehicleFromDepotWndPt(w, e->we.dragdrop.pt.x, e->we.dragdrop.pt.y, &v, NULL) == MODE_DRAG_VEHICLE &&
						v != NULL &&
						sel == v->index) {
						ShowVehicleViewWindow(v);
					}
				} break;

				case DEPOT_WIDGET_SELL: case DEPOT_WIDGET_SELL_CHAIN:
					if (!HASBIT(w->disabled_state, DEPOT_WIDGET_SELL) &&
						WP(w, depot_d).sel != INVALID_VEHICLE) {
						Vehicle *v;
						uint command;
						int sell_cmd;
						bool is_engine;

						if (HASBIT(w->disabled_state, e->we.click.widget)) return;
						if (WP(w, depot_d).sel == INVALID_VEHICLE) return;

						HandleButtonClick(w, e->we.click.widget);

						v = GetVehicle(WP(w, depot_d).sel);
						WP(w, depot_d).sel = INVALID_VEHICLE;
						SetWindowDirty(w);

						sell_cmd = (v->type == VEH_Train && (e->we.click.widget == DEPOT_WIDGET_SELL_CHAIN || _ctrl_pressed)) ? 1 : 0;

						is_engine = (!(v->type == VEH_Train && !IsFrontEngine(v)));

						if (is_engine) {
							_backup_orders_tile = v->tile;
							BackupVehicleOrders(v, _backup_orders_data);
						}

						switch (v->type) {
							case VEH_Train:    command = CMD_SELL_RAIL_WAGON | CMD_MSG(STR_8839_CAN_T_SELL_RAILROAD_VEHICLE); break;
							case VEH_Road:     command = CMD_SELL_ROAD_VEH | CMD_MSG(STR_9014_CAN_T_SELL_ROAD_VEHICLE);       break;
							case VEH_Ship:     command = CMD_SELL_SHIP | CMD_MSG(STR_980C_CAN_T_SELL_SHIP);                   break;
							case VEH_Aircraft: command = CMD_SELL_AIRCRAFT | CMD_MSG(STR_A01C_CAN_T_SELL_AIRCRAFT);           break;
							default: NOT_REACHED(); command = 0;
						}

						if (!DoCommandP(v->tile, v->index, sell_cmd, NULL, command) && is_engine) _backup_orders_tile = 0;
					}
					break;
				default:
					WP(w, depot_d).sel = INVALID_VEHICLE;
					SetWindowDirty(w);
			}
			break;

		case WE_RESIZE:
			w->vscroll.cap += e->we.sizing.diff.y / (int)w->resize.step_height;
			w->hscroll.cap += e->we.sizing.diff.x / (int)w->resize.step_width;
			w->widget[DEPOT_WIDGET_MATRIX].data = (w->vscroll.cap << 8) + (WP(w, depot_d).type == VEH_Train ? 1 : w->hscroll.cap);
			ResizeDepotButtons(w);
			break;
	}
}

static void SetupStringsForDepotWindow(Window *w, byte type)
{
	switch (type) {
		case VEH_Train:
			w->widget[DEPOT_WIDGET_CAPTION].data      = STR_8800_TRAIN_DEPOT;
			w->widget[DEPOT_WIDGET_SELL].tooltips     = STR_8841_DRAG_TRAIN_VEHICLE_TO_HERE;
			w->widget[DEPOT_WIDGET_MATRIX].tooltips   = STR_883F_TRAINS_CLICK_ON_TRAIN_FOR;
			w->widget[DEPOT_WIDGET_BUILD].data        = STR_8815_NEW_VEHICLES;
			w->widget[DEPOT_WIDGET_BUILD].tooltips    = STR_8840_BUILD_NEW_TRAIN_VEHICLE;
			w->widget[DEPOT_WIDGET_CLONE].data        = STR_CLONE_TRAIN;
			w->widget[DEPOT_WIDGET_CLONE].tooltips    = STR_CLONE_TRAIN_DEPOT_INFO;
			w->widget[DEPOT_WIDGET_LOCATION].tooltips = STR_8842_CENTER_MAIN_VIEW_ON_TRAIN;
			break;

		case VEH_Road:
			w->widget[DEPOT_WIDGET_CAPTION].data      = STR_9003_ROAD_VEHICLE_DEPOT;
			w->widget[DEPOT_WIDGET_SELL].tooltips     = STR_9024_DRAG_ROAD_VEHICLE_TO_HERE;
			w->widget[DEPOT_WIDGET_MATRIX].tooltips   = STR_9022_VEHICLES_CLICK_ON_VEHICLE;
			w->widget[DEPOT_WIDGET_BUILD].data        = STR_9004_NEW_VEHICLES;
			w->widget[DEPOT_WIDGET_BUILD].tooltips    = STR_9023_BUILD_NEW_ROAD_VEHICLE;
			w->widget[DEPOT_WIDGET_CLONE].data        = STR_CLONE_ROAD_VEHICLE;
			w->widget[DEPOT_WIDGET_CLONE].tooltips    = STR_CLONE_ROAD_VEHICLE_DEPOT_INFO;
			w->widget[DEPOT_WIDGET_LOCATION].tooltips = STR_9025_CENTER_MAIN_VIEW_ON_ROAD;
			break;

		case VEH_Ship:
			w->widget[DEPOT_WIDGET_CAPTION].data      = STR_9803_SHIP_DEPOT;
			w->widget[DEPOT_WIDGET_SELL].tooltips     = STR_9821_DRAG_SHIP_TO_HERE_TO_SELL;
			w->widget[DEPOT_WIDGET_MATRIX].tooltips   = STR_981F_SHIPS_CLICK_ON_SHIP_FOR;
			w->widget[DEPOT_WIDGET_BUILD].data        = STR_9804_NEW_SHIPS;
			w->widget[DEPOT_WIDGET_BUILD].tooltips    = STR_9820_BUILD_NEW_SHIP;
			w->widget[DEPOT_WIDGET_CLONE].data        = STR_CLONE_SHIP;
			w->widget[DEPOT_WIDGET_CLONE].tooltips    = STR_CLONE_SHIP_DEPOT_INFO;
			w->widget[DEPOT_WIDGET_LOCATION].tooltips = STR_9822_CENTER_MAIN_VIEW_ON_SHIP;
			break;

		case VEH_Aircraft:
			w->widget[DEPOT_WIDGET_CAPTION].data      = STR_A002_AIRCRAFT_HANGAR;
			w->widget[DEPOT_WIDGET_SELL].tooltips     = STR_A023_DRAG_AIRCRAFT_TO_HERE_TO;
			w->widget[DEPOT_WIDGET_MATRIX].tooltips   = STR_A021_AIRCRAFT_CLICK_ON_AIRCRAFT;
			w->widget[DEPOT_WIDGET_BUILD].data        = STR_A003_NEW_AIRCRAFT;
			w->widget[DEPOT_WIDGET_BUILD].tooltips    = STR_A022_BUILD_NEW_AIRCRAFT;
			w->widget[DEPOT_WIDGET_CLONE].data        = STR_CLONE_AIRCRAFT;
			w->widget[DEPOT_WIDGET_CLONE].tooltips    = STR_CLONE_AIRCRAFT_INFO_HANGAR_WINDOW;
			w->widget[DEPOT_WIDGET_LOCATION].tooltips = STR_A024_CENTER_MAIN_VIEW_ON_HANGAR;

			/* Special strings only for hangars (using hangar instead of depot and so on) */
			w->widget[DEPOT_WIDGET_STOP_ALL].tooltips = STR_MASS_STOP_HANGAR_TOOLTIP;
			w->widget[DEPOT_WIDGET_START_ALL].tooltips=	STR_MASS_START_HANGAR_TOOLTIP;
			w->widget[DEPOT_WIDGET_SELL_ALL].tooltips =	STR_DEPOT_SELL_ALL_BUTTON_HANGAR_TIP;
			w->widget[DEPOT_WIDGET_AUTOREPLACE].tooltips = STR_DEPOT_AUTOREPLACE_HANGAR_TIP;
			break;
	}
}

/** Opens a depot window
 * @param tile The tile where the depot/hangar is located
 * @param type The type of vehicles in the depot
 */
void ShowDepotWindow(TileIndex tile, byte type)
{
	Window *w;

	/* First we ensure that the widget counts are equal in all 3 lists to prevent bad stuff from happening */
	assert(lengthof(widget_moves) == lengthof(_depot_widgets) - 1); // we should not count WIDGETS_END
	assert(lengthof(widget_moves) == DEPOT_WIDGET_LAST);

	switch (type) {
		case VEH_Train:    w = AllocateWindowDescFront(&_train_depot_desc, tile);    break;
		case VEH_Road:     w = AllocateWindowDescFront(&_road_depot_desc, tile);     break;
		case VEH_Ship:     w = AllocateWindowDescFront(&_ship_depot_desc, tile);     break;
		case VEH_Aircraft: w = AllocateWindowDescFront(&_aircraft_depot_desc, tile); break;
		default: NOT_REACHED(); w = NULL;
	}

	if (w != NULL) {
		int16 horizontal = 0, vertical = 0;
		w->caption_color = GetTileOwner(tile);
		WP(w, depot_d).type = type;
		WP(w, depot_d).sel = INVALID_VEHICLE;
		_backup_orders_tile = 0;

		/* Resize the window according to the vehicle type */
		switch (type) {
			case VEH_Train:
				horizontal = 56;
				vertical = 26;
				w->vscroll.cap = 6;
				w->hscroll.cap = 10 * 29;
				w->resize.step_width = 1;
				w->resize.step_height = 14;
				break;

			case VEH_Road:
				horizontal = 10;
				vertical = - 14;
				w->vscroll.cap = 4;
				w->hscroll.cap = 5;
				w->resize.step_width = 56;
				w->resize.step_height = 14;
				break;

			case VEH_Ship:
				w->vscroll.cap = 3;
				w->hscroll.cap = 3;
				w->resize.step_width = 90;
				w->resize.step_height = 23;
				break;

			case VEH_Aircraft:
				horizontal = 26;
				w->vscroll.cap = 3;
				w->hscroll.cap = 4;
				w->resize.step_width = 74;
				w->resize.step_height = 23;
				break;

			default: NOT_REACHED();
		}

		SetupStringsForDepotWindow(w, type);

		w->widget[DEPOT_WIDGET_MATRIX].data =
			(w->vscroll.cap * 0x100) // number of rows to draw on the background
			+ (type == VEH_Train ? 1 : w->hscroll.cap); // number of boxes in each row. Trains always have just one

		if (type != VEH_Train) {
			SETBIT(w->hidden_state, DEPOT_WIDGET_H_SCROLL);
			SETBIT(w->hidden_state, DEPOT_WIDGET_SELL_CHAIN);
		}

		/* Move the widgets to their right locations */
		ResizeWindowWidgets(w, widget_moves, lengthof(widget_moves), horizontal, vertical);

		if (type == VEH_Train) {
			/* Now we move the train only widgets so they are placed correctly
			 * Doing it here will ensure that they move if the widget they are placed on top of/aligned to are moved */

			/* DEPOT_WIDGET_H_SCROLL is placed in the lowest part of DEPOT_WIDGET_MATRIX */
			w->widget[DEPOT_WIDGET_H_SCROLL].left = w->widget[DEPOT_WIDGET_MATRIX].left;
			w->widget[DEPOT_WIDGET_H_SCROLL].right = w->widget[DEPOT_WIDGET_MATRIX].right;
			w->widget[DEPOT_WIDGET_H_SCROLL].bottom = w->widget[DEPOT_WIDGET_MATRIX].bottom;
			w->widget[DEPOT_WIDGET_H_SCROLL].top = w->widget[DEPOT_WIDGET_MATRIX].bottom - 11;
			w->widget[DEPOT_WIDGET_MATRIX].bottom -= 12;

			/* DEPOT_WIDGET_SELL_CHAIN is under DEPOT_WIDGET_SELL. They got the same left and right and height is controlled in ResizeDepotButtons() */
			w->widget[DEPOT_WIDGET_SELL_CHAIN].left = w->widget[DEPOT_WIDGET_SELL].left;
			w->widget[DEPOT_WIDGET_SELL_CHAIN].right = w->widget[DEPOT_WIDGET_SELL].right;
		}
		ResizeDepotButtons(w);
	}
}
