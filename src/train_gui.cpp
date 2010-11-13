/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file train_gui.cpp GUI for trains. */

#include "stdafx.h"
#include "window_gui.h"
#include "gfx_func.h"
#include "command_func.h"
#include "vehicle_gui.h"
#include "train.h"
#include "strings_func.h"
#include "vehicle_func.h"
#include "window_func.h"

#include "table/sprites.h"
#include "table/strings.h"

void CcBuildWagon(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2)
{
	if (result.Failed()) return;

	/* find a locomotive in the depot. */
	const Vehicle *found = NULL;
	const Train *t;
	FOR_ALL_TRAINS(t) {
		if (t->IsFrontEngine() && t->tile == tile &&
				t->track == TRACK_BIT_DEPOT) {
			if (found != NULL) return; // must be exactly one.
			found = t;
		}
	}

	/* if we found a loco, */
	if (found != NULL) {
		found = found->Last();
		/* put the new wagon at the end of the loco. */
		DoCommandP(0, _new_vehicle_id, found->index, CMD_MOVE_RAIL_VEHICLE);
		InvalidateWindowClassesData(WC_TRAINS_LIST, 0);
	}
}

/**
 * Highlight the position where a rail vehicle is dragged over by drawing a light gray background.
 * @param px        The current x position to draw from.
 * @param max_width The maximum space available to draw.
 * @param selection Selected vehicle that is dragged.
 * @return The width of the highlight mark.
 */
static int HighlightDragPosition(int px, int max_width, VehicleID selection)
{
	bool rtl = _current_text_dir == TD_RTL;

	assert(selection != INVALID_VEHICLE);
	Point offset;
	int dragged_width = Train::Get(selection)->GetDisplayImageWidth(&offset) + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;

	int drag_hlight_left = rtl ? max(px -dragged_width, 0) : px;
	int drag_hlight_right = rtl ? px : min(px + dragged_width, max_width);
	int drag_hlight_width = max(drag_hlight_right - drag_hlight_left, 0);

	if (drag_hlight_width > 0) {
		GfxFillRect(drag_hlight_left + WD_FRAMERECT_LEFT, WD_FRAMERECT_TOP + 1,
				drag_hlight_right - WD_FRAMERECT_RIGHT, 13 - WD_FRAMERECT_BOTTOM, _colour_gradient[COLOUR_GREY][7]);
	}

	return drag_hlight_width;
}

/**
 * Draws an image of a whole train
 * @param v         Front vehicle
 * @param left      The minimum horizontal position
 * @param right     The maximum horizontal position
 * @param y         Vertical position to draw at
 * @param selection Selected vehicle to draw a frame around
 * @param skip      Number of pixels to skip at the front (for scrolling)
 * @param drag_dest The vehicle another one is dragged over, \c INVALID_VEHICLE if none.
 */
void DrawTrainImage(const Train *v, int left, int right, int y, VehicleID selection, int skip, VehicleID drag_dest)
{
	bool rtl = _current_text_dir == TD_RTL;
	Direction dir = rtl ? DIR_E : DIR_W;

	DrawPixelInfo tmp_dpi, *old_dpi;
	/* Position of highlight box */
	int highlight_l = 0;
	int highlight_r = 0;
	int max_width = right - left + 1;

	if (!FillDrawPixelInfo(&tmp_dpi, left, y, max_width, 14)) return;

	old_dpi = _cur_dpi;
	_cur_dpi = &tmp_dpi;

	int px = rtl ? max_width + skip : -skip;
	bool sel_articulated = false;
	bool dragging = (drag_dest != INVALID_VEHICLE);
	bool drag_at_end_of_train = (drag_dest == v->index); // Head index is used to mark dragging at end of train.
	for (; v != NULL && (rtl ? px > 0 : px < max_width); v = v->Next()) {
		if (dragging && !drag_at_end_of_train && drag_dest == v->index) {
			/* Highlight the drag-and-drop destination inside the train. */
			int drag_hlight_width = HighlightDragPosition(px, max_width, selection);
			px += rtl ? -drag_hlight_width : drag_hlight_width;
		}

		Point offset;
		int width = Train::From(v)->GetDisplayImageWidth(&offset);

		if (rtl ? px + width > 0 : px - width < max_width) {
			PaletteID pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
			DrawSprite(v->GetImage(dir), pal, px + (rtl ? -offset.x : offset.x), 7 + offset.y);
		}

		if (!v->IsArticulatedPart()) sel_articulated = false;

		if (v->index == selection) {
			/* Set the highlight position */
			highlight_l = rtl ? px - width : px;
			highlight_r = rtl ? px - 1 : px + width - 1;
			sel_articulated = true;
		} else if ((_cursor.vehchain && highlight_r != 0) || sel_articulated) {
			if (rtl) {
				highlight_l -= width;
			} else {
				highlight_r += width;
			}
		}

		px += rtl ? -width : width;
	}

	if (dragging && drag_at_end_of_train) {
		/* Highlight the drag-and-drop destination at the end of the train. */
		HighlightDragPosition(px, max_width, selection);
	}

	if (highlight_l != highlight_r) {
		/* Draw the highlight. Now done after drawing all the engines, as
		 * the next engine after the highlight could overlap it. */
		DrawFrameRect(highlight_l, 0, highlight_r, 13, COLOUR_WHITE, FR_BORDERONLY);
	}

	_cur_dpi = old_dpi;
}

