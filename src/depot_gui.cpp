/* $Id$ */

/** @file depot_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "train.h"
#include "roadveh.h"
#include "ship.h"
#include "aircraft.h"
#include "table/strings.h"
#include "strings.h"
#include "table/sprites.h"
#include "gui.h"
#include "textbuf_gui.h"
#include "gfx.h"
#include "vehicle.h"
#include "viewport.h"
#include "command.h"
#include "depot.h"
#include "vehicle_gui.h"
#include "station_map.h"
#include "newgrf_engine.h"
#include "spritecache.h"

/*
 * Since all depot window sizes aren't the same, we need to modify sizes a little.
 * It's done with the following arrays of widget indexes. Each of them tells if a widget side should be moved and in what direction.
 * How long they should be moved and for what window types are controlled in ShowDepotWindow()
 */

/* Names of the widgets. Keep them in the same order as in the widget array */
enum DepotWindowWidgets {
	DEPOT_WIDGET_CLOSEBOX = 0,
	DEPOT_WIDGET_CAPTION,
	DEPOT_WIDGET_STICKY,
	DEPOT_WIDGET_SELL,
	DEPOT_WIDGET_SELL_CHAIN,
	DEPOT_WIDGET_SELL_ALL,
	DEPOT_WIDGET_AUTOREPLACE,
	DEPOT_WIDGET_MATRIX,
	DEPOT_WIDGET_V_SCROLL, ///< Vertical scrollbar
	DEPOT_WIDGET_H_SCROLL, ///< Horizontal scrollbar
	DEPOT_WIDGET_BUILD,
	DEPOT_WIDGET_CLONE,
	DEPOT_WIDGET_LOCATION,
	DEPOT_WIDGET_VEHICLE_LIST,
	DEPOT_WIDGET_STOP_ALL,
	DEPOT_WIDGET_START_ALL,
	DEPOT_WIDGET_RESIZE,
};

/* Widget array for all depot windows.
 * If a widget is needed in some windows only (like train specific), add it for all windows
 * and use HideWindowWidget in ShowDepotWindow() to remove it in the windows where it should not be
 * Keep the widget numbers in sync with the enum or really bad stuff will happen!!! */

/* When adding widgets, place them as you would place them for the ship depot and define how you want it to move in widget_moves[]
 * If you want a widget for one window only, set it to be hidden in ShowDepotWindow() for the windows where you don't want it
 * NOTE: the train only widgets are moved/resized in ShowDepotWindow() so they follow certain other widgets if they are moved to ensure that they stick together.
 *    Changing the size of those here will not have an effect at all. It should be done in ShowDepotWindow()
 */

/*
 * Some of the widgets are placed outside the window (negative coordinates).
 * The reason is that they are placed relatively to the matrix and the matrix is just one pixel (in 0, 14).
 * The matrix and the rest of the window will be resized when the size of the boxes is set and then all the widgets will be inside the window.
 */
static const Widget _depot_widgets[] = {
	{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,            STR_018B_CLOSE_WINDOW},            // DEPOT_WIDGET_CLOSEBOX
	{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,    23,     0,    13, 0x0,                 STR_018C_WINDOW_TITLE_DRAG_THIS},  // DEPOT_WIDGET_CAPTION
	{  WWT_STICKYBOX,     RESIZE_LR,    14,    24,    35,     0,    13, 0x0,                 STR_STICKY_BUTTON},                // DEPOT_WIDGET_STICKY

	/* Widgets are set up run-time */
	{     WWT_IMGBTN,    RESIZE_LRB,    14,     1,    23,    14,   -32, 0x0,                 STR_NULL},                         // DEPOT_WIDGET_SELL
	{     WWT_IMGBTN,   RESIZE_LRTB,    14,     1,    23,   -55,   -32, SPR_SELL_CHAIN_TRAIN,STR_DRAG_WHOLE_TRAIN_TO_SELL_TIP}, // DEPOT_WIDGET_SELL_CHAIN, trains only
	{ WWT_PUSHIMGBTN,   RESIZE_LRTB,    14,     1,    23,   -31,    -9, 0x0,                 STR_NULL},                         // DEPOT_WIDGET_SELL_ALL
	{ WWT_PUSHIMGBTN,   RESIZE_LRTB,    14,     1,    23,    -8,    14, 0x0,                 STR_NULL},                         // DEPOT_WIDGET_AUTOREPLACE

	{     WWT_MATRIX,     RESIZE_RB,    14,     0,     0,    14,    14, 0x0,                 STR_NULL},                         // DEPOT_WIDGET_MATRIX
	{  WWT_SCROLLBAR,    RESIZE_LRB,    14,    24,    35,    14,    14, 0x0,                 STR_0190_SCROLL_BAR_SCROLLS_LIST}, // DEPOT_WIDGET_V_SCROLL

	{ WWT_HSCROLLBAR,    RESIZE_RTB,    14,     0,     0,     3,    14, 0x0,                 STR_HSCROLL_BAR_SCROLLS_LIST},     // DEPOT_WIDGET_H_SCROLL, trains only

	/* The buttons in the bottom of the window. left and right is not important as they are later resized to be equal in size
	 * This calculation is based on right in DEPOT_WIDGET_LOCATION and it presumes left of DEPOT_WIDGET_BUILD is 0            */
	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,     0,    15,    26, 0x0,                 STR_NULL},                         // DEPOT_WIDGET_BUILD
	{    WWT_TEXTBTN,     RESIZE_TB,    14,     0,     0,    15,    26, 0x0,                 STR_NULL},                         // DEPOT_WIDGET_CLONE
	{ WWT_PUSHTXTBTN,    RESIZE_RTB,    14,     0,   -12,    15,    26, STR_00E4_LOCATION,   STR_NULL},                         // DEPOT_WIDGET_LOCATION
	{ WWT_PUSHTXTBTN,   RESIZE_LRTB,    14,   -11,     0,    15,    26, 0x0,                 STR_NULL},                         // DEPOT_WIDGET_VEHICLE_LIST
	{ WWT_PUSHIMGBTN,   RESIZE_LRTB,    14,     1,    11,    15,    26, SPR_FLAG_VEH_STOPPED,STR_NULL},                         // DEPOT_WIDGET_STOP_ALL
	{ WWT_PUSHIMGBTN,   RESIZE_LRTB,    14,    12,    23,    15,    26, SPR_FLAG_VEH_RUNNING,STR_NULL},                         // DEPOT_WIDGET_START_ALL
	{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,    24,    35,    15,    26, 0x0,                 STR_RESIZE_BUTTON},                // DEPOT_WIDGET_RESIZE
	{   WIDGETS_END},
};

