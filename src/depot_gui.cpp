/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file depot_gui.cpp The GUI for depots. */

#include "train.h"
#include "ship.h"
#include "aircraft.h"
#include "roadveh.h"
#include "gui.h"
#include "textbuf_gui.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "command_func.h"
#include "depot_base.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "spritecache.h"
#include "strings_func.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "company_func.h"
#include "tilehighlight_func.h"
#include "window_gui.h"
#include "vehiclelist.h"

#include "table/strings.h"
#include "table/sprites.h"

/*
 * Since all depot window sizes aren't the same, we need to modify sizes a little.
 * It's done with the following arrays of widget indexes. Each of them tells if a widget side should be moved and in what direction.
 * How long they should be moved and for what window types are controlled in ShowDepotWindow()
 */

/* Names of the widgets. Keep them in the same order as in the widget array */
enum DepotWindowWidgets {
	DEPOT_WIDGET_CAPTION,
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
};

/** Nested widget definition for train depots. */
static const NWidgetPart _nested_train_depot_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, DEPOT_WIDGET_CAPTION),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_MATRIX, COLOUR_GREY, DEPOT_WIDGET_MATRIX), SetDataTip(0x0, STR_NULL), SetResize(1, 1),
			NWidget(WWT_HSCROLLBAR, COLOUR_GREY, DEPOT_WIDGET_H_SCROLL),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_IMGBTN, COLOUR_GREY, DEPOT_WIDGET_SELL), SetDataTip(0x0, STR_NULL), SetResize(0, 1), SetFill(0, 1),
			NWidget(WWT_IMGBTN, COLOUR_GREY, DEPOT_WIDGET_SELL_CHAIN), SetDataTip(SPR_SELL_CHAIN_TRAIN, STR_DEPOT_DRAG_WHOLE_TRAIN_TO_SELL_TOOLTIP), SetResize(0, 1), SetFill(0, 1),
			NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, DEPOT_WIDGET_SELL_ALL), SetDataTip(0x0, STR_NULL),
			NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, DEPOT_WIDGET_AUTOREPLACE), SetDataTip(0x0, STR_NULL),
		EndContainer(),
		NWidget(WWT_SCROLLBAR, COLOUR_GREY, DEPOT_WIDGET_V_SCROLL),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, DEPOT_WIDGET_BUILD), SetDataTip(0x0, STR_NULL), SetFill(1, 1), SetResize(1, 0),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, DEPOT_WIDGET_CLONE), SetDataTip(0x0, STR_NULL), SetFill(1, 1), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, DEPOT_WIDGET_LOCATION), SetDataTip(STR_BUTTON_LOCATION, STR_NULL), SetFill(1, 1), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, DEPOT_WIDGET_VEHICLE_LIST), SetDataTip(0x0, STR_NULL), SetFill(0, 1),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, DEPOT_WIDGET_STOP_ALL), SetDataTip(SPR_FLAG_VEH_STOPPED, STR_NULL), SetFill(0, 1),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, DEPOT_WIDGET_START_ALL), SetDataTip(SPR_FLAG_VEH_RUNNING, STR_NULL), SetFill(0, 1),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static const WindowDesc _train_depot_desc(
	WDP_AUTO, WDP_AUTO, 362, 123,
	WC_VEHICLE_DEPOT, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_train_depot_widgets, lengthof(_nested_train_depot_widgets)
);

static const WindowDesc _road_depot_desc(
	WDP_AUTO, WDP_AUTO, 316, 97,
	WC_VEHICLE_DEPOT, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_train_depot_widgets, lengthof(_nested_train_depot_widgets)
);

static const WindowDesc _ship_depot_desc(
	WDP_AUTO, WDP_AUTO, 306, 99,
	WC_VEHICLE_DEPOT, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_train_depot_widgets, lengthof(_nested_train_depot_widgets)
);

static const WindowDesc _aircraft_depot_desc(
	WDP_AUTO, WDP_AUTO, 332, 99,
	WC_VEHICLE_DEPOT, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_train_depot_widgets, lengthof(_nested_train_depot_widgets)
);

extern void DepotSortList(VehicleList *list);

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

	const Vehicle *v = Vehicle::Get(_new_vehicle_id);

	ShowVehicleViewWindow(v);
}

static void TrainDepotMoveVehicle(const Vehicle *wagon, VehicleID sel, const Vehicle *head)
{
	const Vehicle *v = Vehicle::Get(sel);

	if (v == wagon) return;

	if (wagon == NULL) {
		if (head != NULL) wagon = head->Last();
	} else {
		wagon = wagon->Previous();
		if (wagon == NULL) return;
	}

	if (wagon == v) return;

	DoCommandP(v->tile, v->index + ((wagon == NULL ? INVALID_VEHICLE : wagon->index) << 16), _ctrl_pressed ? 1 : 0, CMD_MOVE_RAIL_VEHICLE | CMD_MSG(STR_ERROR_CAN_T_MOVE_VEHICLE));
}

/** Array containing the cell size in pixels of the #DEPOT_WIDGET_MATRIX widget for each vehicle type.
 * @note The train vehicle type uses the entire row for each train. */
