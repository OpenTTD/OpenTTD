/* $Id$ */

/** @file depot_gui.cpp The GUI for depots. */

#include "stdafx.h"
#include "openttd.h"
#include "train.h"
#include "roadveh.h"
#include "ship.h"
#include "aircraft.h"
#include "gui.h"
#include "textbuf_gui.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "command_func.h"
#include "depot_base.h"
#include "vehicle_gui.h"
#include "station_map.h"
#include "newgrf_engine.h"
#include "spritecache.h"
#include "strings_func.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "player_func.h"
#include "order_func.h"
#include "depot_base.h"
#include "tilehighlight_func.h"

#include "table/strings.h"
#include "table/sprites.h"

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


static const WindowDesc _train_depot_desc = {
	WDP_AUTO, WDP_AUTO, 36, 27, 36, 27,
	WC_VEHICLE_DEPOT, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_depot_widgets,
	NULL
};

static const WindowDesc _road_depot_desc = {
	WDP_AUTO, WDP_AUTO, 36, 27, 36, 27,
	WC_VEHICLE_DEPOT, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_depot_widgets,
	NULL
};

static const WindowDesc _ship_depot_desc = {
	WDP_AUTO, WDP_AUTO, 36, 27, 36, 27,
	WC_VEHICLE_DEPOT, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_depot_widgets,
	NULL
};

static const WindowDesc _aircraft_depot_desc = {
	WDP_AUTO, WDP_AUTO, 36, 27, 36, 27,
	WC_VEHICLE_DEPOT, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_depot_widgets,
	NULL
};