static void DepotWndProc(Window *w, WindowEvent *e);

static const WindowDesc _train_depot_desc = {
	WDP_AUTO, WDP_AUTO, 36, 27, 36, 27,
	WC_VEHICLE_DEPOT, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_depot_widgets,
	DepotWndProc
};

static const WindowDesc _road_depot_desc = {
	WDP_AUTO, WDP_AUTO, 36, 27, 36, 27,
	WC_VEHICLE_DEPOT, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_depot_widgets,
	DepotWndProc
};

static const WindowDesc _ship_depot_desc = {
	WDP_AUTO, WDP_AUTO, 36, 27, 36, 27,
	WC_VEHICLE_DEPOT, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_depot_widgets,
	DepotWndProc
};

static const WindowDesc _aircraft_depot_desc = {
	WDP_AUTO, WDP_AUTO, 36, 27, 36, 27,
	WC_VEHICLE_DEPOT, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_depot_widgets,
	DepotWndProc
};

extern int WagonLengthToPixels(int len);

/**
 * This is the Callback method after the cloning attempt of a vehicle
 * @param success indicates completion (or not) of the operation
 * @param tile unused
 * @param p1 unused
 * @param p2 unused
 */
void CcCloneVehicle(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (!success) return;

	Vehicle *v = GetVehicle(_new_vehicle_id);

	ShowVehicleViewWindow(v);
}

static void DepotSellAllConfirmationCallback(Window *w, bool confirmed)
{
	if (confirmed) {
		TileIndex tile = w->window_number;
		byte vehtype = WP(w, depot_d).type;
		DoCommandP(tile, vehtype, 0, NULL, CMD_DEPOT_SELL_ALL_VEHICLES);
	}
}

const Sprite *GetAircraftSprite(EngineID engine);

/** Draw a vehicle in the depot window in the box with the top left corner at x,y
 * @param *w Window to draw in
 * @param *v Vehicle to draw
 * @param x Left side of the box to draw in
 * @param y Top of the box to draw in
 */
static void DrawVehicleInDepot(Window *w, const Vehicle *v, int x, int y)
{
	byte diff_x = 0, diff_y = 0;

	int sprite_y = y + w->resize.step_height - GetVehicleListHeight(v->type);

	switch (v->type) {
		case VEH_TRAIN:
			DrawTrainImage(v, x + 21, sprite_y, w->hscroll.cap + 4, w->hscroll.pos, WP(w, depot_d).sel);

			/* Number of wagons relative to a standard length wagon (rounded up) */
			SetDParam(0, (v->u.rail.cached_total_length + 7) / 8);
			DrawStringRightAligned(w->widget[DEPOT_WIDGET_MATRIX].right - 1, y + 4, STR_TINY_BLACK, TC_FROMSTRING); // Draw the counter
			break;

		case VEH_ROAD:     DrawRoadVehImage( v, x + 24, sprite_y, 1, WP(w, depot_d).sel); break;
		case VEH_SHIP:     DrawShipImage(    v, x + 19, sprite_y - 1, WP(w, depot_d).sel); break;
		case VEH_AIRCRAFT: {
			const Sprite *spr = GetSprite(v->GetImage(DIR_W));
			DrawAircraftImage(v, x + 12,
							  y + max(spr->height + spr->y_offs - 14, 0), // tall sprites needs an y offset
							  WP(w, depot_d).sel);
		} break;
		default: NOT_REACHED();
	}

	if (w->resize.step_height == 14) {
		/* VEH_TRAIN and VEH_ROAD, which are low */
		diff_x = 15;
	} else {
		/* VEH_SHIP and VEH_AIRCRAFT, which are tall */
		diff_y = 12;
	}

	DrawSprite((v->vehstatus & VS_STOPPED) ? SPR_FLAG_VEH_STOPPED : SPR_FLAG_VEH_RUNNING, PAL_NONE, x + diff_x, y + diff_y);

	SetDParam(0, v->unitnumber);
	DrawString(x, y + 2, (uint16)(v->max_age-366) >= v->age ? STR_00E2 : STR_00E3, TC_FROMSTRING);
}

