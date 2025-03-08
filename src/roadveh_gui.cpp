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
	Money feeder_share = 0;

	DrawString(r.left, r.right, y, GetString(STR_VEHICLE_INFO_BUILT_VALUE, PackEngineNameDParam(v->engine_type, EngineNameContext::VehicleDetails), v->build_year, v->value));
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
		std::string_view list_separator = GetListSeparator();

		bool first = true;
		for (const CargoSpec *cs : _sorted_cargo_specs) {
			CargoType cargo_type = cs->Index();
			if (max_cargo[cargo_type] > 0) {
				if (!first) capacity += list_separator;

				auto params = MakeParameters(cargo_type, max_cargo[cargo_type]);
				AppendStringWithArgsInPlace(capacity, STR_JUST_CARGO, params);

				if (subtype_text[cargo_type] != STR_NULL) {
					AppendStringInPlace(capacity, subtype_text[cargo_type]);
				}

				first = false;
			}
		}

		DrawString(r.left, r.right, y, capacity, TC_BLUE);
		y += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;

		for (const Vehicle *u = v; u != nullptr; u = u->Next()) {
			if (u->cargo_cap == 0) continue;

			std::string str;
			if (u->cargo.StoredCount() > 0) {
				str = GetString(STR_VEHICLE_DETAILS_CARGO_FROM, u->cargo_type, u->cargo.StoredCount(), u->cargo.GetFirstStation());
				feeder_share += u->cargo.GetFeederShare();
			} else {
				str = GetString(STR_VEHICLE_DETAILS_CARGO_EMPTY);
			}
			DrawString(r.left, r.right, y, str);
			y += GetCharacterHeight(FS_NORMAL);
		}
		y += WidgetDimensions::scaled.vsep_normal;
	} else {
		DrawString(r.left, r.right, y, GetString(STR_VEHICLE_INFO_CAPACITY, v->cargo_type, v->cargo_cap, GetCargoSubtypeText(v)));
		y += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;

		std::string str;
		if (v->cargo.StoredCount() > 0) {
			str = GetString(STR_VEHICLE_DETAILS_CARGO_FROM, v->cargo_type, v->cargo.StoredCount(), v->cargo.GetFirstStation());
			feeder_share += v->cargo.GetFeederShare();
		} else {
			str = GetString(STR_VEHICLE_DETAILS_CARGO_EMPTY);
		}
		DrawString(r.left, r.right, y, str);
		y += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;
	}

	/* Draw Transfer credits text */
	DrawString(r.left, r.right, y, GetString(STR_VEHICLE_INFO_FEEDER_CARGO_VALUE, feeder_share));
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

	bool do_overlays = ShowCargoIconOverlay();
	/* List of overlays, only used if cargo icon overlays are enabled. */
	static std::vector<CargoIconOverlay> overlays;

	int px = rtl ? max_width + skip : -skip;
	int y = r.Height() / 2;
	for (; u != nullptr && (rtl ? px > 0 : px < max_width); u = u->Next())
	{
		Point offset;
		int width = u->GetDisplayImageWidth(&offset);

		if (rtl ? px + width > 0 : px - width < max_width) {
			PaletteID pal = u->vehstatus.Test(VehState::Crashed) ? PALETTE_CRASH : GetVehiclePalette(u);
			VehicleSpriteSeq seq;
			u->GetImage(dir, image_type, &seq);
			seq.Draw(px + (rtl ? -offset.x : offset.x), y + offset.y, pal, u->vehstatus.Test(VehState::Crashed));
		}

		if (do_overlays) AddCargoIconOverlay(overlays, px, width, u);
		px += rtl ? -width : width;
	}

	if (do_overlays) {
		DrawCargoIconOverlays(overlays, y);
		overlays.clear();
	}

	if (v->index == selection) {
		int height = ScaleSpriteTrad(12);
		Rect hr = {(rtl ? px : 0), 0, (rtl ? max_width : px) - 1, height - 1};
		DrawFrameRect(hr.Translate(r.left, CenterBounds(r.top, r.bottom, height)).Expand(WidgetDimensions::scaled.bevel), COLOUR_WHITE, FrameFlag::BorderOnly);
	}
}
