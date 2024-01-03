/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file aircraft_gui.cpp The GUI of aircraft. */

#include "stdafx.h"
#include "aircraft.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "strings_func.h"
#include "vehicle_func.h"
#include "window_gui.h"
#include "spritecache.h"
#include "zoom_func.h"

#include "table/strings.h"

#include "safeguards.h"

/**
 * Draw the details for the given vehicle at the given position
 *
 * @param v     current vehicle
 * @param r     the Rect to draw within
 */
void DrawAircraftDetails(const Aircraft *v, const Rect &r)
{
	Money feeder_share = 0;

	int y = r.top;
	for (const Aircraft *u = v; u != nullptr; u = u->Next()) {
		if (u->IsNormalAircraft()) {
			SetDParam(0, PackEngineNameDParam(u->engine_type, EngineNameContext::VehicleDetails));
			SetDParam(1, u->build_year);
			SetDParam(2, u->value);
			DrawString(r.left, r.right, y, STR_VEHICLE_INFO_BUILT_VALUE);
			y += GetCharacterHeight(FS_NORMAL);

			SetDParam(0, u->cargo_type);
			SetDParam(1, u->cargo_cap);
			SetDParam(2, u->Next()->cargo_type);
			SetDParam(3, u->Next()->cargo_cap);
			SetDParam(4, GetCargoSubtypeText(u));
			DrawString(r.left, r.right, y, (u->Next()->cargo_cap != 0) ? STR_VEHICLE_INFO_CAPACITY_CAPACITY : STR_VEHICLE_INFO_CAPACITY);
			y += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;
		}

		if (u->cargo_cap != 0) {
			uint cargo_count = u->cargo.StoredCount();

			if (cargo_count != 0) {
				/* Cargo names (fix pluralness) */
				SetDParam(0, u->cargo_type);
				SetDParam(1, cargo_count);
				SetDParam(2, u->cargo.GetFirstStation());
				DrawString(r.left, r.right, y, STR_VEHICLE_DETAILS_CARGO_FROM);
				y += GetCharacterHeight(FS_NORMAL);
				feeder_share += u->cargo.GetFeederShare();
			}
		}
	}

	y += WidgetDimensions::scaled.vsep_normal;
	SetDParam(0, feeder_share);
	DrawString(r.left, r.right, y, STR_VEHICLE_INFO_FEEDER_CARGO_VALUE);
}


/**
 * Draws an image of an aircraft
 * @param v         Front vehicle
 * @param r         Rect to draw at
 * @param selection Selected vehicle to draw a frame around
 */
void DrawAircraftImage(const Vehicle *v, const Rect &r, VehicleID selection, EngineImageType image_type)
{
	bool rtl = _current_text_dir == TD_RTL;

	VehicleSpriteSeq seq;
	v->GetImage(rtl ? DIR_E : DIR_W, image_type, &seq);

	Rect rect;
	seq.GetBounds(&rect);

	int width = UnScaleGUI(rect.Width());
	int x_offs = UnScaleGUI(rect.left);
	int x = rtl ? r.right - width - x_offs : r.left - x_offs;
	/* This magic -1 offset is related to the sprite_y_offsets in build_vehicle_gui.cpp */
	int y = ScaleSpriteTrad(-1) + CenterBounds(r.top, r.bottom, 0);
	bool helicopter = v->subtype == AIR_HELICOPTER;

	int heli_offs = 0;

	PaletteID pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
	seq.Draw(x, y, pal, (v->vehstatus & VS_CRASHED) != 0);
	if (helicopter) {
		const Aircraft *a = Aircraft::From(v);
		VehicleSpriteSeq rotor_seq;
		GetCustomRotorSprite(a, image_type, &rotor_seq);
		if (!rotor_seq.IsValid()) rotor_seq.Set(SPR_ROTOR_STOPPED);
		heli_offs = ScaleSpriteTrad(5);
		rotor_seq.Draw(x, y - heli_offs, PAL_NONE, false);
	}
	if (v->index == selection) {
		x += x_offs;
		y += UnScaleGUI(rect.top) - heli_offs;
		Rect hr = {x, y, x + width - 1, y + UnScaleGUI(rect.Height()) + heli_offs - 1};
		DrawFrameRect(hr.Expand(WidgetDimensions::scaled.bevel), COLOUR_WHITE, FR_BORDERONLY);
	}
}