static void DrawDepotWindow(Window *w)
{
	Vehicle **vl = WP(w, depot_d).vehicle_list;
	TileIndex tile = w->window_number;
	int x, y, i, maxval;
	uint16 hnum;
	uint16 num = WP(w, depot_d).engine_count;

	/* Set the row and number of boxes in each row based on the number of boxes drawn in the matrix */
	uint16 rows_in_display   = w->widget[DEPOT_WIDGET_MATRIX].data >> 8;
	uint16 boxes_in_each_row = w->widget[DEPOT_WIDGET_MATRIX].data & 0xFF;

	/* setup disabled buttons */
	w->SetWidgetsDisabledState(!IsTileOwner(tile, _local_player),
		DEPOT_WIDGET_STOP_ALL,
		DEPOT_WIDGET_START_ALL,
		DEPOT_WIDGET_SELL,
		DEPOT_WIDGET_SELL_CHAIN,
		DEPOT_WIDGET_SELL_ALL,
		DEPOT_WIDGET_BUILD,
		DEPOT_WIDGET_CLONE,
		DEPOT_WIDGET_AUTOREPLACE,
		WIDGET_LIST_END);

	/* determine amount of items for scroller */
	if (WP(w, depot_d).type == VEH_TRAIN) {
		hnum = 8;
		for (num = 0; num < WP(w, depot_d).engine_count; num++) {
			const Vehicle *v = vl[num];
			hnum = max(hnum, v->u.rail.cached_total_length);
		}
		/* Always have 1 empty row, so people can change the setting of the train */
		SetVScrollCount(w, WP(w, depot_d).engine_count + WP(w, depot_d).wagon_count + 1);
		SetHScrollCount(w, WagonLengthToPixels(hnum));
	} else {
		SetVScrollCount(w, (num + w->hscroll.cap - 1) / w->hscroll.cap);
	}

	/* locate the depot struct */
	if (WP(w, depot_d).type == VEH_AIRCRAFT) {
		SetDParam(0, GetStationIndex(tile)); // Airport name
	} else {
		Depot *depot = GetDepotByTile(tile);
		assert(depot != NULL);

		SetDParam(0, depot->town_index);
	}

	DrawWindowWidgets(w);

	num = w->vscroll.pos * boxes_in_each_row;
	maxval = min(WP(w, depot_d).engine_count, num + (rows_in_display * boxes_in_each_row));

	for (x = 2, y = 15; num < maxval; y += w->resize.step_height, x = 2) { // Draw the rows
		byte i;

		for (i = 0; i < boxes_in_each_row && num < maxval; i++, num++, x += w->resize.step_width) {
			/* Draw all vehicles in the current row */
			const Vehicle *v = vl[num];
			DrawVehicleInDepot(w, v, x, y);
		}
	}

	maxval = min(WP(w, depot_d).engine_count + WP(w, depot_d).wagon_count, (w->vscroll.pos * boxes_in_each_row) + (rows_in_display * boxes_in_each_row));

	/* draw the train wagons, that do not have an engine in front */
	for (; num < maxval; num++, y += 14) {
		const Vehicle *v = WP(w, depot_d).wagon_list[num - WP(w, depot_d).engine_count];
		const Vehicle *u;

		DrawTrainImage(v, x + 50, y, w->hscroll.cap - 29, 0, WP(w, depot_d).sel);
		DrawString(x, y + 2, STR_8816, TC_FROMSTRING);

		/*Draw the train counter */
		i = 0;
		u = v;
		do i++; while ((u = u->Next()) != NULL); // Determine length of train
		SetDParam(0, i);                      // Set the counter
		DrawStringRightAligned(w->widget[DEPOT_WIDGET_MATRIX].right - 1, y + 4, STR_TINY_BLACK, TC_FROMSTRING); // Draw the counter
	}
}

struct GetDepotVehiclePtData {
	Vehicle *head;
	Vehicle *wagon;
};

enum DepotGUIAction {
	MODE_ERROR,
	MODE_DRAG_VEHICLE,
	MODE_SHOW_VEHICLE,
	MODE_START_STOP,
};