/** Helper struct for the cargo details information */
struct CargoSummaryItem {
	CargoID cargo;    ///< The cargo that is carried
	StringID subtype; ///< STR_EMPTY if none
	uint capacity;    ///< Amount that can be carried
	uint amount;      ///< Amount that is carried
	StationID source; ///< One of the source stations

	/** Used by CargoSummary::Find() and similiar functions */
	FORCEINLINE bool operator != (const CargoSummaryItem &other) const
	{
		return this->cargo != other.cargo || this->subtype != other.subtype;
	}
};

static const uint TRAIN_DETAILS_MIN_INDENT = 32; ///< Minimum indent level in the train details window
static const uint TRAIN_DETAILS_MAX_INDENT = 72; ///< Maximum indent level in the train details window; wider than this and we start on a new line

/** Container for the cargo summary information. */
typedef SmallVector<CargoSummaryItem, 2> CargoSummary;
/** Reused container of cargo details */
static CargoSummary _cargo_summary;

/**
 * Draw the details cargo tab for the given vehicle at the given position
 *
 * @param item  Data to draw
 * @param left  The left most coordinate to draw
 * @param right The right most coordinate to draw
 * @param y     The y coordinate
 */
static void TrainDetailsCargoTab(const CargoSummaryItem *item, int left, int right, int y)
{
	StringID str = STR_VEHICLE_DETAILS_CARGO_EMPTY;

	if (item->amount > 0) {
		SetDParam(0, item->cargo);
		SetDParam(1, item->amount);
		SetDParam(2, item->source);
		SetDParam(3, _settings_game.vehicle.freight_trains);
		str = FreightWagonMult(item->cargo) > 1 ? STR_VEHICLE_DETAILS_CARGO_FROM_MULT : STR_VEHICLE_DETAILS_CARGO_FROM;
	}

	DrawString(left, right, y, str);
}

/**
 * Draw the details info tab for the given vehicle at the given position
 *
 * @param v     current vehicle
 * @param left  The left most coordinate to draw
 * @param right The right most coordinate to draw
 * @param y     The y coordinate
 */
static void TrainDetailsInfoTab(const Vehicle *v, int left, int right, int y)
{
	if (RailVehInfo(v->engine_type)->railveh_type == RAILVEH_WAGON) {
		SetDParam(0, v->engine_type);
		SetDParam(1, v->value);
		DrawString(left, right, y, STR_VEHICLE_DETAILS_TRAIN_WAGON_VALUE, TC_FROMSTRING, SA_LEFT | SA_STRIP);
	} else {
		SetDParam(0, v->engine_type);
		SetDParam(1, v->build_year);
		SetDParam(2, v->value);
		DrawString(left, right, y, STR_VEHICLE_DETAILS_TRAIN_ENGINE_BUILT_AND_VALUE, TC_FROMSTRING, SA_LEFT | SA_STRIP);
	}
}

/**
 * Draw the details capacity tab for the given vehicle at the given position
 *
 * @param item  Data to draw
 * @param left  The left most coordinate to draw
 * @param right The right most coordinate to draw
 * @param y     The y coordinate
 */
