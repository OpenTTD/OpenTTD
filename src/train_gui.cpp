/* $Id$ */

/** @file train_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "gui.h"
#include "window_gui.h"
#include "vehicle.h"
#include "viewport.h"
#include "command_func.h"
#include "vehicle_gui.h"
#include "depot.h"
#include "train.h"
#include "newgrf_engine.h"
#include "strings_func.h"

void CcBuildWagon(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	Vehicle *v, *found;

	if (!success) return;

	/* find a locomotive in the depot. */
	found = NULL;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_TRAIN && IsFrontEngine(v) &&
				v->tile == tile &&
				v->u.rail.track == TRACK_BIT_DEPOT) {
			if (found != NULL) return; // must be exactly one.
			found = v;
		}
	}

	/* if we found a loco, */
	if (found != NULL) {
		found = GetLastVehicleInChain(found);
		/* put the new wagon at the end of the loco. */
		DoCommandP(0, _new_vehicle_id | (found->index << 16), 0, NULL, CMD_MOVE_RAIL_VEHICLE);
		RebuildVehicleLists();
	}
}

void CcBuildLoco(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	const Vehicle *v;

	if (!success) return;

	v = GetVehicle(_new_vehicle_id);
	if (tile == _backup_orders_tile) {
		_backup_orders_tile = 0;
		RestoreVehicleOrders(v);
	}
	ShowVehicleViewWindow(v);
}

/**
 * Get the number of pixels for the given wagon length.
 * @param len Length measured in 1/8ths of a standard wagon.
 * @return Number of pixels across.
 */
int WagonLengthToPixels(int len)
{
	return (len * _traininfo_vehicle_width) / 8;
}

void DrawTrainImage(const Vehicle *v, int x, int y, int count, int skip, VehicleID selection)
{
	DrawPixelInfo tmp_dpi, *old_dpi;
	int dx = -(skip * 8) / _traininfo_vehicle_width;
	/* Position of highlight box */
	int highlight_l = 0;
	int highlight_r = 0;

	if (!FillDrawPixelInfo(&tmp_dpi, x - 2, y - 1, count + 1, 14)) return;

	count = (count * 8) / _traininfo_vehicle_width;

	old_dpi = _cur_dpi;
	_cur_dpi = &tmp_dpi;

	do {
		int width = v->u.rail.cached_veh_length;

		if (dx + width > 0) {
			if (dx <= count) {
				SpriteID pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
				DrawSprite(v->GetImage(DIR_W), pal, 16 + WagonLengthToPixels(dx), 7 + (is_custom_sprite(RailVehInfo(v->engine_type)->image_index) ? _traininfo_vehicle_pitch : 0));
				if (v->index == selection) {
					/* Set the highlight position */
					highlight_l = WagonLengthToPixels(dx) + 1;
					highlight_r = WagonLengthToPixels(dx + width) + 1;
				}
			}
		}
		dx += width;

		v = v->Next();
	} while (dx < count && v != NULL);

	if (highlight_l != highlight_r) {
		/* Draw the highlight. Now done after drawing all the engines, as
		 * the next engine after the highlight could overlap it. */
		DrawFrameRect(highlight_l, 0, highlight_r, 13, 15, FR_BORDERONLY);
	}

	_cur_dpi = old_dpi;
}

static void TrainDetailsCargoTab(const Vehicle *v, int x, int y)
{
	if (v->cargo_cap != 0) {
		StringID str = STR_8812_EMPTY;

		if (!v->cargo.Empty()) {
			SetDParam(0, v->cargo_type);
			SetDParam(1, v->cargo.Count());
			SetDParam(2, v->cargo.Source());
			SetDParam(3, _patches.freight_trains);
			str = FreightWagonMult(v->cargo_type) > 1 ? STR_FROM_MULT : STR_8813_FROM;
		}
		DrawString(x, y, str, TC_FROMSTRING);
	}
}

static void TrainDetailsInfoTab(const Vehicle *v, int x, int y)
{
	if (RailVehInfo(v->engine_type)->railveh_type == RAILVEH_WAGON) {
		SetDParam(0, v->engine_type);
		SetDParam(1, v->value);
		DrawString(x, y, STR_882D_VALUE, TC_BLACK);
	} else {
		SetDParam(0, v->engine_type);
		SetDParam(1, v->build_year);
		SetDParam(2, v->value);
		DrawString(x, y, STR_882C_BUILT_VALUE, TC_BLACK);
	}
}

