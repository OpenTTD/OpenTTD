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
#include "engine_base.h"
#include "window_func.h"

#include "table/sprites.h"
#include "table/strings.h"

void CcBuildWagon(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (!success) return;

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
		DoCommandP(0, _new_vehicle_id | (found->index << 16), 0, CMD_MOVE_RAIL_VEHICLE);
		InvalidateWindowClassesData(WC_TRAINS_LIST, 0);
	}
}

void CcBuildLoco(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (!success) return;

	const Vehicle *v = Vehicle::Get(_new_vehicle_id);
	if (tile == _backup_orders_tile) {
		_backup_orders_tile = 0;
		RestoreVehicleOrders(v);
	}
	ShowVehicleViewWindow(v);
}

/**
 * Draws an image of a whole train
 * @param v         Front vehicle
 * @param left      The minimum horizontal position
 * @param right     The maximum horizontal position
 * @param y         Vertical position to draw at
 * @param selection Selected vehicle to draw a frame around
 * @param skip      Number of pixels to skip at the front (for scrolling)
 */
void DrawTrainImage(const Train *v, int left, int right, int y, VehicleID selection, int skip)
{
	bool rtl = _dynlang.text_dir == TD_RTL;
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
	for (; v != NULL && (rtl ? px > 0 : px < max_width); v = v->Next()) {
		Point offset;
		int width = Train::From(v)->GetDisplayImageWidth(&offset);

		if (rtl ? px + width > 0 : px - width < max_width) {
			SpriteID pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
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

	if (highlight_l != highlight_r) {
		/* Draw the highlight. Now done after drawing all the engines, as
		 * the next engine after the highlight could overlap it. */
		DrawFrameRect(highlight_l, 0, highlight_r, 13, COLOUR_WHITE, FR_BORDERONLY);
	}

	_cur_dpi = old_dpi;
}

/**
 * Draw the details cargo tab for the given vehicle at the given position
 *
 * @param v     current vehicle
 * @param left  The left most coordinate to draw
 * @param right The right most coordinate to draw
 * @param y     The y coordinate
 */
static void TrainDetailsCargoTab(const Vehicle *v, int left, int right, int y)
{
	if (v->cargo_cap != 0) {
		StringID str = STR_VEHICLE_DETAILS_CARGO_EMPTY;

		if (!v->cargo.Empty()) {
			SetDParam(0, v->cargo_type);
			SetDParam(1, v->cargo.Count());
			SetDParam(2, v->cargo.Source());
			SetDParam(3, _settings_game.vehicle.freight_trains);
			str = FreightWagonMult(v->cargo_type) > 1 ? STR_VEHICLE_DETAILS_CARGO_FROM_MULT : STR_VEHICLE_DETAILS_CARGO_FROM;
		}
		DrawString(left, right, y, str);
	}
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
 * @param v     current vehicle
 * @param left  The left most coordinate to draw
 * @param right The right most coordinate to draw
 * @param y     The y coordinate
 */
static void TrainDetailsCapacityTab(const Vehicle *v, int left, int right, int y)
{
	if (v->cargo_cap != 0) {
		SetDParam(0, v->cargo_type);
		SetDParam(1, v->cargo_cap);
		SetDParam(4, GetCargoSubtypeText(v));
		SetDParam(5, _settings_game.vehicle.freight_trains);
		DrawString(left, right, y, FreightWagonMult(v->cargo_type) > 1 ? STR_VEHICLE_INFO_CAPACITY_MULT : STR_VEHICLE_INFO_CAPACITY);
	}
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
		for (const Vehicle *v = Vehicle::Get(veh_id) ; v != NULL ; v = v->Next()) {
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
		for (const Train *v = Train::Get(veh_id) ; v != NULL ; v = v->Next()) {
			if (!v->IsArticulatedPart() || v->cargo_cap != 0) num++;
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
		bool rtl = _dynlang.text_dir == TD_RTL;
		Direction dir = rtl ? DIR_E : DIR_W;
		const Train *u = v;
		int x = rtl ? right : left;
		for (;;) {
			if (--vscroll_pos < 0 && vscroll_pos >= -vscroll_cap) {
				int px = x;

				u = v;
				do {
					Point offset;
					int width = u->GetDisplayImageWidth(&offset);
					SpriteID pal = (u->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(u);
					DrawSprite(u->GetImage(dir), pal, px + (rtl ? -offset.x : offset.x), y + 4 + offset.y);
					px += rtl ? -width : width;
					u = u->Next();
				} while (u != NULL && u->IsArticulatedPart() && u->cargo_cap == 0);

				px += rtl ? -2 : 2;
				int py = y;
				switch (det_tab) {
					default: NOT_REACHED();

					case TDW_TAB_CARGO:
						TrainDetailsCargoTab(v, rtl ? left : px, rtl ? px : right, py);
						break;

					case TDW_TAB_INFO:
						/* Only show name and value for the 'real' part */
						if (!v->IsArticulatedPart()) {
							TrainDetailsInfoTab(v, rtl ? left : px, rtl ? px : right, py);
						}
						break;

					case TDW_TAB_CAPACITY:
						TrainDetailsCapacityTab(v, rtl ? left : px, rtl ? px : right, py);
						break;
				}
				y += WD_MATRIX_TOP + FONT_HEIGHT_NORMAL + WD_MATRIX_BOTTOM;

				v = u;
			} else {
				/* Move to the next line */
				do {
					v = v->Next();
				} while (v != NULL && v->IsArticulatedPart() && v->cargo_cap == 0);
			}
			if (v == NULL) return;
		}
	} else {
		CargoArray act_cargo;
		CargoArray max_cargo;
		Money feeder_share = 0;

		for (const Vehicle *u = v; u != NULL ; u = u->Next()) {
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