static void TrainDetailsCapacityTab(const CargoSummaryItem *item, int left, int right, int y)
{
	SetDParam(0, item->cargo);
	SetDParam(1, item->capacity);
	SetDParam(4, item->subtype);
	SetDParam(5, _settings_game.vehicle.freight_trains);
	DrawString(left, right, y, FreightWagonMult(item->cargo) > 1 ? STR_VEHICLE_INFO_CAPACITY_MULT : STR_VEHICLE_INFO_CAPACITY);
}

/**
 * Collects the cargo transportet
 * @param v Vehicle to process
 * @param summary Space for the result
 */
static void GetCargoSummaryOfArticulatedVehicle(const Train *v, CargoSummary *summary)
{
	summary->Clear();
	do {
		if (v->cargo_cap == 0) continue;

		CargoSummaryItem new_item;
		new_item.cargo = v->cargo_type;
		new_item.subtype = GetCargoSubtypeText(v);

		CargoSummaryItem *item = summary->Find(new_item);
		if (item == summary->End()) {
			item = summary->Append();
			item->cargo = new_item.cargo;
			item->subtype = new_item.subtype;
			item->capacity = 0;
			item->amount = 0;
			item->source = INVALID_STATION;
		}

		item->capacity += v->cargo_cap;
		item->amount += v->cargo.Count();
		if (item->source == INVALID_STATION) item->source = v->cargo.Source();
	} while ((v = v->Next()) != NULL && v->IsArticulatedPart());
}

/**
 * Get the length of an articulated vehicle.
 * @param v the vehicle to get the length of.
 * @return the length in pixels.
 */
static uint GetLengthOfArticulatedVehicle(const Train *v)
{
	uint length = 0;

	do {
		length += v->GetDisplayImageWidth();
	} while ((v = v->Next()) != NULL && v->IsArticulatedPart());

	return length;
}

/**
 * Determines the number of lines in the train details window
 * @param veh_id Train
 * @param det_tab Selected details tab
 * @return Number of line
 */
int GetTrainDetailsWndVScroll(VehicleID veh_id, TrainDetailsWindowTabs det_tab)
{
	int num = 0;

	if (det_tab == TDW_TAB_TOTALS) { // Total cargo tab
		CargoArray act_cargo;
		CargoArray max_cargo;
		for (const Vehicle *v = Vehicle::Get(veh_id); v != NULL; v = v->Next()) {
			act_cargo[v->cargo_type] += v->cargo.Count();
			max_cargo[v->cargo_type] += v->cargo_cap;
		}

		/* Set scroll-amount seperately from counting, as to not compute num double
		 * for more carriages of the same type
		 */
		for (CargoID i = 0; i < NUM_CARGO; i++) {
			if (max_cargo[i] > 0) num++; // only count carriages that the train has
		}
		num++; // needs one more because first line is description string
	} else {
		for (const Train *v = Train::Get(veh_id); v != NULL; v = v->GetNextVehicle()) {
			GetCargoSummaryOfArticulatedVehicle(v, &_cargo_summary);
			num += max(1u, _cargo_summary.Length());

			uint length = GetLengthOfArticulatedVehicle(v);
			if (length > TRAIN_DETAILS_MAX_INDENT) num++;
		}
	}

	return num;
}

/**
 * Draw the details for the given vehicle at the given position
 *
 * @param v     current vehicle
 * @param left  The left most coordinate to draw
 * @param right The right most coordinate to draw
 * @param y     The y coordinate
 * @param vscroll_pos Position of scrollbar
 * @param vscroll_cap Number of lines currently displayed
 * @param det_tab Selected details tab
 */
