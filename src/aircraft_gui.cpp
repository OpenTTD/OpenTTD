/* $Id$ */

/** @file aircraft_gui.cpp The GUI of aircraft. */

#include "stdafx.h"
#include "aircraft.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "strings_func.h"
#include "vehicle_func.h"
#include "gfx_func.h"
#include "window_gui.h"

#include "table/sprites.h"
#include "table/strings.h"

/**
 * Draw the details for the given vehicle at the given position
 *
 * @param v     current vehicle
 * @param left  The left most coordinate to draw
 * @param right The right most coordinate to draw
 * @param y     The y coordinate
 */
void DrawAircraftDetails(const Vehicle *v, int left, int right, int y)
{
	int y_offset = (v->Next()->cargo_cap != 0) ? -11 : 0;
	Money feeder_share = 0;

	for (const Vehicle *u = v ; u != NULL ; u = u->Next()) {
		if (IsNormalAircraft(u)) {
			SetDParam(0, u->engine_type);
			SetDParam(1, u->build_year);
			SetDParam(2, u->value);
			DrawString(left, right, y, STR_VEHICLE_INFO_BUILT_VALUE);

			SetDParam(0, u->cargo_type);
			SetDParam(1, u->cargo_cap);
			SetDParam(2, u->Next()->cargo_type);
			SetDParam(3, u->Next()->cargo_cap);
			SetDParam(4, GetCargoSubtypeText(u));
			DrawString(left, right, y + 10, (u->Next()->cargo_cap != 0) ? STR_VEHICLE_INFO_CAPACITY_CAPACITY : STR_VEHICLE_INFO_CAPACITY);
		}

		if (u->cargo_cap != 0) {
			uint cargo_count = u->cargo.Count();

			y_offset += 11;
			if (cargo_count != 0) {
				/* Cargo names (fix pluralness) */
				SetDParam(0, u->cargo_type);
				SetDParam(1, cargo_count);
				SetDParam(2, u->cargo.Source());
				DrawString(left, right, y + 21 + y_offset, STR_VEHICLE_DETAILS_CARGO_FROM);
				feeder_share += u->cargo.FeederShare();
			}
		}
	}

	SetDParam(0, feeder_share);
	DrawString(left, right, y + 33 + y_offset, STR_FEEDER_CARGO_VALUE);
}


void DrawAircraftImage(const Vehicle *v, int x, int y, VehicleID selection)
{
	SpriteID pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
	DrawSprite(v->GetImage(DIR_W), pal, x + 25, y + 10);
	if (v->subtype == AIR_HELICOPTER) {
		const Aircraft *a = (const Aircraft *)v;
		SpriteID rotor_sprite = GetCustomRotorSprite(a, true);
		if (rotor_sprite == 0) rotor_sprite = SPR_ROTOR_STOPPED;
		DrawSprite(rotor_sprite, PAL_NONE, x + 25, y + 5);
	}
	if (v->index == selection) {
		DrawFrameRect(x - 1, y - 1, x + 58, y + 21, COLOUR_WHITE, FR_BORDERONLY);
	}
}

/**
 * This is the Callback method after the construction attempt of an aircraft
 * @param success indicates completion (or not) of the operation
 * @param tile of depot where aircraft is built
 * @param p1 unused
 * @param p2 unused
 */
void CcBuildAircraft(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		const Vehicle *v = Vehicle::Get(_new_vehicle_id);

		if (v->tile == _backup_orders_tile) {
			_backup_orders_tile = 0;
			RestoreVehicleOrders(v);
		}
		ShowVehicleViewWindow(v);
	}
}