static void TrainDetailsCapacityTab(const Vehicle *v, int x, int y)
{
	if (v->cargo_cap != 0) {
		SetDParam(0, v->cargo_type);
		SetDParam(1, v->cargo_cap);
		SetDParam(2, _patches.freight_trains);
		DrawString(x, y, FreightWagonMult(v->cargo_type) > 1 ? STR_CAPACITY_MULT : STR_013F_CAPACITY, TC_FROMSTRING);
	}
}

int GetTrainDetailsWndVScroll(VehicleID veh_id, byte det_tab)
{
	AcceptedCargo act_cargo;
	AcceptedCargo max_cargo;
	int num = 0;

	if (det_tab == 3) { // Total cargo tab
		memset(max_cargo, 0, sizeof(max_cargo));
		memset(act_cargo, 0, sizeof(act_cargo));

		for (const Vehicle *v = GetVehicle(veh_id) ; v != NULL ; v = v->Next()) {
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
		for (const Vehicle *v = GetVehicle(veh_id) ; v != NULL ; v = v->Next()) {
			if (!IsArticulatedPart(v) || v->cargo_cap != 0) num++;
		}
	}

	return num;
}

void DrawTrainDetails(const Vehicle *v, int x, int y, int vscroll_pos, uint16 vscroll_cap, byte det_tab)
{
	/* draw the first 3 details tabs */
	if (det_tab != 3) {
		const Vehicle *u = v;
		x = 1;
		for (;;) {
			if (--vscroll_pos < 0 && vscroll_pos >= -vscroll_cap) {
				int dx = 0;
				int px;
				int py;

				u = v;
				do {
					SpriteID pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
					DrawSprite(u->GetImage(DIR_W), pal, x + WagonLengthToPixels(4 + dx), y + 6 + (is_custom_sprite(RailVehInfo(u->engine_type)->image_index) ? _traininfo_vehicle_pitch : 0));
					dx += u->u.rail.cached_veh_length;
					u = u->Next();
				} while (u != NULL && IsArticulatedPart(u) && u->cargo_cap == 0);

				px = x + WagonLengthToPixels(dx) + 2;
				py = y + 2;
				switch (det_tab) {
					default: NOT_REACHED();
					case 0: TrainDetailsCargoTab(   v, px, py); break;
					case 1:
						/* Only show name and value for the 'real' part */
						if (!IsArticulatedPart(v)) {
							TrainDetailsInfoTab(v, px, py);
						}
						break;
					case 2: TrainDetailsCapacityTab(v, px, py); break;
				}
				y += 14;

				v = u;
			} else {
				/* Move to the next line */
				do {
					v = v->Next();
				} while (v != NULL && IsArticulatedPart(v) && v->cargo_cap == 0);
			}
			if (v == NULL) return;
		}
	} else {
		AcceptedCargo act_cargo;
		AcceptedCargo max_cargo;

		memset(max_cargo, 0, sizeof(max_cargo));
		memset(act_cargo, 0, sizeof(act_cargo));

		for (const Vehicle *u = v; u != NULL ; u = u->Next()) {
			act_cargo[u->cargo_type] += u->cargo.Count();
			max_cargo[u->cargo_type] += u->cargo_cap;
		}

		/* draw total cargo tab */
		DrawString(x, y + 2, STR_013F_TOTAL_CAPACITY_TEXT, TC_FROMSTRING);
		for (CargoID i = 0; i < NUM_CARGO; i++) {
			if (max_cargo[i] > 0 && --vscroll_pos < 0 && vscroll_pos > -vscroll_cap) {
				y += 14;
				SetDParam(0, i);            // {CARGO} #1
				SetDParam(1, act_cargo[i]); // {CARGO} #2
				SetDParam(2, i);            // {SHORTCARGO} #1
				SetDParam(3, max_cargo[i]); // {SHORTCARGO} #2
				SetDParam(4, _patches.freight_trains);
				DrawString(x, y + 2, FreightWagonMult(i) > 1 ? STR_TOTAL_CAPACITY_MULT : STR_013F_TOTAL_CAPACITY, TC_FROMSTRING);
			}
		}
		SetDParam(0, v->cargo.FeederShare());
		DrawString(x, y + 15, STR_FEEDER_CARGO_VALUE, TC_FROMSTRING);
	}
}
