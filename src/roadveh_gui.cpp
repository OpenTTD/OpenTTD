/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file roadveh_gui.cpp GUI for road vehicles. */

#include "stdafx.h"
#include "core/backup_type.hpp"
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
 * @param r     the Rect to draw within
 */
void DrawRoadVehDetails(const Vehicle *v, const Rect &r)
{
	int y = r.top + (v->HasArticulatedPart() ? ScaleSpriteTrad(15) : 0); // Draw the first line below the sprite of an articulated RV instead of after it.
	StringID str;
	Money feeder_share = 0;

	SetDParam(0, PackEngineNameDParam(v->engine_type, EngineNameContext::VehicleDetails));
	SetDParam(1, v->build_year);
	SetDParam(2, v->value);
	DrawString(r.left, r.right, y, STR_VEHICLE_INFO_BUILT_VALUE);
	y += GetCharacterHeight(FS_NORMAL);

	if (v->HasArticulatedPart()) {
		CargoArray max_cargo{};
		std::array<StringID, NUM_CARGO> subtype_text{};

		for (const Vehicle *u = v; u != nullptr; u = u->Next()) {
			max_cargo[u->cargo_type] += u->cargo_cap;
			if (u->cargo_cap > 0) {
				StringID text = GetCargoSubtypeText(u);
				if (text != STR_EMPTY) subtype_text[u->cargo_type] = text;
			}
		}

		std::string capacity = GetString(STR_VEHICLE_DETAILS_TRAIN_ARTICULATED_RV_CAPACITY);

		bool first = true;
		for (const CargoSpec *cs : _sorted_cargo_specs) {
			CargoID cid = cs->Index();
			if (max_cargo[cid] > 0) {
				if (!first) capacity += ", ";

				SetDParam(0, cid);
				SetDParam(1, max_cargo[cid]);
				capacity += GetString(STR_JUST_CARGO);

				if (subtype_text[cid] != STR_NULL) {
					capacity += GetString(subtype_text[cid]);
				}

				first = false;
			}
		}

		DrawString(r.left, r.right, y, capacity, TC_BLUE);
		y += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;

		for (const Vehicle *u = v; u != nullptr; u = u->Next()) {
			if (u->cargo_cap == 0) continue;

			str = STR_VEHICLE_DETAILS_CARGO_EMPTY;
			if (u->cargo.StoredCount() > 0) {
				SetDParam(0, u->cargo_type);
				SetDParam(1, u->cargo.StoredCount());
				SetDParam(2, u->cargo.GetFirstStation());
				str = STR_VEHICLE_DETAILS_CARGO_FROM;
				feeder_share += u->cargo.GetFeederShare();
			}
			DrawString(r.left, r.right, y, str);
			y += GetCharacterHeight(FS_NORMAL);
		}
		y += WidgetDimensions::scaled.vsep_normal;
	} else {
		SetDParam(0, v->cargo_type);
		SetDParam(1, v->cargo_cap);
		SetDParam(4, GetCargoSubtypeText(v));
		DrawString(r.left, r.right, y, STR_VEHICLE_INFO_CAPACITY);
		y += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;

		str = STR_VEHICLE_DETAILS_CARGO_EMPTY;
		if (v->cargo.StoredCount() > 0) {
			SetDParam(0, v->cargo_type);
			SetDParam(1, v->cargo.StoredCount());
			SetDParam(2, v->cargo.GetFirstStation());
			str = STR_VEHICLE_DETAILS_CARGO_FROM;
			feeder_share += v->cargo.GetFeederShare();
		}
		DrawString(r.left, r.right, y, str);
		y += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;
	}

	/* Draw Transfer credits text */
	SetDParam(0, feeder_share);
	DrawString(r.left, r.right, y, STR_VEHICLE_INFO_FEEDER_CARGO_VALUE);
}

/**
 * Draws an image of a road vehicle chain
 * @param v         Front vehicle
 * @param r         Rect to draw at
 * @param selection Selected vehicle to draw a frame around
 * @param skip      Number of pixels to skip at the front (for scrolling)
 */
void DrawRoadVehImage(const Vehicle *v, const Rect &r, VehicleID selection, EngineImageType image_type, int skip)
{
	bool rtl = _current_text_dir == TD_RTL;
	Direction dir = rtl ? DIR_E : DIR_W;
	const RoadVehicle *u = RoadVehicle::From(v);

	DrawPixelInfo tmp_dpi;
	int max_width = r.Width();

	if (!FillDrawPixelInfo(&tmp_dpi, r)) return;

	AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);

	int px = rtl ? max_width + skip : -skip;
	int y = r.Height() / 2;
	for (; u != nullptr && (rtl ? px > 0 : px < max_width); u = u->Next())
	{
		Point offset;
		int width = u->GetDisplayImageWidth(&offset);

		if (rtl ? px + width > 0 : px - width < max_width) {
			PaletteID pal = (u->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(u);
			VehicleSpriteSeq seq;
			u->GetImage(dir, image_type, &seq);
			seq.Draw(px + (rtl ? -offset.x : offset.x), y + offset.y, pal, (u->vehstatus & VS_CRASHED) != 0);
		}

		px += rtl ? -width : width;
	}

	if (v->index == selection) {
		int height = ScaleSpriteTrad(12);
		Rect hr = {(rtl ? px : 0), 0, (rtl ? max_width : px) - 1, height - 1};
		DrawFrameRect(hr.Translate(r.left, CenterBounds(r.top, r.bottom, height)).Expand(WidgetDimensions::scaled.bevel), COLOUR_WHITE, FR_BORDERONLY);
	}
}
