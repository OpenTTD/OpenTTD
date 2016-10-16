/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file roadveh_gui.cpp GUI for road vehicles. */

#include "stdafx.h"
#include "roadveh.h"
#include "window_gui.h"
#include "strings_func.h"
#include "vehicle_func.h"
#include "string_func.h"
#include "zoom_func.h"

#include "table/strings.h"

#include "safeguards.h"

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
	uint y_offset = v->HasArticulatedPart() ? ScaleGUITrad(15) : 0; // Draw the first line below the sprite of an articulated RV instead of after it.
	StringID str;
	Money feeder_share = 0;

	SetDParam(0, v->engine_type);
	SetDParam(1, v->build_year);
	SetDParam(2, v->value);
	DrawString(left, right, y + y_offset, STR_VEHICLE_INFO_BUILT_VALUE);

	if (v->HasArticulatedPart()) {
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

		GetString(capacity, STR_VEHICLE_DETAILS_TRAIN_ARTICULATED_RV_CAPACITY, lastof(capacity));

		bool first = true;
		for (CargoID i = 0; i < NUM_CARGO; i++) {
			if (max_cargo[i] > 0) {
				char buffer[128];

				SetDParam(0, i);
				SetDParam(1, max_cargo[i]);
				GetString(buffer, STR_JUST_CARGO, lastof(buffer));

				if (!first) strecat(capacity, ", ", lastof(capacity));
				strecat(capacity, buffer, lastof(capacity));

				if (subtype_text[i] != 0) {
					GetString(buffer, subtype_text[i], lastof(buffer));
					strecat(capacity, buffer, lastof(capacity));
				}

				first = false;
			}
		}

		DrawString(left, right, y + FONT_HEIGHT_NORMAL + y_offset, capacity, TC_BLUE);

		for (const Vehicle *u = v; u != NULL; u = u->Next()) {
			if (u->cargo_cap == 0) continue;

			str = STR_VEHICLE_DETAILS_CARGO_EMPTY;
			if (u->cargo.StoredCount() > 0) {
				SetDParam(0, u->cargo_type);
				SetDParam(1, u->cargo.StoredCount());
				SetDParam(2, u->cargo.Source());
				str = STR_VEHICLE_DETAILS_CARGO_FROM;
				feeder_share += u->cargo.FeederShare();
			}
			DrawString(left, right, y + 2 * FONT_HEIGHT_NORMAL + 1 + y_offset, str);

			y_offset += FONT_HEIGHT_NORMAL + 1;
		}

		y_offset -= FONT_HEIGHT_NORMAL + 1;
	} else {
		SetDParam(0, v->cargo_type);
		SetDParam(1, v->cargo_cap);
		SetDParam(4, GetCargoSubtypeText(v));
		DrawString(left, right, y + FONT_HEIGHT_NORMAL + y_offset, STR_VEHICLE_INFO_CAPACITY);

		str = STR_VEHICLE_DETAILS_CARGO_EMPTY;
		if (v->cargo.StoredCount() > 0) {
			SetDParam(0, v->cargo_type);
			SetDParam(1, v->cargo.StoredCount());
			SetDParam(2, v->cargo.Source());
			str = STR_VEHICLE_DETAILS_CARGO_FROM;
			feeder_share += v->cargo.FeederShare();
		}
		DrawString(left, right, y + 2 * FONT_HEIGHT_NORMAL + 1 + y_offset, str);
	}

	/* Draw Transfer credits text */
	SetDParam(0, feeder_share);
	DrawString(left, right, y + 3 * FONT_HEIGHT_NORMAL + 3 + y_offset, STR_VEHICLE_INFO_FEEDER_CARGO_VALUE);
}

/**
 * Draws an image of a road vehicle chain
 * @param v         Front vehicle
 * @param left      The minimum horizontal position
 * @param right     The maximum horizontal position
 * @param y         Vertical position to draw at
 * @param selection Selected vehicle to draw a frame around
 * @param skip      Number of pixels to skip at the front (for scrolling)
 */
void DrawRoadVehImage(const Vehicle *v, int left, int right, int y, VehicleID selection, EngineImageType image_type, int skip)
{
	bool rtl = _current_text_dir == TD_RTL;
	Direction dir = rtl ? DIR_E : DIR_W;
	const RoadVehicle *u = RoadVehicle::From(v);

	DrawPixelInfo tmp_dpi, *old_dpi;
	int max_width = right - left + 1;

	if (!FillDrawPixelInfo(&tmp_dpi, left, y, max_width, ScaleGUITrad(14))) return;

	old_dpi = _cur_dpi;
	_cur_dpi = &tmp_dpi;

	int px = rtl ? max_width + skip : -skip;
	for (; u != NULL && (rtl ? px > 0 : px < max_width); u = u->Next()) {
		Point offset;
		int width = u->GetDisplayImageWidth(&offset);

		if (rtl ? px + width > 0 : px - width < max_width) {
			PaletteID pal = (u->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(u);
			VehicleSpriteSeq seq;
			u->GetImage(dir, image_type, &seq);
			seq.Draw(px + (rtl ? -offset.x : offset.x), ScaleGUITrad(6) + offset.y, pal, u->vehstatus & VS_CRASHED);
		}

		px += rtl ? -width : width;
	}

	if (v->index == selection) {
		DrawFrameRect((rtl ? px : 0), 0, (rtl ? max_width : px) - 1, ScaleGUITrad(13) - 1, COLOUR_WHITE, FR_BORDERONLY);
	}

	_cur_dpi = old_dpi;
}
