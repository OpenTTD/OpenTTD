/* $Id$ */

/** @file roadveh_gui.cpp GUI for road vehicles. */

#include "stdafx.h"
#include "roadveh.h"
#include "window_gui.h"
#include "gfx_func.h"
#include "vehicle_gui.h"
#include "strings_func.h"
#include "vehicle_func.h"
#include "string_func.h"

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
void DrawRoadVehDetails(const Vehicle *v, int left, int right, int y)
{
	uint y_offset = RoadVehHasArticPart(v) ? 15 : 0;
	StringID str;
	Money feeder_share = 0;

	SetDParam(0, v->engine_type);
	SetDParam(1, v->build_year);
	SetDParam(2, v->value);
	DrawString(left, right, y + y_offset, STR_VEHICLE_INFO_BUILT_VALUE);

	if (RoadVehHasArticPart(v)) {
		CargoArray max_cargo;
		StringID subtype_text[NUM_CARGO];
		char capacity[512];

		memset(subtype_text, 0, sizeof(subtype_text));

		for (const Vehicle *u = v; u != NULL; u = u->Next()) {
			max_cargo[u->cargo_type] += u->cargo_cap;
			if (u->cargo_cap > 0) {
				StringID text = GetCargoSubtypeText(u);
				if (text != STR_EMPTY) subtype_text[u->cargo_type] = text;
			}
		}

		GetString(capacity, STR_ARTICULATED_RV_CAPACITY, lastof(capacity));

		bool first = true;
		for (CargoID i = 0; i < NUM_CARGO; i++) {
			if (max_cargo[i] > 0) {
				char buffer[128];

				SetDParam(0, i);
				SetDParam(1, max_cargo[i]);
				GetString(buffer, STR_BARE_CARGO, lastof(buffer));

				if (!first) strecat(capacity, ", ", lastof(capacity));
				strecat(capacity, buffer, lastof(capacity));

				if (subtype_text[i] != 0) {
					GetString(buffer, subtype_text[i], lastof(buffer));
					strecat(capacity, buffer, lastof(capacity));
				}

				first = false;
			}
		}

		DrawString(left, right, y + 10 + y_offset, capacity, TC_BLUE);

		for (const Vehicle *u = v; u != NULL; u = u->Next()) {
			if (u->cargo_cap == 0) continue;

			str = STR_VEHICLE_DETAILS_CARGO_EMPTY;
			if (!u->cargo.Empty()) {
				SetDParam(0, u->cargo_type);
				SetDParam(1, u->cargo.Count());
				SetDParam(2, u->cargo.Source());
				str = STR_VEHICLE_DETAILS_CARGO_FROM;
				feeder_share += u->cargo.FeederShare();
			}
			DrawString(left, right, y + 21 + y_offset, str);

			y_offset += 11;
		}

		y_offset -= 11;
	} else {
		SetDParam(0, v->cargo_type);
		SetDParam(1, v->cargo_cap);
		SetDParam(4, GetCargoSubtypeText(v));
		DrawString(left, right, y + 10 + y_offset, STR_VEHICLE_INFO_CAPACITY);

		str = STR_VEHICLE_DETAILS_CARGO_EMPTY;
		if (!v->cargo.Empty()) {
			SetDParam(0, v->cargo_type);
			SetDParam(1, v->cargo.Count());
			SetDParam(2, v->cargo.Source());
			str = STR_VEHICLE_DETAILS_CARGO_FROM;
			feeder_share += v->cargo.FeederShare();
		}
		DrawString(left, right, y + 21 + y_offset, str);
	}

	/* Draw Transfer credits text */
	SetDParam(0, feeder_share);
	DrawString(left, right, y + 33 + y_offset, STR_FEEDER_CARGO_VALUE);
}


static inline int RoadVehLengthToPixels(int length)
{
	return (length * 28) / 8;
}

void DrawRoadVehImage(const Vehicle *v, int x, int y, VehicleID selection, int count)
{
	/* Road vehicle lengths are measured in eighths of the standard length, so
	 * count is the number of standard vehicles that should be drawn. If it is
	 * 0, we draw enough vehicles for 10 standard vehicle lengths. */
	int max_length = (count == 0) ? 80 : count * 8;

	/* Width of highlight box */
	int highlight_w = 0;

	for (int dx = 0; v != NULL && dx < max_length ; v = v->Next()) {
		int width = RoadVehicle::From(v)->rcache.cached_veh_length;

		if (dx + width > 0 && dx <= max_length) {
			SpriteID pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
			DrawSprite(v->GetImage(DIR_W), pal, x + 14 + RoadVehLengthToPixels(dx), y + 6);

			if (v->index == selection) {
				/* Set the highlight position */
				highlight_w = RoadVehLengthToPixels(width);
			} else if (_cursor.vehchain && highlight_w != 0) {
				highlight_w += RoadVehLengthToPixels(width);
			}
		}

		dx += width;
	}

	if (highlight_w != 0) {
		DrawFrameRect(x - 1, y - 1, x - 1 + highlight_w, y + 12, COLOUR_WHITE, FR_BORDERONLY);
	}
}

void CcBuildRoadVeh(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	const Vehicle *v;

	if (!success) return;

	v = Vehicle::Get(_new_vehicle_id);
	if (v->tile == _backup_orders_tile) {
		_backup_orders_tile = 0;
		RestoreVehicleOrders(v);
	}
	ShowVehicleViewWindow(v);
}