static Dimension _base_block_sizes[4];

static void InitBlocksizeForShipAircraft(VehicleType type)
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
			_base_block_sizes[VEH_SHIP].width = max(76U, max_width);
			break;
		case VEH_AIRCRAFT:
			_base_block_sizes[VEH_AIRCRAFT].width = max(67U, max_width);
			break;
	}
	_base_block_sizes[type].height = max(GetVehicleHeight(type), max_height);
}

/** Set the size of the blocks in the window so we can be sure that they are big enough for the vehicle sprites in the current game.
 * @note Calling this function once for each game is enough. */
void InitDepotWindowBlockSizes()
{
	_base_block_sizes[VEH_TRAIN].width = 0;
	_base_block_sizes[VEH_TRAIN].height = GetVehicleHeight(VEH_TRAIN);

	_base_block_sizes[VEH_ROAD].width = 32;
	_base_block_sizes[VEH_ROAD].height = GetVehicleHeight(VEH_ROAD);

	InitBlocksizeForShipAircraft(VEH_SHIP);
	InitBlocksizeForShipAircraft(VEH_AIRCRAFT);
}

static void DepotSellAllConfirmationCallback(Window *w, bool confirmed);
const Sprite *GetAircraftSprite(EngineID engine);

struct DepotWindow : Window {
	VehicleID sel;
	VehicleType type;
	bool generate_list;
	VehicleList vehicle_list;
	VehicleList wagon_list;

	DepotWindow(const WindowDesc *desc, TileIndex tile, VehicleType type) : Window()
	{
		assert(IsCompanyBuildableVehicleType(type)); // ensure that we make the call with a valid type

		this->sel = INVALID_VEHICLE;
		this->generate_list = true;
		this->type = type;

		this->CreateNestedTree(desc);
		this->SetupWidgetData(type);
		this->FinishInitNested(desc, tile);

		this->owner = GetTileOwner(tile);
		_backup_orders_tile = 0;

	}

	~DepotWindow()
	{
		DeleteWindowById(WC_BUILD_VEHICLE, this->window_number);
	}

	/** Draw a vehicle in the depot window in the box with the top left corner at x,y.
	 * @param v     Vehicle to draw.
	 * @param left  Left side of the box to draw in.
	 * @param right Right side of the box to draw in.
	 * @param y     Top of the box to draw in.
	 */
	void DrawVehicleInDepot(const Vehicle *v, int left, int right, int y) const
	{
		bool free_wagon = false;
		int sprite_y = y + (this->resize.step_height - GetVehicleHeight(v->type)) / 2;

		bool rtl = _dynlang.text_dir == TD_RTL;
		int image_left  = rtl ? left  + this->count_width  : left  + this->header_width;
		int image_right = rtl ? right - this->header_width : right - this->count_width;

		switch (v->type) {
			case VEH_TRAIN: {
				const Train *u = Train::From(v);
				free_wagon = u->IsFreeWagon();

				uint x_space = free_wagon ? TRAININFO_DEFAULT_VEHICLE_WIDTH : 0;
				DrawTrainImage(u, image_left + (rtl ? 0 : x_space), image_right - (rtl ? x_space : 0), sprite_y - 1, this->sel, free_wagon ? 0 : this->hscroll.GetPosition());

				/* Number of wagons relative to a standard length wagon (rounded up) */
				SetDParam(0, (u->tcache.cached_total_length + 7) / 8);
				DrawString(rtl ? left + WD_FRAMERECT_LEFT : right - this->count_width, rtl ? left + this->count_width : right - WD_FRAMERECT_RIGHT, y + (this->resize.step_height - FONT_HEIGHT_SMALL) / 2, STR_TINY_BLACK_COMA, TC_FROMSTRING, SA_RIGHT); // Draw the counter
				break;
			}

			case VEH_ROAD:     DrawRoadVehImage( v, image_left, image_right, sprite_y, this->sel); break;
			case VEH_SHIP:     DrawShipImage(    v, image_left, image_right, sprite_y, this->sel); break;
			case VEH_AIRCRAFT: {
				const Sprite *spr = GetSprite(v->GetImage(DIR_W), ST_NORMAL);
				DrawAircraftImage(v, image_left, image_right,
									y + max(spr->height + spr->y_offs - 14, 0), // tall sprites needs an y offset
									this->sel);
			} break;
			default: NOT_REACHED();
		}

		uint diff_x, diff_y;
		if (v->type == VEH_TRAIN || v->type == VEH_ROAD) {
			/* Arrange unitnumber and flag horizontally */
			diff_x = this->flag_width + WD_FRAMERECT_LEFT;
			diff_y = (this->resize.step_height - this->flag_height) / 2 - 2;
		} else {
			/* Arrange unitnumber and flag vertically */
			diff_x = WD_FRAMERECT_LEFT;
			diff_y = FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
		}
		int text_left  = rtl ? right - this->header_width - 1 : left + diff_x;
		int text_right = rtl ? right - diff_x : left + this->header_width - 1;

		if (free_wagon) {
			DrawString(text_left, text_right, y + 2, STR_DEPOT_NO_ENGINE);
		} else {
			DrawSprite((v->vehstatus & VS_STOPPED) ? SPR_FLAG_VEH_STOPPED : SPR_FLAG_VEH_RUNNING, PAL_NONE, rtl ? right - this->flag_width : left + WD_FRAMERECT_LEFT, y + diff_y);

			SetDParam(0, v->unitnumber);
			DrawString(text_left, text_right, y + 2, (uint16)(v->max_age - DAYS_IN_LEAP_YEAR) >= v->age ? STR_BLACK_COMMA : STR_RED_COMMA);
		}
	}