static DepotGUIAction GetVehicleFromDepotWndPt(const Window *w, int x, int y, Vehicle **veh, GetDepotVehiclePtData *d)
{
	Vehicle **vl = WP(w, depot_d).vehicle_list;
	uint xt, row, xm = 0, ym = 0;
	int pos, skip = 0;
	uint16 boxes_in_each_row = w->widget[DEPOT_WIDGET_MATRIX].data & 0xFF;

	if (WP(w, depot_d).type == VEH_TRAIN) {
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

	pos = ((row + w->vscroll.pos) * boxes_in_each_row) + xt;

	if (WP(w, depot_d).engine_count + WP(w, depot_d).wagon_count <= pos) {
		if (WP(w, depot_d).type == VEH_TRAIN) {
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
		case VEH_TRAIN: {
			Vehicle *v = *veh;
			d->head = d->wagon = v;

			/* either pressed the flag or the number, but only when it's a loco */
			if (x < 0 && IsFrontEngine(v)) return (x >= -10) ? MODE_START_STOP : MODE_SHOW_VEHICLE;

			skip = (skip * 8) / _traininfo_vehicle_width;
			x = (x * 8) / _traininfo_vehicle_width;

			/* Skip vehicles that are scrolled off the list */
			x += skip;

			/* find the vehicle in this row that was clicked */
			while (v != NULL && (x -= v->u.rail.cached_veh_length) >= 0) v = v->Next();

			/* if an articulated part was selected, find its parent */
			while (v != NULL && IsArticulatedPart(v)) v = v->Previous();

			d->wagon = v;

			return MODE_DRAG_VEHICLE;
			}
			break;

		case VEH_ROAD:
			if (xm >= 24) return MODE_DRAG_VEHICLE;
			if (xm <= 16) return MODE_SHOW_VEHICLE;
			break;

		case VEH_SHIP:
			if (xm >= 19) return MODE_DRAG_VEHICLE;
			if (ym <= 10) return MODE_SHOW_VEHICLE;
			break;

		case VEH_AIRCRAFT:
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
		wagon = wagon->Previous();
		if (wagon == NULL) return;
	}

	if (wagon == v) return;

	DoCommandP(v->tile, v->index + ((wagon == NULL ? INVALID_VEHICLE : wagon->index) << 16), _ctrl_pressed ? 1 : 0, NULL, CMD_MOVE_RAIL_VEHICLE | CMD_MSG(STR_8837_CAN_T_MOVE_VEHICLE));
}

static void DepotClick(Window *w, int x, int y)
{
	GetDepotVehiclePtData gdvp;
	Vehicle *v = NULL;
	DepotGUIAction mode = GetVehicleFromDepotWndPt(w, x, y, &v, &gdvp);

	/* share / copy orders */
	if (_thd.place_mode != VHM_NONE && mode != MODE_ERROR) {
		_place_clicked_vehicle = (WP(w, depot_d).type == VEH_TRAIN ? gdvp.head : v);
		return;
	}

	if (WP(w, depot_d).type == VEH_TRAIN) v = gdvp.wagon;

	switch (mode) {
		case MODE_ERROR: // invalid
			return;

		case MODE_DRAG_VEHICLE: { // start dragging of vehicle
			VehicleID sel = WP(w, depot_d).sel;

			if (WP(w, depot_d).type == VEH_TRAIN && sel != INVALID_VEHICLE) {
				WP(w, depot_d).sel = INVALID_VEHICLE;
				TrainDepotMoveVehicle(v, sel, gdvp.head);
			} else if (v != NULL) {
				int image = v->GetImage(DIR_W);

				WP(w, depot_d).sel = v->index;
				SetWindowDirty(w);
				SetObjectToPlaceWnd(image, GetVehiclePalette(v), VHM_DRAG, w);
			}
			}
			break;

		case MODE_SHOW_VEHICLE: // show info window
			ShowVehicleViewWindow(v);
			break;

		case MODE_START_STOP: { // click start/stop flag
			uint command;

			switch (WP(w, depot_d).type) {
				case VEH_TRAIN:    command = CMD_START_STOP_TRAIN | CMD_MSG(STR_883B_CAN_T_STOP_START_TRAIN);          break;
				case VEH_ROAD:     command = CMD_START_STOP_ROADVEH | CMD_MSG(STR_9015_CAN_T_STOP_START_ROAD_VEHICLE); break;
				case VEH_SHIP:     command = CMD_START_STOP_SHIP | CMD_MSG(STR_9818_CAN_T_STOP_START_SHIP);            break;
				case VEH_AIRCRAFT: command = CMD_START_STOP_AIRCRAFT | CMD_MSG(STR_A016_CAN_T_STOP_START_AIRCRAFT);    break;
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

	if (!v->IsPrimaryVehicle()) {
		v = v->First();
		/* Do nothing when clicking on a train in depot with no loc attached */
		if (v->type == VEH_TRAIN && !IsFrontEngine(v)) return;
	}

	switch (v->type) {
		case VEH_TRAIN:    error_str = CMD_MSG(STR_882B_CAN_T_BUILD_RAILROAD_VEHICLE); break;
		case VEH_ROAD:     error_str = CMD_MSG(STR_9009_CAN_T_BUILD_ROAD_VEHICLE);     break;
		case VEH_SHIP:     error_str = CMD_MSG(STR_980D_CAN_T_BUILD_SHIP);             break;
		case VEH_AIRCRAFT: error_str = CMD_MSG(STR_A008_CAN_T_BUILD_AIRCRAFT);         break;
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
	ResizeButtons(w, DEPOT_WIDGET_BUILD, DEPOT_WIDGET_LOCATION);

	if (WP(w, depot_d).type == VEH_TRAIN) {
		/* Divide the size of DEPOT_WIDGET_SELL into two equally big buttons so DEPOT_WIDGET_SELL and DEPOT_WIDGET_SELL_CHAIN will get the same size.
		* This way it will stay the same even if DEPOT_WIDGET_SELL_CHAIN is resized for some reason                                                  */
		w->widget[DEPOT_WIDGET_SELL_CHAIN].top    = ((w->widget[DEPOT_WIDGET_SELL_CHAIN].bottom - w->widget[DEPOT_WIDGET_SELL].top) / 2) + w->widget[DEPOT_WIDGET_SELL].top;
		w->widget[DEPOT_WIDGET_SELL].bottom     = w->widget[DEPOT_WIDGET_SELL_CHAIN].top - 1;
	}
}

/* Function to set up vehicle specific sprites and strings
 * Only use this if it's the same widget, that's used for more than one vehicle type and it needs different text/sprites
 * Vehicle specific text/sprites, that's in a widget, that's only shown for one vehicle type (like sell whole train) is set in the widget array
 */
static void SetupStringsForDepotWindow(Window *w, byte type)
{
	switch (type) {
		case VEH_TRAIN:
			w->widget[DEPOT_WIDGET_CAPTION].data      = STR_8800_TRAIN_DEPOT;
			w->widget[DEPOT_WIDGET_STOP_ALL].tooltips = STR_MASS_STOP_DEPOT_TRAIN_TIP;
			w->widget[DEPOT_WIDGET_START_ALL].tooltips= STR_MASS_START_DEPOT_TRAIN_TIP;
			w->widget[DEPOT_WIDGET_SELL].tooltips     = STR_8841_DRAG_TRAIN_VEHICLE_TO_HERE;
			w->widget[DEPOT_WIDGET_SELL_ALL].tooltips = STR_DEPOT_SELL_ALL_BUTTON_TRAIN_TIP;
			w->widget[DEPOT_WIDGET_MATRIX].tooltips   = STR_883F_TRAINS_CLICK_ON_TRAIN_FOR;

			w->widget[DEPOT_WIDGET_BUILD].data        = STR_8815_NEW_VEHICLES;
			w->widget[DEPOT_WIDGET_BUILD].tooltips    = STR_8840_BUILD_NEW_TRAIN_VEHICLE;
			w->widget[DEPOT_WIDGET_CLONE].data        = STR_CLONE_TRAIN;
			w->widget[DEPOT_WIDGET_CLONE].tooltips    = STR_CLONE_TRAIN_DEPOT_INFO;

			w->widget[DEPOT_WIDGET_LOCATION].tooltips = STR_8842_CENTER_MAIN_VIEW_ON_TRAIN;
			w->widget[DEPOT_WIDGET_VEHICLE_LIST].data = STR_TRAIN;
			w->widget[DEPOT_WIDGET_VEHICLE_LIST].tooltips = STR_DEPOT_VEHICLE_ORDER_LIST_TRAIN_TIP;
			w->widget[DEPOT_WIDGET_AUTOREPLACE].tooltips = STR_DEPOT_AUTOREPLACE_TRAIN_TIP;

			/* Sprites */
			w->widget[DEPOT_WIDGET_SELL].data        = SPR_SELL_TRAIN;
			w->widget[DEPOT_WIDGET_SELL_ALL].data    = SPR_SELL_ALL_TRAIN;
			w->widget[DEPOT_WIDGET_AUTOREPLACE].data = SPR_REPLACE_TRAIN;
			break;

		case VEH_ROAD:
			w->widget[DEPOT_WIDGET_CAPTION].data      = STR_9003_ROAD_VEHICLE_DEPOT;
			w->widget[DEPOT_WIDGET_STOP_ALL].tooltips = STR_MASS_STOP_DEPOT_ROADVEH_TIP;
			w->widget[DEPOT_WIDGET_START_ALL].tooltips= STR_MASS_START_DEPOT_ROADVEH_TIP;
			w->widget[DEPOT_WIDGET_SELL].tooltips     = STR_9024_DRAG_ROAD_VEHICLE_TO_HERE;
			w->widget[DEPOT_WIDGET_SELL_ALL].tooltips = STR_DEPOT_SELL_ALL_BUTTON_ROADVEH_TIP;
			w->widget[DEPOT_WIDGET_MATRIX].tooltips   = STR_9022_VEHICLES_CLICK_ON_VEHICLE;

			w->widget[DEPOT_WIDGET_BUILD].data        = STR_9004_NEW_VEHICLES;
			w->widget[DEPOT_WIDGET_BUILD].tooltips    = STR_9023_BUILD_NEW_ROAD_VEHICLE;
			w->widget[DEPOT_WIDGET_CLONE].data        = STR_CLONE_ROAD_VEHICLE;
			w->widget[DEPOT_WIDGET_CLONE].tooltips    = STR_CLONE_ROAD_VEHICLE_DEPOT_INFO;

			w->widget[DEPOT_WIDGET_LOCATION].tooltips = STR_9025_CENTER_MAIN_VIEW_ON_ROAD;
			w->widget[DEPOT_WIDGET_VEHICLE_LIST].data = STR_LORRY;
			w->widget[DEPOT_WIDGET_VEHICLE_LIST].tooltips = STR_DEPOT_VEHICLE_ORDER_LIST_ROADVEH_TIP;
			w->widget[DEPOT_WIDGET_AUTOREPLACE].tooltips = STR_DEPOT_AUTOREPLACE_ROADVEH_TIP;

			/* Sprites */
			w->widget[DEPOT_WIDGET_SELL].data        = SPR_SELL_ROADVEH;
			w->widget[DEPOT_WIDGET_SELL_ALL].data    = SPR_SELL_ALL_ROADVEH;
			w->widget[DEPOT_WIDGET_AUTOREPLACE].data = SPR_REPLACE_ROADVEH;
			break;

		case VEH_SHIP:
			w->widget[DEPOT_WIDGET_CAPTION].data      = STR_9803_SHIP_DEPOT;
			w->widget[DEPOT_WIDGET_STOP_ALL].tooltips = STR_MASS_STOP_DEPOT_SHIP_TIP;
			w->widget[DEPOT_WIDGET_START_ALL].tooltips= STR_MASS_START_DEPOT_SHIP_TIP;
			w->widget[DEPOT_WIDGET_SELL].tooltips     = STR_9821_DRAG_SHIP_TO_HERE_TO_SELL;
			w->widget[DEPOT_WIDGET_SELL_ALL].tooltips = STR_DEPOT_SELL_ALL_BUTTON_SHIP_TIP;
			w->widget[DEPOT_WIDGET_MATRIX].tooltips   = STR_981F_SHIPS_CLICK_ON_SHIP_FOR;

			w->widget[DEPOT_WIDGET_BUILD].data        = STR_9804_NEW_SHIPS;
			w->widget[DEPOT_WIDGET_BUILD].tooltips    = STR_9820_BUILD_NEW_SHIP;
			w->widget[DEPOT_WIDGET_CLONE].data        = STR_CLONE_SHIP;
			w->widget[DEPOT_WIDGET_CLONE].tooltips    = STR_CLONE_SHIP_DEPOT_INFO;

			w->widget[DEPOT_WIDGET_LOCATION].tooltips = STR_9822_CENTER_MAIN_VIEW_ON_SHIP;
			w->widget[DEPOT_WIDGET_VEHICLE_LIST].data = STR_SHIP;
			w->widget[DEPOT_WIDGET_VEHICLE_LIST].tooltips = STR_DEPOT_VEHICLE_ORDER_LIST_SHIP_TIP;
			w->widget[DEPOT_WIDGET_AUTOREPLACE].tooltips = STR_DEPOT_AUTOREPLACE_SHIP_TIP;

			/* Sprites */
			w->widget[DEPOT_WIDGET_SELL].data        = SPR_SELL_SHIP;
			w->widget[DEPOT_WIDGET_SELL_ALL].data    = SPR_SELL_ALL_SHIP;
			w->widget[DEPOT_WIDGET_AUTOREPLACE].data = SPR_REPLACE_SHIP;
			break;

		case VEH_AIRCRAFT:
			w->widget[DEPOT_WIDGET_CAPTION].data      = STR_A002_AIRCRAFT_HANGAR;
			w->widget[DEPOT_WIDGET_STOP_ALL].tooltips = STR_MASS_STOP_HANGAR_TIP;
			w->widget[DEPOT_WIDGET_START_ALL].tooltips= STR_MASS_START_HANGAR_TIP;
			w->widget[DEPOT_WIDGET_SELL].tooltips     = STR_A023_DRAG_AIRCRAFT_TO_HERE_TO;
			w->widget[DEPOT_WIDGET_SELL_ALL].tooltips = STR_DEPOT_SELL_ALL_BUTTON_AIRCRAFT_TIP;
			w->widget[DEPOT_WIDGET_MATRIX].tooltips   = STR_A021_AIRCRAFT_CLICK_ON_AIRCRAFT;

			w->widget[DEPOT_WIDGET_BUILD].data        = STR_A003_NEW_AIRCRAFT;
			w->widget[DEPOT_WIDGET_BUILD].tooltips    = STR_A022_BUILD_NEW_AIRCRAFT;
			w->widget[DEPOT_WIDGET_CLONE].data        = STR_CLONE_AIRCRAFT;
			w->widget[DEPOT_WIDGET_CLONE].tooltips    = STR_CLONE_AIRCRAFT_INFO_HANGAR_WINDOW;

			w->widget[DEPOT_WIDGET_LOCATION].tooltips = STR_A024_CENTER_MAIN_VIEW_ON_HANGAR;
			w->widget[DEPOT_WIDGET_VEHICLE_LIST].data = STR_PLANE;
			w->widget[DEPOT_WIDGET_VEHICLE_LIST].tooltips = STR_DEPOT_VEHICLE_ORDER_LIST_AIRCRAFT_TIP;
			w->widget[DEPOT_WIDGET_AUTOREPLACE].tooltips = STR_DEPOT_AUTOREPLACE_AIRCRAFT_TIP;

			/* Sprites */
			w->widget[DEPOT_WIDGET_SELL].data        = SPR_SELL_AIRCRAFT;
			w->widget[DEPOT_WIDGET_SELL_ALL].data    = SPR_SELL_ALL_AIRCRAFT;
			w->widget[DEPOT_WIDGET_AUTOREPLACE].data = SPR_REPLACE_AIRCRAFT;
			break;
	}
}


/* Array to hold the block sizes
 * First part is the vehicle type, while the last is 0 = x, 1 = y */
uint _block_sizes[4][2];

/* Array to hold the default resize capacities
* First part is the vehicle type, while the last is 0 = x, 1 = y */
const uint _resize_cap[][2] = {
/* VEH_TRAIN */    {6, 10 * 29},
/* VEH_ROAD */     {5, 5},
/* VEH_SHIP */     {3, 3},
/* VEH_AIRCRAFT */ {3, 4},
};

static void ResizeDefaultWindowSizeForTrains()
{
	_block_sizes[VEH_TRAIN][0] = 1;
	_block_sizes[VEH_TRAIN][1] = GetVehicleListHeight(VEH_TRAIN);
}

static void ResizeDefaultWindowSizeForRoadVehicles()
{
	_block_sizes[VEH_ROAD][0] = 56;
	_block_sizes[VEH_ROAD][1] = GetVehicleListHeight(VEH_ROAD);
}

static void ResizeDefaultWindowSize(VehicleType type)
{
	EngineID engine;
	uint max_width  = 0;
	uint max_height = 0;

	FOR_ALL_ENGINEIDS_OF_TYPE(engine, type) {
		uint x, y;

		switch (type) {
			default: NOT_REACHED();
			case VEH_SHIP:     GetShipSpriteSize(    engine, x, y); break;
			case VEH_AIRCRAFT: GetAircraftSpriteSize(engine, x, y); break;
		}
		if (x > max_width)  max_width  = x;
		if (y > max_height) max_height = y;
	}

	switch (type) {
		default: NOT_REACHED();
		case VEH_SHIP:
			_block_sizes[VEH_SHIP][0] = max(90U, max_width + 20); // we need 20 pixels from the right edge to the sprite
			break;
		case VEH_AIRCRAFT:
			_block_sizes[VEH_AIRCRAFT][0] = max(74U, max_width);
			break;
	}
	_block_sizes[type][1] = max(GetVehicleListHeight(type), max_height);
}

/* Set the size of the blocks in the window so we can be sure that they are big enough for the vehicle sprites in the current game
 * We will only need to call this once for each game */
void InitDepotWindowBlockSizes()
{
	ResizeDefaultWindowSizeForTrains();
	ResizeDefaultWindowSizeForRoadVehicles();
	ResizeDefaultWindowSize(VEH_SHIP);
	ResizeDefaultWindowSize(VEH_AIRCRAFT);
}

static void CreateDepotListWindow(Window *w, VehicleType type)
{
	WP(w, depot_d).type = type;
	_backup_orders_tile = 0;

	assert(IsPlayerBuildableVehicleType(type)); // ensure that we make the call with a valid type

	/* Resize the window according to the vehicle type */

	/* Set the number of blocks in each direction */
	w->vscroll.cap = _resize_cap[type][0];
	w->hscroll.cap = _resize_cap[type][1];

	/* Set the block size */
	w->resize.step_width  = _block_sizes[type][0];
	w->resize.step_height = _block_sizes[type][1];

	/* Enlarge the window to fit with the selected number of blocks of the selected size */
	ResizeWindow(w,
				 _block_sizes[type][0] * w->hscroll.cap,
				 _block_sizes[type][1] * w->vscroll.cap);

	if (type == VEH_TRAIN) {
		/* Make space for the horizontal scrollbar vertically, and the unit
		 * number, flag, and length counter horizontally. */
		ResizeWindow(w, 36, 12);
		/* substract the newly added space from the matrix since it was meant for the scrollbar */
		w->widget[DEPOT_WIDGET_MATRIX].bottom -= 12;
	}

	/* Set the minimum window size to the current window size */
	w->resize.width  = w->width;
	w->resize.height = w->height;

	SetupStringsForDepotWindow(w, type);

	w->widget[DEPOT_WIDGET_MATRIX].data =
		(w->vscroll.cap * 0x100) // number of rows to draw on the background
		+ (type == VEH_TRAIN ? 1 : w->hscroll.cap); // number of boxes in each row. Trains always have just one


	w->SetWidgetsHiddenState(type != VEH_TRAIN,
		DEPOT_WIDGET_H_SCROLL,
		DEPOT_WIDGET_SELL_CHAIN,
		WIDGET_LIST_END);

	ResizeDepotButtons(w);
}

void DepotSortList(Vehicle **v, uint16 length);

static void DepotWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_CREATE:
			WP(w, depot_d).sel = INVALID_VEHICLE;
			WP(w, depot_d).vehicle_list  = NULL;
			WP(w, depot_d).wagon_list    = NULL;
			WP(w, depot_d).engine_count  = 0;
			WP(w, depot_d).wagon_count   = 0;
			WP(w, depot_d).generate_list = true;
			break;

		case WE_INVALIDATE_DATA:
			WP(w, depot_d).generate_list = true;
			break;

		case WE_PAINT:
			if (WP(w, depot_d).generate_list) {
				/* Generate the vehicle list
				 * It's ok to use the wagon pointers for non-trains as they will be ignored */
				BuildDepotVehicleList(WP(w, depot_d).type, w->window_number,
					&WP(w, depot_d).vehicle_list, &WP(w, depot_d).engine_list_length, &WP(w, depot_d).engine_count,
					&WP(w, depot_d).wagon_list,   &WP(w, depot_d).wagon_list_length,  &WP(w, depot_d).wagon_count);
				WP(w, depot_d).generate_list = false;
				DepotSortList(WP(w, depot_d).vehicle_list, WP(w, depot_d).engine_count);
//#ifndef NDEBUG
#if 0
/* We disabled this check for now, but will keep it to quickly make this test again later (if we change some code) */
			} else {
				/* Here we got a piece of code, that only checks if we got a different number of vehicles in the depot list and the number of vehicles actually being in the depot.
				 * IF they aren't the same, then WE_INVALIDATE_DATA should have been called somewhere, but it wasn't and we got a bug
				 * Since this is a time consuming check and not nice to memory fragmentation, it may not stay for long, but it's a good way to check this
				 * We can turn it on/off by switching between #ifndef NDEBUG and #if 0 */
				Vehicle **engines = NULL, **wagons = NULL;
				uint16 engine_count = 0, engine_length = 0;
				uint16 wagon_count  = 0, wagon_length  = 0;
				BuildDepotVehicleList(WP(w, depot_d).type, w->window_number, &engines, &engine_length, &engine_count,
									  &wagons,  &wagon_length,  &wagon_count);

				assert(engine_count == WP(w, depot_d).engine_count);
				assert(wagon_count == WP(w, depot_d).wagon_count);
				free((void*)engines);
				free((void*)wagons);
#endif
			}
			DrawDepotWindow(w);
			break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case DEPOT_WIDGET_MATRIX: // List
					DepotClick(w, e->we.click.pt.x, e->we.click.pt.y);
					break;

				case DEPOT_WIDGET_BUILD: // Build vehicle
					ResetObjectToPlace();
					ShowBuildVehicleWindow(w->window_number, WP(w, depot_d).type);
					break;

				case DEPOT_WIDGET_CLONE: // Clone button
					w->InvalidateWidget(DEPOT_WIDGET_CLONE);
					w->ToggleWidgetLoweredState(DEPOT_WIDGET_CLONE);

					if (w->IsWidgetLowered(DEPOT_WIDGET_CLONE)) {
						static const CursorID clone_icons[] = {
							SPR_CURSOR_CLONE_TRAIN, SPR_CURSOR_CLONE_ROADVEH,
							SPR_CURSOR_CLONE_SHIP, SPR_CURSOR_CLONE_AIRPLANE
						};

						_place_clicked_vehicle = NULL;
						SetObjectToPlaceWnd(clone_icons[WP(w, depot_d).type], PAL_NONE, VHM_RECT, w);
					} else {
						ResetObjectToPlace();
					}
						break;

				case DEPOT_WIDGET_LOCATION: ScrollMainWindowToTile(w->window_number); break;

				case DEPOT_WIDGET_STOP_ALL:
				case DEPOT_WIDGET_START_ALL:
					DoCommandP(w->window_number, 0, WP(w, depot_d).type | (e->we.click.widget == DEPOT_WIDGET_START_ALL ? (1 << 5) : 0), NULL, CMD_MASS_START_STOP);
					break;

				case DEPOT_WIDGET_SELL_ALL:
					/* Only open the confimation window if there are anything to sell */
					if (WP(w, depot_d).engine_count != 0 || WP(w, depot_d).wagon_count != 0) {
						static const StringID confirm_captions[] = {
							STR_8800_TRAIN_DEPOT,
							STR_9003_ROAD_VEHICLE_DEPOT,
							STR_9803_SHIP_DEPOT,
							STR_A002_AIRCRAFT_HANGAR
						};
						TileIndex tile = w->window_number;
						byte vehtype = WP(w, depot_d).type;

						SetDParam(0, (vehtype == VEH_AIRCRAFT) ? GetStationIndex(tile) : GetDepotByTile(tile)->town_index);
						ShowQuery(
							confirm_captions[vehtype],
							STR_DEPOT_SELL_CONFIRMATION_TEXT,
							w,
							DepotSellAllConfirmationCallback
						);
					}
					break;

				case DEPOT_WIDGET_VEHICLE_LIST:
					ShowVehicleListWindow(GetTileOwner(w->window_number), WP(w, depot_d).type, (TileIndex)w->window_number);
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
			w->RaiseWidget(DEPOT_WIDGET_CLONE);
			w->InvalidateWidget(DEPOT_WIDGET_CLONE);
		} break;

			/* check if a vehicle in a depot was clicked.. */
		case WE_MOUSELOOP: {
			const Vehicle *v = _place_clicked_vehicle;

			/* since OTTD checks all open depot windows, we will make sure that it triggers the one with a clicked clone button */
			if (v != NULL && w->IsWidgetLowered(DEPOT_WIDGET_CLONE)) {
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

					if (WP(w, depot_d).type == VEH_TRAIN) {
						GetDepotVehiclePtData gdvp;

						if (GetVehicleFromDepotWndPt(w, e->we.dragdrop.pt.x, e->we.dragdrop.pt.y, &v, &gdvp) == MODE_DRAG_VEHICLE &&
							sel != INVALID_VEHICLE) {
							if (gdvp.wagon != NULL && gdvp.wagon->index == sel && _ctrl_pressed) {
								DoCommandP(GetVehicle(sel)->tile, GetVehicle(sel)->index, true, NULL, CMD_REVERSE_TRAIN_DIRECTION | CMD_MSG(STR_9033_CAN_T_MAKE_VEHICLE_TURN));
							} else if (gdvp.wagon == NULL || gdvp.wagon->index != sel) {
								TrainDepotMoveVehicle(gdvp.wagon, sel, gdvp.head);
							} else if (gdvp.head != NULL && IsFrontEngine(gdvp.head)) {
								ShowVehicleViewWindow(gdvp.head);
							}
						}
					} else if (GetVehicleFromDepotWndPt(w, e->we.dragdrop.pt.x, e->we.dragdrop.pt.y, &v, NULL) == MODE_DRAG_VEHICLE &&
						v != NULL &&
						sel == v->index) {
						ShowVehicleViewWindow(v);
					}
				} break;

				case DEPOT_WIDGET_SELL: case DEPOT_WIDGET_SELL_CHAIN:
					if (!w->IsWidgetDisabled(DEPOT_WIDGET_SELL) &&
						WP(w, depot_d).sel != INVALID_VEHICLE) {
						Vehicle *v;
						uint command;
						int sell_cmd;
						bool is_engine;

						if (w->IsWidgetDisabled(e->we.click.widget)) return;
						if (WP(w, depot_d).sel == INVALID_VEHICLE) return;

						w->HandleButtonClick(e->we.click.widget);

						v = GetVehicle(WP(w, depot_d).sel);
						WP(w, depot_d).sel = INVALID_VEHICLE;
						SetWindowDirty(w);

						sell_cmd = (v->type == VEH_TRAIN && (e->we.click.widget == DEPOT_WIDGET_SELL_CHAIN || _ctrl_pressed)) ? 1 : 0;

						is_engine = (!(v->type == VEH_TRAIN && !IsFrontEngine(v)));

						if (is_engine) {
							_backup_orders_tile = v->tile;
							BackupVehicleOrders(v);
						}

						switch (v->type) {
							case VEH_TRAIN:    command = CMD_SELL_RAIL_WAGON | CMD_MSG(STR_8839_CAN_T_SELL_RAILROAD_VEHICLE); break;
							case VEH_ROAD:     command = CMD_SELL_ROAD_VEH | CMD_MSG(STR_9014_CAN_T_SELL_ROAD_VEHICLE);       break;
							case VEH_SHIP:     command = CMD_SELL_SHIP | CMD_MSG(STR_980C_CAN_T_SELL_SHIP);                   break;
							case VEH_AIRCRAFT: command = CMD_SELL_AIRCRAFT | CMD_MSG(STR_A01C_CAN_T_SELL_AIRCRAFT);           break;
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
			w->widget[DEPOT_WIDGET_MATRIX].data = (w->vscroll.cap << 8) + (WP(w, depot_d).type == VEH_TRAIN ? 1 : w->hscroll.cap);
			ResizeDepotButtons(w);
			break;
	}
}

/** Opens a depot window
 * @param tile The tile where the depot/hangar is located
 * @param type The type of vehicles in the depot
 */
void ShowDepotWindow(TileIndex tile, VehicleType type)
{
	Window *w;

	switch (type) {
		default: NOT_REACHED();
		case VEH_TRAIN:
			w = AllocateWindowDescFront(&_train_depot_desc, tile); break;
		case VEH_ROAD:
			w = AllocateWindowDescFront(&_road_depot_desc, tile); break;
		case VEH_SHIP:
			w = AllocateWindowDescFront(&_ship_depot_desc, tile); break;
		case VEH_AIRCRAFT:
			w = AllocateWindowDescFront(&_aircraft_depot_desc, tile); break;
	}

	if (w != NULL) {
		w->caption_color = GetTileOwner(tile);
		CreateDepotListWindow(w, type);
	}
}

/** Removes the highlight of a vehicle in a depot window
 * @param *v Vehicle to remove all highlights from
 */
void DeleteDepotHighlightOfVehicle(const Vehicle *v)
{
	Window *w;

	/* If we haven't got any vehicles on the mouse pointer, we haven't got any highlighted in any depots either
	 * If that is the case, we can skip looping though the windows and save time                                */
	if (_special_mouse_mode != WSM_DRAGDROP) return;

	w = FindWindowById(WC_VEHICLE_DEPOT, v->tile);
	if (w != NULL) {
		WP(w, depot_d).sel = INVALID_VEHICLE;
		ResetObjectToPlace();
	}
}