void DrawTrainDetails(const Train *v, int left, int right, int y, int vscroll_pos, uint16 vscroll_cap, TrainDetailsWindowTabs det_tab)
{
	/* draw the first 3 details tabs */
	if (det_tab != TDW_TAB_TOTALS) {
		bool rtl = _current_text_dir == TD_RTL;
		Direction dir = rtl ? DIR_E : DIR_W;
		int x = rtl ? right : left;
		int sprite_y_offset = 4 + (FONT_HEIGHT_NORMAL - 10) / 2;
		int line_height = WD_MATRIX_TOP + FONT_HEIGHT_NORMAL + WD_MATRIX_BOTTOM;
		for (; v != NULL && vscroll_pos > -vscroll_cap; v = v->GetNextVehicle()) {
			GetCargoSummaryOfArticulatedVehicle(v, &_cargo_summary);

			/* Draw sprites */
			uint dx = 0;
			int px = x;
			const Train *u = v;
			do {
				Point offset;
				int width = u->GetDisplayImageWidth(&offset);
				if (vscroll_pos <= 0 && vscroll_pos > -vscroll_cap) {
					PaletteID pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
					DrawSprite(u->GetImage(dir), pal, px + (rtl ? -offset.x : offset.x), y - line_height * vscroll_pos + sprite_y_offset + offset.y);
				}
				px += rtl ? -width : width;
				dx += width;
				u = u->Next();
			} while (u != NULL && u->IsArticulatedPart());

			bool separate_sprite_row = (dx > TRAIN_DETAILS_MAX_INDENT);
			if (separate_sprite_row) {
				vscroll_pos--;
				dx = 0;
			}

			uint num_lines = max(1u, _cargo_summary.Length());
			for (uint i = 0; i < num_lines; i++) {
				int sprite_width = max<int>(dx, TRAIN_DETAILS_MIN_INDENT) + 3;
				int data_left  = left + (rtl ? 0 : sprite_width);
				int data_right = right - (rtl ? sprite_width : 0);
				if (vscroll_pos <= 0 && vscroll_pos > -vscroll_cap) {
					int py = y - line_height * vscroll_pos;
					if (i > 0 || separate_sprite_row) {
						if (vscroll_pos != 0) GfxFillRect(left, py - WD_MATRIX_TOP - 1, right, py - WD_MATRIX_TOP, _colour_gradient[COLOUR_GREY][5]);
					}
					switch (det_tab) {
						case TDW_TAB_CARGO:
							if (i < _cargo_summary.Length()) {
								TrainDetailsCargoTab(&_cargo_summary[i], data_left, data_right, py);
							} else {
								DrawString(data_left, data_right, py, STR_QUANTITY_N_A, TC_LIGHT_BLUE);
							}
							break;

						case TDW_TAB_INFO:
							if (i == 0) TrainDetailsInfoTab(v, data_left, data_right, py);
							break;

						case TDW_TAB_CAPACITY:
							if (i < _cargo_summary.Length()) {
								TrainDetailsCapacityTab(&_cargo_summary[i], data_left, data_right, py);
							} else {
								DrawString(data_left, data_right, py, STR_VEHICLE_INFO_NO_CAPACITY);
							}
							break;

						default: NOT_REACHED();
					}
				}
				vscroll_pos--;
			}
		}
	} else {
		CargoArray act_cargo;
		CargoArray max_cargo;
		Money feeder_share = 0;

		for (const Vehicle *u = v; u != NULL; u = u->Next()) {
			act_cargo[u->cargo_type] += u->cargo.Count();
			max_cargo[u->cargo_type] += u->cargo_cap;
			feeder_share             += u->cargo.FeederShare();
		}

		/* draw total cargo tab */
		DrawString(left, right, y, STR_VEHICLE_DETAILS_TRAIN_TOTAL_CAPACITY_TEXT);
		y += WD_MATRIX_TOP + FONT_HEIGHT_NORMAL + WD_MATRIX_BOTTOM;

		for (CargoID i = 0; i < NUM_CARGO; i++) {
			if (max_cargo[i] > 0 && --vscroll_pos < 0 && vscroll_pos > -vscroll_cap) {
				SetDParam(0, i);            // {CARGO} #1
				SetDParam(1, act_cargo[i]); // {CARGO} #2
				SetDParam(2, i);            // {SHORTCARGO} #1
				SetDParam(3, max_cargo[i]); // {SHORTCARGO} #2
				SetDParam(4, _settings_game.vehicle.freight_trains);
				DrawString(left, right, y, FreightWagonMult(i) > 1 ? STR_VEHICLE_DETAILS_TRAIN_TOTAL_CAPACITY_MULT : STR_VEHICLE_DETAILS_TRAIN_TOTAL_CAPACITY);
				y += WD_MATRIX_TOP + FONT_HEIGHT_NORMAL + WD_MATRIX_BOTTOM;
			}
		}
		SetDParam(0, feeder_share);
		DrawString(left, right, y, STR_VEHICLE_INFO_FEEDER_CARGO_VALUE);
	}
}