extern int WagonLengthToPixels(int len);
extern void DepotSortList(Vehicle **v, uint16 length);

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
	uint max_width  = 0;
	uint max_height = 0;

	const Engine *e;
	FOR_ALL_ENGINES_OF_TYPE(e, type) {
		EngineID eid = e->index;
		uint x, y;

		switch (type) {
			default: NOT_REACHED();
			case VEH_SHIP:     GetShipSpriteSize(    eid, x, y); break;
			case VEH_AIRCRAFT: GetAircraftSpriteSize(eid, x, y); break;
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

static void DepotSellAllConfirmationCallback(Window *w, bool confirmed);
const Sprite *GetAircraftSprite(EngineID engine);

struct DepotWindow : Window {
	VehicleID sel;
	VehicleType type;
	bool generate_list;
	uint16 engine_list_length;
	uint16 wagon_list_length;
	uint16 engine_count;
	uint16 wagon_count;
	Vehicle **vehicle_list;
	Vehicle **wagon_list;

	DepotWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->sel = INVALID_VEHICLE;
		this->vehicle_list  = NULL;
		this->wagon_list    = NULL;
		this->engine_count  = 0;
		this->wagon_count   = 0;
		this->generate_list = true;

		this->FindWindowPlacementAndResize(desc);
	}

	~DepotWindow()
	{
		DeleteWindowById(WC_BUILD_VEHICLE, this->window_number);
		free((void*)this->vehicle_list);
		free((void*)this->wagon_list);
	}

	/** Draw a vehicle in the depot window in the box with the top left corner at x,y
	* @param *w Window to draw in
	* @param *v Vehicle to draw
	* @param x Left side of the box to draw in
	* @param y Top of the box to draw in
	*/
	void DrawVehicleInDepot(Window *w, const Vehicle *v, int x, int y)
	{
		byte diff_x = 0, diff_y = 0;

		int sprite_y = y + this->resize.step_height - GetVehicleListHeight(v->type);

		switch (v->type) {
			case VEH_TRAIN:
				DrawTrainImage(v, x + 21, sprite_y, this->sel, this->hscroll.cap + 4, this->hscroll.pos);

				/* Number of wagons relative to a standard length wagon (rounded up) */
				SetDParam(0, (v->u.rail.cached_total_length + 7) / 8);
				DrawStringRightAligned(this->widget[DEPOT_WIDGET_MATRIX].right - 1, y + 4, STR_TINY_BLACK, TC_FROMSTRING); // Draw the counter
				break;

			case VEH_ROAD:     DrawRoadVehImage( v, x + 24, sprite_y, this->sel, 1); break;
			case VEH_SHIP:     DrawShipImage(    v, x + 19, sprite_y - 1, this->sel); break;
			case VEH_AIRCRAFT: {
				const Sprite *spr = GetSprite(v->GetImage(DIR_W));
				DrawAircraftImage(v, x + 12,
									y + max(spr->height + spr->y_offs - 14, 0), // tall sprites needs an y offset
									this->sel);
			} break;
			default: NOT_REACHED();
		}

		if (this->resize.step_height == 14) {
			/* VEH_TRAIN and VEH_ROAD, which are low */
			diff_x = 15;
		} else {
			/* VEH_SHIP and VEH_AIRCRAFT, which are tall */
			diff_y = 12;
		}

		DrawSprite((v->vehstatus & VS_STOPPED) ? SPR_FLAG_VEH_STOPPED : SPR_FLAG_VEH_RUNNING, PAL_NONE, x + diff_x, y + diff_y);

		SetDParam(0, v->unitnumber);
		DrawString(x, y + 2, (uint16)(v->max_age - 366) >= v->age ? STR_00E2 : STR_00E3, TC_FROMSTRING);
	}

	void DrawDepotWindow(Window *w)
	{
		Vehicle **vl = this->vehicle_list;
		TileIndex tile = this->window_number;
		int x, y, i, maxval;
		uint16 hnum;
		uint16 num = this->engine_count;

		/* Set the row and number of boxes in each row based on the number of boxes drawn in the matrix */
		uint16 rows_in_display   = this->widget[DEPOT_WIDGET_MATRIX].data >> 8;
		uint16 boxes_in_each_row = this->widget[DEPOT_WIDGET_MATRIX].data & 0xFF;

		/* setup disabled buttons */
		this->SetWidgetsDisabledState(!IsTileOwner(tile, _local_player),
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
		if (this->type == VEH_TRAIN) {
			hnum = 8;
			for (num = 0; num < this->engine_count; num++) {
				const Vehicle *v = vl[num];
				hnum = max(hnum, v->u.rail.cached_total_length);
			}
			/* Always have 1 empty row, so people can change the setting of the train */
			SetVScrollCount(w, this->engine_count + this->wagon_count + 1);
			SetHScrollCount(w, WagonLengthToPixels(hnum));
		} else {
			SetVScrollCount(w, (num + this->hscroll.cap - 1) / this->hscroll.cap);
		}

		/* locate the depot struct */
		if (this->type == VEH_AIRCRAFT) {
			SetDParam(0, GetStationIndex(tile)); // Airport name
		} else {
			Depot *depot = GetDepotByTile(tile);
			assert(depot != NULL);

			SetDParam(0, depot->town_index);
		}

		DrawWindowWidgets(w);

		num = this->vscroll.pos * boxes_in_each_row;
		maxval = min(this->engine_count, num + (rows_in_display * boxes_in_each_row));

		for (x = 2, y = 15; num < maxval; y += this->resize.step_height, x = 2) { // Draw the rows
			byte i;

			for (i = 0; i < boxes_in_each_row && num < maxval; i++, num++, x += this->resize.step_width) {
				/* Draw all vehicles in the current row */
				const Vehicle *v = vl[num];
				DrawVehicleInDepot(w, v, x, y);
			}
		}

		maxval = min(this->engine_count + this->wagon_count, (this->vscroll.pos * boxes_in_each_row) + (rows_in_display * boxes_in_each_row));

		/* draw the train wagons, that do not have an engine in front */
		for (; num < maxval; num++, y += 14) {
			const Vehicle *v = this->wagon_list[num - this->engine_count];
			const Vehicle *u;

			DrawTrainImage(v, x + 50, y, this->sel, this->hscroll.cap - 29, 0);
			DrawString(x, y + 2, STR_8816, TC_FROMSTRING);

			/*Draw the train counter */
			i = 0;
			u = v;
			do i++; while ((u = u->Next()) != NULL); // Determine length of train
			SetDParam(0, i);                      // Set the counter
			DrawStringRightAligned(this->widget[DEPOT_WIDGET_MATRIX].right - 1, y + 4, STR_TINY_BLACK, TC_FROMSTRING); // Draw the counter
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

	DepotGUIAction GetVehicleFromDepotWndPt(int x, int y, Vehicle **veh, GetDepotVehiclePtData *d) const
	{
		Vehicle **vl = this->vehicle_list;
		uint xt, row, xm = 0, ym = 0;
		int pos, skip = 0;
		uint16 boxes_in_each_row = this->widget[DEPOT_WIDGET_MATRIX].data & 0xFF;

		if (this->type == VEH_TRAIN) {
			xt = 0;
			x -= 23;
		} else {
			xt = x / this->resize.step_width;
			xm = x % this->resize.step_width;
			if (xt >= this->hscroll.cap) return MODE_ERROR;

			ym = (y - 14) % this->resize.step_height;
		}

		row = (y - 14) / this->resize.step_height;
		if (row >= this->vscroll.cap) return MODE_ERROR;

		pos = ((row + this->vscroll.pos) * boxes_in_each_row) + xt;

		if (this->engine_count + this->wagon_count <= pos) {
			if (this->type == VEH_TRAIN) {
				d->head  = NULL;
				d->wagon = NULL;
				return MODE_DRAG_VEHICLE;
			} else {
				return MODE_ERROR; // empty block, so no vehicle is selected
			}
		}

		if (this->engine_count > pos) {
			*veh = vl[pos];
			skip = this->hscroll.pos;
		} else {
			vl = this->wagon_list;
			pos -= this->engine_count;
			*veh = vl[pos];
			/* free wagons don't have an initial loco. */
			x -= _traininfo_vehicle_width;
		}

		switch (this->type) {
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

	void DepotClick(int x, int y)
	{
		GetDepotVehiclePtData gdvp = { NULL, NULL };
		Vehicle *v = NULL;
		DepotGUIAction mode = this->GetVehicleFromDepotWndPt(x, y, &v, &gdvp);

		/* share / copy orders */
		if (_thd.place_mode != VHM_NONE && mode != MODE_ERROR) {
			_place_clicked_vehicle = (this->type == VEH_TRAIN ? gdvp.head : v);
			return;
		}

		if (this->type == VEH_TRAIN) v = gdvp.wagon;

		switch (mode) {
			case MODE_ERROR: // invalid
				return;

			case MODE_DRAG_VEHICLE: { // start dragging of vehicle
				VehicleID sel = this->sel;

				if (this->type == VEH_TRAIN && sel != INVALID_VEHICLE) {
					this->sel = INVALID_VEHICLE;
					TrainDepotMoveVehicle(v, sel, gdvp.head);
				} else if (v != NULL) {
					int image = v->GetImage(DIR_W);

					this->sel = v->index;
					this->SetDirty();
					SetObjectToPlaceWnd(image, GetVehiclePalette(v), VHM_DRAG, this);
					_cursor.vehchain = _ctrl_pressed;
				}
			} break;

			case MODE_SHOW_VEHICLE: // show info window
				ShowVehicleViewWindow(v);
				break;

			case MODE_START_STOP: { // click start/stop flag
				uint command;

				switch (this->type) {
					case VEH_TRAIN:    command = CMD_START_STOP_TRAIN | CMD_MSG(STR_883B_CAN_T_STOP_START_TRAIN);          break;
					case VEH_ROAD:     command = CMD_START_STOP_ROADVEH | CMD_MSG(STR_9015_CAN_T_STOP_START_ROAD_VEHICLE); break;
					case VEH_SHIP:     command = CMD_START_STOP_SHIP | CMD_MSG(STR_9818_CAN_T_STOP_START_SHIP);            break;
					case VEH_AIRCRAFT: command = CMD_START_STOP_AIRCRAFT | CMD_MSG(STR_A016_CAN_T_STOP_START_AIRCRAFT);    break;
					default: NOT_REACHED(); command = 0;
				}
				DoCommandP(v->tile, v->index, 0, NULL, command);
			} break;

			default: NOT_REACHED();
		}
	}

	/**
	* Clones a vehicle
	* @param *v is the original vehicle to clone
	* @param *w is the window of the depot where the clone is build
	*/
	void HandleCloneVehClick(const Vehicle *v, const Window *w)
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

		DoCommandP(this->window_number, v->index, _ctrl_pressed ? 1 : 0, CcCloneVehicle, CMD_CLONE_VEHICLE | error_str);

		ResetObjectToPlace();
	}

	void ResizeDepotButtons(Window *w)
	{
		ResizeButtons(w, DEPOT_WIDGET_BUILD, DEPOT_WIDGET_LOCATION);

		if (this->type == VEH_TRAIN) {
			/* Divide the size of DEPOT_WIDGET_SELL into two equally big buttons so DEPOT_WIDGET_SELL and DEPOT_WIDGET_SELL_CHAIN will get the same size.
			* This way it will stay the same even if DEPOT_WIDGET_SELL_CHAIN is resized for some reason                                                  */
			this->widget[DEPOT_WIDGET_SELL_CHAIN].top    = ((this->widget[DEPOT_WIDGET_SELL_CHAIN].bottom - this->widget[DEPOT_WIDGET_SELL].top) / 2) + this->widget[DEPOT_WIDGET_SELL].top;
			this->widget[DEPOT_WIDGET_SELL].bottom     = this->widget[DEPOT_WIDGET_SELL_CHAIN].top - 1;
		}
	}

	/* Function to set up vehicle specific sprites and strings
	 * Only use this if it's the same widget, that's used for more than one vehicle type and it needs different text/sprites
	 * Vehicle specific text/sprites, that's in a widget, that's only shown for one vehicle type (like sell whole train) is set in the widget array
	 */
	void SetupStringsForDepotWindow(VehicleType type)
	{
		switch (type) {
			default: NOT_REACHED();

			case VEH_TRAIN:
				this->widget[DEPOT_WIDGET_CAPTION].data      = STR_8800_TRAIN_DEPOT;
				this->widget[DEPOT_WIDGET_STOP_ALL].tooltips = STR_MASS_STOP_DEPOT_TRAIN_TIP;
				this->widget[DEPOT_WIDGET_START_ALL].tooltips= STR_MASS_START_DEPOT_TRAIN_TIP;
				this->widget[DEPOT_WIDGET_SELL].tooltips     = STR_8841_DRAG_TRAIN_VEHICLE_TO_HERE;
				this->widget[DEPOT_WIDGET_SELL_ALL].tooltips = STR_DEPOT_SELL_ALL_BUTTON_TRAIN_TIP;
				this->widget[DEPOT_WIDGET_MATRIX].tooltips   = STR_883F_TRAINS_CLICK_ON_TRAIN_FOR;

				this->widget[DEPOT_WIDGET_BUILD].data        = STR_8815_NEW_VEHICLES;
				this->widget[DEPOT_WIDGET_BUILD].tooltips    = STR_8840_BUILD_NEW_TRAIN_VEHICLE;
				this->widget[DEPOT_WIDGET_CLONE].data        = STR_CLONE_TRAIN;
				this->widget[DEPOT_WIDGET_CLONE].tooltips    = STR_CLONE_TRAIN_DEPOT_INFO;

				this->widget[DEPOT_WIDGET_LOCATION].tooltips = STR_8842_CENTER_MAIN_VIEW_ON_TRAIN;
				this->widget[DEPOT_WIDGET_VEHICLE_LIST].data = STR_TRAIN;
				this->widget[DEPOT_WIDGET_VEHICLE_LIST].tooltips = STR_DEPOT_VEHICLE_ORDER_LIST_TRAIN_TIP;
				this->widget[DEPOT_WIDGET_AUTOREPLACE].tooltips = STR_DEPOT_AUTOREPLACE_TRAIN_TIP;

				/* Sprites */
				this->widget[DEPOT_WIDGET_SELL].data        = SPR_SELL_TRAIN;
				this->widget[DEPOT_WIDGET_SELL_ALL].data    = SPR_SELL_ALL_TRAIN;
				this->widget[DEPOT_WIDGET_AUTOREPLACE].data = SPR_REPLACE_TRAIN;
				break;

			case VEH_ROAD:
				this->widget[DEPOT_WIDGET_CAPTION].data      = STR_9003_ROAD_VEHICLE_DEPOT;
				this->widget[DEPOT_WIDGET_STOP_ALL].tooltips = STR_MASS_STOP_DEPOT_ROADVEH_TIP;
				this->widget[DEPOT_WIDGET_START_ALL].tooltips= STR_MASS_START_DEPOT_ROADVEH_TIP;
				this->widget[DEPOT_WIDGET_SELL].tooltips     = STR_9024_DRAG_ROAD_VEHICLE_TO_HERE;
				this->widget[DEPOT_WIDGET_SELL_ALL].tooltips = STR_DEPOT_SELL_ALL_BUTTON_ROADVEH_TIP;
				this->widget[DEPOT_WIDGET_MATRIX].tooltips   = STR_9022_VEHICLES_CLICK_ON_VEHICLE;

				this->widget[DEPOT_WIDGET_BUILD].data        = STR_9004_NEW_VEHICLES;
				this->widget[DEPOT_WIDGET_BUILD].tooltips    = STR_9023_BUILD_NEW_ROAD_VEHICLE;
				this->widget[DEPOT_WIDGET_CLONE].data        = STR_CLONE_ROAD_VEHICLE;
				this->widget[DEPOT_WIDGET_CLONE].tooltips    = STR_CLONE_ROAD_VEHICLE_DEPOT_INFO;

				this->widget[DEPOT_WIDGET_LOCATION].tooltips = STR_9025_CENTER_MAIN_VIEW_ON_ROAD;
				this->widget[DEPOT_WIDGET_VEHICLE_LIST].data = STR_LORRY;
				this->widget[DEPOT_WIDGET_VEHICLE_LIST].tooltips = STR_DEPOT_VEHICLE_ORDER_LIST_ROADVEH_TIP;
				this->widget[DEPOT_WIDGET_AUTOREPLACE].tooltips = STR_DEPOT_AUTOREPLACE_ROADVEH_TIP;

				/* Sprites */
				this->widget[DEPOT_WIDGET_SELL].data        = SPR_SELL_ROADVEH;
				this->widget[DEPOT_WIDGET_SELL_ALL].data    = SPR_SELL_ALL_ROADVEH;
				this->widget[DEPOT_WIDGET_AUTOREPLACE].data = SPR_REPLACE_ROADVEH;
				break;

			case VEH_SHIP:
				this->widget[DEPOT_WIDGET_CAPTION].data      = STR_9803_SHIP_DEPOT;
				this->widget[DEPOT_WIDGET_STOP_ALL].tooltips = STR_MASS_STOP_DEPOT_SHIP_TIP;
				this->widget[DEPOT_WIDGET_START_ALL].tooltips= STR_MASS_START_DEPOT_SHIP_TIP;
				this->widget[DEPOT_WIDGET_SELL].tooltips     = STR_9821_DRAG_SHIP_TO_HERE_TO_SELL;
				this->widget[DEPOT_WIDGET_SELL_ALL].tooltips = STR_DEPOT_SELL_ALL_BUTTON_SHIP_TIP;
				this->widget[DEPOT_WIDGET_MATRIX].tooltips   = STR_981F_SHIPS_CLICK_ON_SHIP_FOR;

				this->widget[DEPOT_WIDGET_BUILD].data        = STR_9804_NEW_SHIPS;
				this->widget[DEPOT_WIDGET_BUILD].tooltips    = STR_9820_BUILD_NEW_SHIP;
				this->widget[DEPOT_WIDGET_CLONE].data        = STR_CLONE_SHIP;
				this->widget[DEPOT_WIDGET_CLONE].tooltips    = STR_CLONE_SHIP_DEPOT_INFO;

				this->widget[DEPOT_WIDGET_LOCATION].tooltips = STR_9822_CENTER_MAIN_VIEW_ON_SHIP;
				this->widget[DEPOT_WIDGET_VEHICLE_LIST].data = STR_SHIP;
				this->widget[DEPOT_WIDGET_VEHICLE_LIST].tooltips = STR_DEPOT_VEHICLE_ORDER_LIST_SHIP_TIP;
				this->widget[DEPOT_WIDGET_AUTOREPLACE].tooltips = STR_DEPOT_AUTOREPLACE_SHIP_TIP;

				/* Sprites */
				this->widget[DEPOT_WIDGET_SELL].data        = SPR_SELL_SHIP;
				this->widget[DEPOT_WIDGET_SELL_ALL].data    = SPR_SELL_ALL_SHIP;
				this->widget[DEPOT_WIDGET_AUTOREPLACE].data = SPR_REPLACE_SHIP;
				break;

			case VEH_AIRCRAFT:
				this->widget[DEPOT_WIDGET_CAPTION].data      = STR_A002_AIRCRAFT_HANGAR;
				this->widget[DEPOT_WIDGET_STOP_ALL].tooltips = STR_MASS_STOP_HANGAR_TIP;
				this->widget[DEPOT_WIDGET_START_ALL].tooltips= STR_MASS_START_HANGAR_TIP;
				this->widget[DEPOT_WIDGET_SELL].tooltips     = STR_A023_DRAG_AIRCRAFT_TO_HERE_TO;
				this->widget[DEPOT_WIDGET_SELL_ALL].tooltips = STR_DEPOT_SELL_ALL_BUTTON_AIRCRAFT_TIP;
				this->widget[DEPOT_WIDGET_MATRIX].tooltips   = STR_A021_AIRCRAFT_CLICK_ON_AIRCRAFT;

				this->widget[DEPOT_WIDGET_BUILD].data        = STR_A003_NEW_AIRCRAFT;
				this->widget[DEPOT_WIDGET_BUILD].tooltips    = STR_A022_BUILD_NEW_AIRCRAFT;
				this->widget[DEPOT_WIDGET_CLONE].data        = STR_CLONE_AIRCRAFT;
				this->widget[DEPOT_WIDGET_CLONE].tooltips    = STR_CLONE_AIRCRAFT_INFO_HANGAR_WINDOW;

				this->widget[DEPOT_WIDGET_LOCATION].tooltips = STR_A024_CENTER_MAIN_VIEW_ON_HANGAR;
				this->widget[DEPOT_WIDGET_VEHICLE_LIST].data = STR_PLANE;
				this->widget[DEPOT_WIDGET_VEHICLE_LIST].tooltips = STR_DEPOT_VEHICLE_ORDER_LIST_AIRCRAFT_TIP;
				this->widget[DEPOT_WIDGET_AUTOREPLACE].tooltips = STR_DEPOT_AUTOREPLACE_AIRCRAFT_TIP;

				/* Sprites */
				this->widget[DEPOT_WIDGET_SELL].data        = SPR_SELL_AIRCRAFT;
				this->widget[DEPOT_WIDGET_SELL_ALL].data    = SPR_SELL_ALL_AIRCRAFT;
				this->widget[DEPOT_WIDGET_AUTOREPLACE].data = SPR_REPLACE_AIRCRAFT;
				break;
		}
	}

	void CreateDepotListWindow(VehicleType type)
	{
		this->type = type;
		_backup_orders_tile = 0;

		assert(IsPlayerBuildableVehicleType(type)); // ensure that we make the call with a valid type

		/* Resize the window according to the vehicle type */

		/* Set the number of blocks in each direction */
		this->vscroll.cap = _resize_cap[type][0];
		this->hscroll.cap = _resize_cap[type][1];

		/* Set the block size */
		this->resize.step_width  = _block_sizes[type][0];
		this->resize.step_height = _block_sizes[type][1];

		/* Enlarge the window to fit with the selected number of blocks of the selected size */
		ResizeWindow(this,
					_block_sizes[type][0] * this->hscroll.cap,
					_block_sizes[type][1] * this->vscroll.cap);

		if (type == VEH_TRAIN) {
			/* Make space for the horizontal scrollbar vertically, and the unit
			 * number, flag, and length counter horizontally. */
			ResizeWindow(this, 36, 12);
			/* substract the newly added space from the matrix since it was meant for the scrollbar */
			this->widget[DEPOT_WIDGET_MATRIX].bottom -= 12;
		}

		/* Set the minimum window size to the current window size */
		this->resize.width  = this->width;
		this->resize.height = this->height;

		this->SetupStringsForDepotWindow(type);

		this->widget[DEPOT_WIDGET_MATRIX].data =
			(this->vscroll.cap * 0x100) // number of rows to draw on the background
			+ (type == VEH_TRAIN ? 1 : this->hscroll.cap); // number of boxes in each row. Trains always have just one


		this->SetWidgetsHiddenState(type != VEH_TRAIN,
			DEPOT_WIDGET_H_SCROLL,
			DEPOT_WIDGET_SELL_CHAIN,
			WIDGET_LIST_END);

		ResizeDepotButtons(this);
	}

	virtual void OnInvalidateData(int data)
	{
		this->generate_list = true;
	}

	virtual void OnPaint()
	{
		if (this->generate_list) {
			/* Generate the vehicle list
			 * It's ok to use the wagon pointers for non-trains as they will be ignored */
			BuildDepotVehicleList(this->type, this->window_number,
				&this->vehicle_list, &this->engine_list_length, &this->engine_count,
				&this->wagon_list,   &this->wagon_list_length,  &this->wagon_count);
			this->generate_list = false;
			DepotSortList(this->vehicle_list, this->engine_count);
		}
		DrawDepotWindow(this);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case DEPOT_WIDGET_MATRIX: // List
				this->DepotClick(pt.x, pt.y);
				break;

			case DEPOT_WIDGET_BUILD: // Build vehicle
				ResetObjectToPlace();
				ShowBuildVehicleWindow(this->window_number, this->type);
				break;

			case DEPOT_WIDGET_CLONE: // Clone button
				this->InvalidateWidget(DEPOT_WIDGET_CLONE);
				this->ToggleWidgetLoweredState(DEPOT_WIDGET_CLONE);

				if (this->IsWidgetLowered(DEPOT_WIDGET_CLONE)) {
					static const CursorID clone_icons[] = {
						SPR_CURSOR_CLONE_TRAIN, SPR_CURSOR_CLONE_ROADVEH,
						SPR_CURSOR_CLONE_SHIP, SPR_CURSOR_CLONE_AIRPLANE
					};

					_place_clicked_vehicle = NULL;
					SetObjectToPlaceWnd(clone_icons[this->type], PAL_NONE, VHM_RECT, this);
				} else {
					ResetObjectToPlace();
				}
					break;

			case DEPOT_WIDGET_LOCATION:
				if (_ctrl_pressed) {
					ShowExtraViewPortWindow(this->window_number);
				} else {
					ScrollMainWindowToTile(this->window_number);
				}
				break;

			case DEPOT_WIDGET_STOP_ALL:
			case DEPOT_WIDGET_START_ALL:
				DoCommandP(this->window_number, 0, this->type | (widget == DEPOT_WIDGET_START_ALL ? (1 << 5) : 0), NULL, CMD_MASS_START_STOP);
				break;

			case DEPOT_WIDGET_SELL_ALL:
				/* Only open the confimation window if there are anything to sell */
				if (this->engine_count != 0 || this->wagon_count != 0) {
					static const StringID confirm_captions[] = {
						STR_8800_TRAIN_DEPOT,
						STR_9003_ROAD_VEHICLE_DEPOT,
						STR_9803_SHIP_DEPOT,
						STR_A002_AIRCRAFT_HANGAR
					};
					TileIndex tile = this->window_number;
					byte vehtype = this->type;

					SetDParam(0, (vehtype == VEH_AIRCRAFT) ? GetStationIndex(tile) : GetDepotByTile(tile)->town_index);
					ShowQuery(
						confirm_captions[vehtype],
						STR_DEPOT_SELL_CONFIRMATION_TEXT,
						this,
						DepotSellAllConfirmationCallback
					);
				}
				break;

			case DEPOT_WIDGET_VEHICLE_LIST:
				ShowVehicleListWindow(GetTileOwner(this->window_number), this->type, (TileIndex)this->window_number);
				break;

			case DEPOT_WIDGET_AUTOREPLACE:
				DoCommandP(this->window_number, this->type, 0, NULL, CMD_DEPOT_MASS_AUTOREPLACE);
				break;

		}
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		const Vehicle *v = CheckMouseOverVehicle();

		if (v != NULL) HandleCloneVehClick(v, this);
	}

	virtual void OnPlaceObjectAbort()
	{
		/* abort clone */
		this->RaiseWidget(DEPOT_WIDGET_CLONE);
		this->InvalidateWidget(DEPOT_WIDGET_CLONE);

		/* abort drag & drop */
		this->sel = INVALID_VEHICLE;
		this->InvalidateWidget(DEPOT_WIDGET_MATRIX);
	};

	/* check if a vehicle in a depot was clicked.. */
	virtual void OnMouseLoop()
	{
		const Vehicle *v = _place_clicked_vehicle;

		/* since OTTD checks all open depot windows, we will make sure that it triggers the one with a clicked clone button */
		if (v != NULL && this->IsWidgetLowered(DEPOT_WIDGET_CLONE)) {
			_place_clicked_vehicle = NULL;
			HandleCloneVehClick(v, this);
		}
	}

	virtual void OnDragDrop(Point pt, int widget)
	{
		switch (widget) {
			case DEPOT_WIDGET_MATRIX: {
				Vehicle *v;
				VehicleID sel = this->sel;

				this->sel = INVALID_VEHICLE;
				this->SetDirty();

				if (this->type == VEH_TRAIN) {
					GetDepotVehiclePtData gdvp = { NULL, NULL };

					if (this->GetVehicleFromDepotWndPt(pt.x, pt.y, &v, &gdvp) == MODE_DRAG_VEHICLE &&
						sel != INVALID_VEHICLE) {
						if (gdvp.wagon != NULL && gdvp.wagon->index == sel && _ctrl_pressed) {
							DoCommandP(GetVehicle(sel)->tile, GetVehicle(sel)->index, true, NULL, CMD_REVERSE_TRAIN_DIRECTION | CMD_MSG(STR_9033_CAN_T_MAKE_VEHICLE_TURN));
						} else if (gdvp.wagon == NULL || gdvp.wagon->index != sel) {
							TrainDepotMoveVehicle(gdvp.wagon, sel, gdvp.head);
						} else if (gdvp.head != NULL && IsFrontEngine(gdvp.head)) {
							ShowVehicleViewWindow(gdvp.head);
						}
					}
				} else if (this->GetVehicleFromDepotWndPt(pt.x, pt.y, &v, NULL) == MODE_DRAG_VEHICLE &&
					v != NULL &&
					sel == v->index) {
					ShowVehicleViewWindow(v);
				}
			} break;

			case DEPOT_WIDGET_SELL: case DEPOT_WIDGET_SELL_CHAIN:
				if (!this->IsWidgetDisabled(DEPOT_WIDGET_SELL) &&
					this->sel != INVALID_VEHICLE) {
					Vehicle *v;
					uint command;
					int sell_cmd;
					bool is_engine;

					if (this->IsWidgetDisabled(widget)) return;
					if (this->sel == INVALID_VEHICLE) return;

					this->HandleButtonClick(widget);

					v = GetVehicle(this->sel);
					this->sel = INVALID_VEHICLE;
					this->SetDirty();

					sell_cmd = (v->type == VEH_TRAIN && (widget == DEPOT_WIDGET_SELL_CHAIN || _ctrl_pressed)) ? 1 : 0;

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
				this->sel = INVALID_VEHICLE;
				this->SetDirty();
		}
		_cursor.vehchain = false;
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		this->vscroll.cap += delta.y / (int)this->resize.step_height;
		this->hscroll.cap += delta.x / (int)this->resize.step_width;
		this->widget[DEPOT_WIDGET_MATRIX].data = (this->vscroll.cap << 8) + (this->type == VEH_TRAIN ? 1 : this->hscroll.cap);
		ResizeDepotButtons(this);
	}

	virtual bool OnCTRLStateChange()
	{
		if (this->sel != INVALID_VEHICLE) {
			_cursor.vehchain = _ctrl_pressed;
			this->InvalidateWidget(DEPOT_WIDGET_MATRIX);
		}

		return true;
	}
};

static void DepotSellAllConfirmationCallback(Window *win, bool confirmed)
{
	if (confirmed) {
		DepotWindow *w = (DepotWindow*)win;
		TileIndex tile = w->window_number;
		byte vehtype = w->type;
		DoCommandP(tile, vehtype, 0, NULL, CMD_DEPOT_SELL_ALL_VEHICLES);
	}
}

/** Opens a depot window
 * @param tile The tile where the depot/hangar is located
 * @param type The type of vehicles in the depot
 */
void ShowDepotWindow(TileIndex tile, VehicleType type)
{
	DepotWindow *w;

	switch (type) {
		default: NOT_REACHED();
		case VEH_TRAIN:
			w = AllocateWindowDescFront<DepotWindow>(&_train_depot_desc, tile); break;
		case VEH_ROAD:
			w = AllocateWindowDescFront<DepotWindow>(&_road_depot_desc, tile); break;
		case VEH_SHIP:
			w = AllocateWindowDescFront<DepotWindow>(&_ship_depot_desc, tile); break;
		case VEH_AIRCRAFT:
			w = AllocateWindowDescFront<DepotWindow>(&_aircraft_depot_desc, tile); break;
	}

	if (w == NULL) return;

	w->caption_color = GetTileOwner(tile);
	w->CreateDepotListWindow(type);
}

/** Removes the highlight of a vehicle in a depot window
 * @param *v Vehicle to remove all highlights from
 */
void DeleteDepotHighlightOfVehicle(const Vehicle *v)
{
	DepotWindow *w;

	/* If we haven't got any vehicles on the mouse pointer, we haven't got any highlighted in any depots either
	 * If that is the case, we can skip looping though the windows and save time
	 */
	if (_special_mouse_mode != WSM_DRAGDROP) return;

	w = dynamic_cast<DepotWindow*>(FindWindowById(WC_VEHICLE_DEPOT, v->tile));
	if (w != NULL) {
		w->sel = INVALID_VEHICLE;
		ResetObjectToPlace();
	}
}