	void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != DEPOT_WIDGET_MATRIX) return;

		bool rtl = _dynlang.text_dir == TD_RTL;

		/* Set the row and number of boxes in each row based on the number of boxes drawn in the matrix */
		uint16 mat_data = this->GetWidget<NWidgetCore>(DEPOT_WIDGET_MATRIX)->widget_data;
		uint16 rows_in_display   = GB(mat_data, MAT_ROW_START, MAT_ROW_BITS);
		uint16 boxes_in_each_row = GB(mat_data, MAT_COL_START, MAT_COL_BITS);

		uint16 num = this->vscroll.GetPosition() * boxes_in_each_row;
		int maxval = min(this->vehicle_list.Length(), num + (rows_in_display * boxes_in_each_row));
		int y;
		for (y = r.top + 1; num < maxval; y += this->resize.step_height) { // Draw the rows
			for (byte i = 0; i < boxes_in_each_row && num < maxval; i++, num++) {
				/* Draw all vehicles in the current row */
				const Vehicle *v = this->vehicle_list[num];
				if (boxes_in_each_row == 1) {
					this->DrawVehicleInDepot(v, r.left, r.right, y);
				} else {
					int x = r.left + (rtl ? (boxes_in_each_row - i - 1) : i) * this->resize.step_width;
					this->DrawVehicleInDepot(v, x, x + this->resize.step_width - 1, y);
				}
			}
		}

		maxval = min(this->vehicle_list.Length() + this->wagon_list.Length(), (this->vscroll.GetPosition() * boxes_in_each_row) + (rows_in_display * boxes_in_each_row));

		/* draw the train wagons, that do not have an engine in front */
		for (; num < maxval; num++, y += 14) {
			const Vehicle *v = this->wagon_list[num - this->vehicle_list.Length()];
			this->DrawVehicleInDepot(v, r.left, r.right, y);
		}
	}

	void SetStringParameters(int widget) const
	{
		if (widget != DEPOT_WIDGET_CAPTION) return;

		/* locate the depot struct */
		TileIndex tile = this->window_number;
		if (this->type == VEH_AIRCRAFT) {
			SetDParam(0, GetStationIndex(tile)); // Airport name
		} else {
			Depot *depot = Depot::GetByTile(tile);
			assert(depot != NULL);

			SetDParam(0, depot->town_index);
		}
	}

	struct GetDepotVehiclePtData {
		const Vehicle *head;
		const Vehicle *wagon;
	};

	enum DepotGUIAction {
		MODE_ERROR,
		MODE_DRAG_VEHICLE,
		MODE_SHOW_VEHICLE,
		MODE_START_STOP,
	};

	DepotGUIAction GetVehicleFromDepotWndPt(int x, int y, const Vehicle **veh, GetDepotVehiclePtData *d) const
	{
		const NWidgetCore *matrix_widget = this->GetWidget<NWidgetCore>(DEPOT_WIDGET_MATRIX);
		/* In case of RTL the widgets are swapped as a whole */
		if (_dynlang.text_dir == TD_RTL) x = matrix_widget->current_x - x;

		uint xt = 0, xm = 0, ym = 0;
		if (this->type == VEH_TRAIN) {
			xm = x;
		} else {
			xt = x / this->resize.step_width;
			xm = x % this->resize.step_width;
			if (xt >= this->hscroll.GetCapacity()) return MODE_ERROR;
		}
		ym = y % this->resize.step_height;

		uint row = y / this->resize.step_height;
		if (row >= this->vscroll.GetCapacity()) return MODE_ERROR;

		uint boxes_in_each_row = GB(matrix_widget->widget_data, MAT_COL_START, MAT_COL_BITS);
		uint pos = ((row + this->vscroll.GetPosition()) * boxes_in_each_row) + xt;

		if (this->vehicle_list.Length() + this->wagon_list.Length() <= pos) {
			/* Clicking on 'line' / 'block' without a vehicle */
			if (this->type == VEH_TRAIN) {
				/* End the dragging */
				d->head  = NULL;
				d->wagon = NULL;
				return MODE_DRAG_VEHICLE;
			} else {
				return MODE_ERROR; // empty block, so no vehicle is selected
			}
		}

		bool wagon = false;
		if (this->vehicle_list.Length() > pos) {
			*veh = this->vehicle_list[pos];
			/* Skip vehicles that are scrolled off the list */
			x += this->hscroll.GetPosition();
		} else {
			pos -= this->vehicle_list.Length();
			*veh = this->wagon_list[pos];
			/* free wagons don't have an initial loco. */
			x -= VEHICLEINFO_FULL_VEHICLE_WIDTH;
			wagon = true;
		}

		const Train *v = NULL;
		if (this->type == VEH_TRAIN) {
			v = Train::From(*veh);
			d->head = d->wagon = v;
		}

		if (xm <= this->header_width) {
			switch (this->type) {
				case VEH_TRAIN:
					if (wagon) return MODE_ERROR;
				case VEH_ROAD:
					if (xm <= this->flag_width) return MODE_START_STOP;
					break;

				case VEH_SHIP:
				case VEH_AIRCRAFT:
					if (xm <= this->flag_width && ym >= (uint)(FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL)) return MODE_START_STOP;
					break;

				default: NOT_REACHED();
			}
			return MODE_SHOW_VEHICLE;
		}

		if (this->type != VEH_TRAIN) return MODE_DRAG_VEHICLE;

		/* Clicking on the counter */
		if (xm >= matrix_widget->current_x - this->count_width) return wagon ? MODE_ERROR : MODE_SHOW_VEHICLE;

		/* Account for the header */
		x -= this->header_width;

		/* find the vehicle in this row that was clicked */
		for (; v != NULL; v = v->Next()) {
			x -= v->GetDisplayImageWidth();
			if (x < 0) break;
		}

		d->wagon = (v != NULL ? v->GetFirstEnginePart() : NULL);

		return MODE_DRAG_VEHICLE;
	}

	/** Handle click in the depot matrix.
	 * @param x Horizontal position in the matrix widget in pixels.
	 * @param y Vertical position in the matrix widget in pixels.
	 */
	void DepotClick(int x, int y)
	{
		GetDepotVehiclePtData gdvp = { NULL, NULL };
		const Vehicle *v = NULL;
		DepotGUIAction mode = this->GetVehicleFromDepotWndPt(x, y, &v, &gdvp);

		/* share / copy orders */
		if (_thd.place_mode != HT_NONE && mode != MODE_ERROR) {
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
					SetObjectToPlaceWnd(image, GetVehiclePalette(v), HT_DRAG, this);

					switch (v->type) {
						case VEH_TRAIN:
							_cursor.short_vehicle_offset = 16 - Train::From(v)->tcache.cached_veh_length * 2;
							break;

						case VEH_ROAD:
							_cursor.short_vehicle_offset = 16 - RoadVehicle::From(v)->rcache.cached_veh_length * 2;
							break;

						default:
							_cursor.short_vehicle_offset = 0;
							break;
					}
					_cursor.vehchain = _ctrl_pressed;
				}
			} break;

			case MODE_SHOW_VEHICLE: // show info window
				ShowVehicleViewWindow(v);
				break;

			case MODE_START_STOP: { // click start/stop flag
				uint command;

				switch (this->type) {
					case VEH_TRAIN:    command = CMD_START_STOP_VEHICLE | CMD_MSG(STR_ERROR_CAN_T_STOP_START_TRAIN);        break;
					case VEH_ROAD:     command = CMD_START_STOP_VEHICLE | CMD_MSG(STR_ERROR_CAN_T_STOP_START_ROAD_VEHICLE); break;
					case VEH_SHIP:     command = CMD_START_STOP_VEHICLE | CMD_MSG(STR_ERROR_CAN_T_STOP_START_SHIP);         break;
					case VEH_AIRCRAFT: command = CMD_START_STOP_VEHICLE | CMD_MSG(STR_ERROR_CAN_T_STOP_START_AIRCRAFT);     break;
					default: NOT_REACHED();
				}
				DoCommandP(v->tile, v->index, 0, command);
			} break;

			default: NOT_REACHED();
		}
	}

	/**
	 * Clones a vehicle
	 * @param *v is the original vehicle to clone
	 */
	void HandleCloneVehClick(const Vehicle *v)
	{
		if (v == NULL || !IsCompanyBuildableVehicleType(v)) return;

		if (!v->IsPrimaryVehicle()) {
			v = v->First();
			/* Do nothing when clicking on a train in depot with no loc attached */
			if (v->type == VEH_TRAIN && !Train::From(v)->IsFrontEngine()) return;
		}

		DoCommandP(this->window_number, v->index, _ctrl_pressed ? 1 : 0, CMD_CLONE_VEHICLE | CMD_MSG(STR_ERROR_CAN_T_BUY_TRAIN + v->type), CcCloneVehicle);

		ResetObjectToPlace();
	}

	/* Function to set up vehicle specific widgets (mainly sprites and strings).
	 * Only use this if it's the same widget, that's used for more than one vehicle type and it needs different text/sprites
	 * Vehicle specific text/sprites, that's in a widget, that's only shown for one vehicle type (like sell whole train) is set in the nested widget array
	 */
	void SetupWidgetData(VehicleType type)
	{
		if (type != VEH_TRAIN) this->GetWidget<NWidgetCore>(DEPOT_WIDGET_SELL_CHAIN)->fill_y = 0; // Disable vertical filling of chain-sell widget for non-train windows.

		this->GetWidget<NWidgetCore>(DEPOT_WIDGET_CAPTION)->widget_data   = STR_DEPOT_TRAIN_CAPTION + type;
		this->GetWidget<NWidgetCore>(DEPOT_WIDGET_STOP_ALL)->tool_tip     = STR_DEPOT_MASS_STOP_DEPOT_TRAIN_TOOLTIP + type;
		this->GetWidget<NWidgetCore>(DEPOT_WIDGET_START_ALL)->tool_tip    = STR_DEPOT_MASS_START_DEPOT_TRAIN_TOOLTIP + type;
		this->GetWidget<NWidgetCore>(DEPOT_WIDGET_SELL)->tool_tip         = STR_DEPOT_TRAIN_SELL_TOOLTIP + type;
		this->GetWidget<NWidgetCore>(DEPOT_WIDGET_SELL_ALL)->tool_tip     = STR_DEPOT_SELL_ALL_BUTTON_TRAIN_TOOLTIP + type;

		this->GetWidget<NWidgetCore>(DEPOT_WIDGET_BUILD)->SetDataTip(STR_DEPOT_TRAIN_NEW_VEHICLES_BUTTON + type, STR_DEPOT_TRAIN_NEW_VEHICLES_TOOLTIP + type);
		this->GetWidget<NWidgetCore>(DEPOT_WIDGET_CLONE)->SetDataTip(STR_DEPOT_CLONE_TRAIN + type, STR_DEPOT_CLONE_TRAIN_DEPOT_INFO + type);

		this->GetWidget<NWidgetCore>(DEPOT_WIDGET_LOCATION)->tool_tip     = STR_DEPOT_TRAIN_LOCATION_TOOLTIP + type;
		this->GetWidget<NWidgetCore>(DEPOT_WIDGET_VEHICLE_LIST)->tool_tip = STR_DEPOT_VEHICLE_ORDER_LIST_TRAIN_TOOLTIP + type;
		this->GetWidget<NWidgetCore>(DEPOT_WIDGET_AUTOREPLACE)->tool_tip  = STR_DEPOT_AUTOREPLACE_TRAIN_TOOLTIP + type;

		switch (type) {
			default: NOT_REACHED();

			case VEH_TRAIN:
				this->GetWidget<NWidgetCore>(DEPOT_WIDGET_VEHICLE_LIST)->widget_data = STR_TRAIN;

				/* Sprites */
				this->GetWidget<NWidgetCore>(DEPOT_WIDGET_SELL)->widget_data        = SPR_SELL_TRAIN;
				this->GetWidget<NWidgetCore>(DEPOT_WIDGET_SELL_ALL)->widget_data    = SPR_SELL_ALL_TRAIN;
				this->GetWidget<NWidgetCore>(DEPOT_WIDGET_AUTOREPLACE)->widget_data = SPR_REPLACE_TRAIN;
				break;

			case VEH_ROAD:
				this->GetWidget<NWidgetCore>(DEPOT_WIDGET_VEHICLE_LIST)->widget_data = STR_LORRY;

				/* Sprites */
				this->GetWidget<NWidgetCore>(DEPOT_WIDGET_SELL)->widget_data        = SPR_SELL_ROADVEH;
				this->GetWidget<NWidgetCore>(DEPOT_WIDGET_SELL_ALL)->widget_data    = SPR_SELL_ALL_ROADVEH;
				this->GetWidget<NWidgetCore>(DEPOT_WIDGET_AUTOREPLACE)->widget_data = SPR_REPLACE_ROADVEH;
				break;

			case VEH_SHIP:
				this->GetWidget<NWidgetCore>(DEPOT_WIDGET_VEHICLE_LIST)->widget_data = STR_SHIP;

				/* Sprites */
				this->GetWidget<NWidgetCore>(DEPOT_WIDGET_SELL)->widget_data        = SPR_SELL_SHIP;
				this->GetWidget<NWidgetCore>(DEPOT_WIDGET_SELL_ALL)->widget_data    = SPR_SELL_ALL_SHIP;
				this->GetWidget<NWidgetCore>(DEPOT_WIDGET_AUTOREPLACE)->widget_data = SPR_REPLACE_SHIP;
				break;

			case VEH_AIRCRAFT:
				this->GetWidget<NWidgetCore>(DEPOT_WIDGET_VEHICLE_LIST)->widget_data = STR_PLANE;

				/* Sprites */
				this->GetWidget<NWidgetCore>(DEPOT_WIDGET_SELL)->widget_data        = SPR_SELL_AIRCRAFT;
				this->GetWidget<NWidgetCore>(DEPOT_WIDGET_SELL_ALL)->widget_data    = SPR_SELL_ALL_AIRCRAFT;
				this->GetWidget<NWidgetCore>(DEPOT_WIDGET_AUTOREPLACE)->widget_data = SPR_REPLACE_AIRCRAFT;
				break;
		}
	}

	uint count_width;
	uint header_width;
	uint flag_width;
	uint flag_height;

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case DEPOT_WIDGET_SELL_CHAIN:
			case DEPOT_WIDGET_H_SCROLL:
				/* Hide the 'sell chain' and the horizontal scrollbar when not a train depot. */
				if (this->type != VEH_TRAIN) {
					size->height = 0;
					resize->height = 0;
				}
				break;

			case DEPOT_WIDGET_MATRIX: {
				uint min_height = 0;

				if (this->type == VEH_TRAIN) {
					SetDParam(0, 100);
					this->count_width = GetStringBoundingBox(STR_TINY_BLACK_COMA).width + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				} else {
					this->count_width = 0;
				}

				SetDParam(0, 999);
				Dimension unumber = GetStringBoundingBox(STR_BLACK_COMMA);
				const Sprite *spr = GetSprite(SPR_FLAG_VEH_STOPPED, ST_NORMAL);
				this->flag_width  = spr->width + WD_FRAMERECT_RIGHT;
				this->flag_height = spr->height;

				if (this->type == VEH_TRAIN || this->type == VEH_ROAD) {
					min_height = max<uint>(unumber.height + WD_MATRIX_TOP, spr->height);
					this->header_width = unumber.width + this->flag_width;
				} else {
					min_height = unumber.height + spr->height + WD_MATRIX_TOP + WD_PAR_VSEP_NORMAL + WD_MATRIX_BOTTOM;
					this->header_width = max<uint>(unumber.width, this->flag_width) + WD_FRAMERECT_RIGHT;
				}
				int base_width = this->count_width + this->header_width;

				resize->height = max(_base_block_sizes[this->type].height, min_height);
				if (this->type == VEH_TRAIN) {
					resize->width = 1;
					size->width = base_width + 2 * 29; // about 2 parts
					size->height = resize->height * 6;
				} else {
					resize->width = base_width + _base_block_sizes[this->type].width;
					size->width = resize->width * (this->type == VEH_ROAD ? 5 : 3);
					size->height = resize->height * (this->type == VEH_ROAD ? 5 : 3);
				}
				fill->width = resize->width;
				fill->height = resize->height;
			} break;
		}
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
			BuildDepotVehicleList(this->type, this->window_number, &this->vehicle_list, &this->wagon_list);
			this->generate_list = false;
			DepotSortList(&this->vehicle_list);
		}

		/* determine amount of items for scroller */
		if (this->type == VEH_TRAIN) {
			uint max_width = VEHICLEINFO_FULL_VEHICLE_WIDTH;
			for (uint num = 0; num < this->vehicle_list.Length(); num++) {
				uint width = 0;
				for (const Train *v = Train::From(this->vehicle_list[num]); v != NULL; v = v->Next()) {
					width += v->GetDisplayImageWidth();
				}
				max_width = max(max_width, width);
			}
			/* Always have 1 empty row, so people can change the setting of the train */
			this->vscroll.SetCount(this->vehicle_list.Length() + this->wagon_list.Length() + 1);
			this->hscroll.SetCount(max_width);
		} else {
			this->vscroll.SetCount((this->vehicle_list.Length() + this->hscroll.GetCapacity() - 1) / this->hscroll.GetCapacity());
		}

		/* Setup disabled buttons. */
		TileIndex tile = this->window_number;
		this->SetWidgetsDisabledState(!IsTileOwner(tile, _local_company),
			DEPOT_WIDGET_STOP_ALL,
			DEPOT_WIDGET_START_ALL,
			DEPOT_WIDGET_SELL,
			DEPOT_WIDGET_SELL_CHAIN,
			DEPOT_WIDGET_SELL_ALL,
			DEPOT_WIDGET_BUILD,
			DEPOT_WIDGET_CLONE,
			DEPOT_WIDGET_AUTOREPLACE,
			WIDGET_LIST_END);

		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case DEPOT_WIDGET_MATRIX: { // List
				NWidgetBase *nwi = this->GetWidget<NWidgetBase>(DEPOT_WIDGET_MATRIX);
				this->DepotClick(pt.x - nwi->pos_x, pt.y - nwi->pos_y);
				break;
			}

			case DEPOT_WIDGET_BUILD: // Build vehicle
				ResetObjectToPlace();
				ShowBuildVehicleWindow(this->window_number, this->type);
				break;

			case DEPOT_WIDGET_CLONE: // Clone button
				this->SetWidgetDirty(DEPOT_WIDGET_CLONE);
				this->ToggleWidgetLoweredState(DEPOT_WIDGET_CLONE);

				if (this->IsWidgetLowered(DEPOT_WIDGET_CLONE)) {
					static const CursorID clone_icons[] = {
						SPR_CURSOR_CLONE_TRAIN, SPR_CURSOR_CLONE_ROADVEH,
						SPR_CURSOR_CLONE_SHIP, SPR_CURSOR_CLONE_AIRPLANE
					};

					_place_clicked_vehicle = NULL;
					SetObjectToPlaceWnd(clone_icons[this->type], PAL_NONE, HT_RECT, this);
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
				DoCommandP(this->window_number, 0, this->type | (widget == DEPOT_WIDGET_START_ALL ? (1 << 5) : 0), CMD_MASS_START_STOP);
				break;

			case DEPOT_WIDGET_SELL_ALL:
				/* Only open the confimation window if there are anything to sell */
				if (this->vehicle_list.Length() != 0 || this->wagon_list.Length() != 0) {
					TileIndex tile = this->window_number;
					byte vehtype = this->type;

					SetDParam(0, (vehtype == VEH_AIRCRAFT) ? GetStationIndex(tile) : Depot::GetByTile(tile)->town_index);
					ShowQuery(
						STR_DEPOT_TRAIN_CAPTION + vehtype,
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
				DoCommandP(this->window_number, this->type, 0, CMD_DEPOT_MASS_AUTOREPLACE);
				break;

		}
	}

	virtual void OnRightClick(Point pt, int widget)
	{
		if (widget != DEPOT_WIDGET_MATRIX) return;

		GetDepotVehiclePtData gdvp = { NULL, NULL };
		const Vehicle *v = NULL;
		NWidgetBase *nwi = this->GetWidget<NWidgetBase>(DEPOT_WIDGET_MATRIX);
		DepotGUIAction mode = this->GetVehicleFromDepotWndPt(pt.x - nwi->pos_x, pt.y - nwi->pos_y, &v, &gdvp);

		if (this->type == VEH_TRAIN) v = gdvp.wagon;

		if (v != NULL && mode == MODE_DRAG_VEHICLE) {
			CargoArray capacity, loaded;

			/* Display info for single (articulated) vehicle, or for whole chain starting with selected vehicle */
			bool whole_chain = (this->type == VEH_TRAIN && _ctrl_pressed);

			/* loop through vehicle chain and collect cargos */
			uint num = 0;
			for (const Vehicle *w = v; w != NULL; w = w->Next()) {
				if (w->cargo_cap > 0 && w->cargo_type < NUM_CARGO) {
					capacity[w->cargo_type] += w->cargo_cap;
					loaded  [w->cargo_type] += w->cargo.Count();
				}

				if (w->type == VEH_TRAIN && !Train::From(w)->HasArticulatedPart()) {
					num++;
					if (!whole_chain) break;
				}
			}

			/* Build tooltipstring */
			static char details[1024];
			details[0] = '\0';
			char *pos = details;

			for (CargoID cargo_type = 0; cargo_type < NUM_CARGO; cargo_type++) {
				if (capacity[cargo_type] == 0) continue;

				SetDParam(0, cargo_type);           // {CARGO} #1
				SetDParam(1, loaded[cargo_type]);   // {CARGO} #2
				SetDParam(2, cargo_type);           // {SHORTCARGO} #1
				SetDParam(3, capacity[cargo_type]); // {SHORTCARGO} #2
				pos = GetString(pos, STR_DEPOT_VEHICLE_TOOLTIP_CARGO, lastof(details));
			}

			/* Show tooltip window */
			uint64 args[2];
			args[0] = (whole_chain ? num : v->engine_type);
			args[1] = (uint64)(size_t)details;
			GuiShowTooltips(whole_chain ? STR_DEPOT_VEHICLE_TOOLTIP_CHAIN : STR_DEPOT_VEHICLE_TOOLTIP, 2, args);
		} else {
			/* Show tooltip help */
			GuiShowTooltips(STR_DEPOT_TRAIN_LIST_TOOLTIP + this->type);
		}
	}


	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		const Vehicle *v = CheckMouseOverVehicle();

		if (v != NULL) this->HandleCloneVehClick(v);
	}

	virtual void OnPlaceObjectAbort()
	{
		/* abort clone */
		this->RaiseWidget(DEPOT_WIDGET_CLONE);
		this->SetWidgetDirty(DEPOT_WIDGET_CLONE);

		/* abort drag & drop */
		this->sel = INVALID_VEHICLE;
		this->SetWidgetDirty(DEPOT_WIDGET_MATRIX);
	};

	/* check if a vehicle in a depot was clicked.. */
	virtual void OnMouseLoop()
	{
		const Vehicle *v = _place_clicked_vehicle;

		/* since OTTD checks all open depot windows, we will make sure that it triggers the one with a clicked clone button */
		if (v != NULL && this->IsWidgetLowered(DEPOT_WIDGET_CLONE)) {
			_place_clicked_vehicle = NULL;
			this->HandleCloneVehClick(v);
		}
	}

	virtual void OnDragDrop(Point pt, int widget)
	{
		switch (widget) {
			case DEPOT_WIDGET_MATRIX: {
				const Vehicle *v = NULL;
				VehicleID sel = this->sel;

				this->sel = INVALID_VEHICLE;
				this->SetDirty();

				NWidgetBase *nwi = this->GetWidget<NWidgetBase>(DEPOT_WIDGET_MATRIX);
				if (this->type == VEH_TRAIN) {
					GetDepotVehiclePtData gdvp = { NULL, NULL };

					if (this->GetVehicleFromDepotWndPt(pt.x - nwi->pos_x, pt.y - nwi->pos_y, &v, &gdvp) == MODE_DRAG_VEHICLE && sel != INVALID_VEHICLE) {
						if (gdvp.wagon != NULL && gdvp.wagon->index == sel && _ctrl_pressed) {
							DoCommandP(Vehicle::Get(sel)->tile, Vehicle::Get(sel)->index, true,
									CMD_REVERSE_TRAIN_DIRECTION | CMD_MSG(STR_ERROR_CAN_T_REVERSE_DIRECTION_RAIL_VEHICLE));
						} else if (gdvp.wagon == NULL || gdvp.wagon->index != sel) {
							TrainDepotMoveVehicle(gdvp.wagon, sel, gdvp.head);
						} else if (gdvp.head != NULL && Train::From(gdvp.head)->IsFrontEngine()) {
							ShowVehicleViewWindow(gdvp.head);
						}
					}
				} else if (this->GetVehicleFromDepotWndPt(pt.x - nwi->pos_x, pt.y - nwi->pos_y, &v, NULL) == MODE_DRAG_VEHICLE && v != NULL && sel == v->index) {
					ShowVehicleViewWindow(v);
				}
			} break;

			case DEPOT_WIDGET_SELL: case DEPOT_WIDGET_SELL_CHAIN: {
				if (this->IsWidgetDisabled(widget)) return;
				if (this->sel == INVALID_VEHICLE) return;

				this->HandleButtonClick(widget);

				const Vehicle *v = Vehicle::Get(this->sel);
				this->sel = INVALID_VEHICLE;
				this->SetDirty();

				int sell_cmd = (v->type == VEH_TRAIN && (widget == DEPOT_WIDGET_SELL_CHAIN || _ctrl_pressed)) ? 1 : 0;

				bool is_engine = (v->type != VEH_TRAIN || Train::From(v)->IsFrontEngine());

				if (is_engine) {
					_backup_orders_tile = v->tile;
					BackupVehicleOrders(v);
				}

				if (!DoCommandP(v->tile, v->index, sell_cmd, GetCmdSellVeh(v->type)) && is_engine) _backup_orders_tile = 0;
			} break;

			default:
				this->sel = INVALID_VEHICLE;
				this->SetDirty();
		}
		_cursor.vehchain = false;
	}

	virtual void OnTimeout()
	{
		if (!this->IsWidgetDisabled(DEPOT_WIDGET_SELL)) {
			this->RaiseWidget(DEPOT_WIDGET_SELL);
			this->SetWidgetDirty(DEPOT_WIDGET_SELL);
		}
		if (this->nested_array[DEPOT_WIDGET_SELL] != NULL && !this->IsWidgetDisabled(DEPOT_WIDGET_SELL_CHAIN)) {
			this->RaiseWidget(DEPOT_WIDGET_SELL_CHAIN);
			this->SetWidgetDirty(DEPOT_WIDGET_SELL_CHAIN);
		}
	}

	virtual void OnResize()
	{
		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(DEPOT_WIDGET_MATRIX);
		this->vscroll.SetCapacity(nwi->current_y / (int)this->resize.step_height);
		if (this->type == VEH_TRAIN) {
			this->hscroll.SetCapacity(nwi->current_x - this->header_width - this->count_width);
			nwi->widget_data = (this->vscroll.GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
		} else {
			this->hscroll.SetCapacity(nwi->current_x / (int)this->resize.step_width);
			nwi->widget_data = (this->vscroll.GetCapacity() << MAT_ROW_START) + (this->hscroll.GetCapacity() << MAT_COL_START);
		}
	}

	virtual EventState OnCTRLStateChange()
	{
		if (this->sel != INVALID_VEHICLE) {
			_cursor.vehchain = _ctrl_pressed;
			this->SetWidgetDirty(DEPOT_WIDGET_MATRIX);
			return ES_HANDLED;
		}

		return ES_NOT_HANDLED;
	}
};

static void DepotSellAllConfirmationCallback(Window *win, bool confirmed)
{
	if (confirmed) {
		DepotWindow *w = (DepotWindow*)win;
		TileIndex tile = w->window_number;
		byte vehtype = w->type;
		DoCommandP(tile, vehtype, 0, CMD_DEPOT_SELL_ALL_VEHICLES);
	}
}

/** Opens a depot window
 * @param tile The tile where the depot/hangar is located
 * @param type The type of vehicles in the depot
 */
void ShowDepotWindow(TileIndex tile, VehicleType type)
{
	if (BringWindowToFrontById(WC_VEHICLE_DEPOT, tile) != NULL) return;

	const WindowDesc *desc;
	switch (type) {
		default: NOT_REACHED();
		case VEH_TRAIN:    desc = &_train_depot_desc;    break;
		case VEH_ROAD:     desc = &_road_depot_desc;     break;
		case VEH_SHIP:     desc = &_ship_depot_desc;     break;
		case VEH_AIRCRAFT: desc = &_aircraft_depot_desc; break;
	}

	new DepotWindow(desc, tile, type);
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
		if (w->sel == v->index) ResetObjectToPlace();
	}
}
