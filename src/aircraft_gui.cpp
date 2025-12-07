/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file aircraft_gui.cpp The GUI of aircraft. */

#include "stdafx.h"
#include "aircraft.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "strings_func.h"
#include "vehicle_func.h"
#include "window_gui.h"
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
			DrawString(r.left, r.right, y, GetString(STR_VEHICLE_INFO_BUILT_VALUE, PackEngineNameDParam(u->engine_type, EngineNameContext::VehicleDetails), u->build_year, u->value));
			y += GetCharacterHeight(FS_NORMAL);

			if (u->Next()->cargo_cap != 0) {
				DrawString(r.left, r.right, y, GetString(STR_VEHICLE_INFO_CAPACITY_CAPACITY, u->cargo_type, u->cargo_cap, u->Next()->cargo_type, u->Next()->cargo_cap, GetCargoSubtypeText(u)));
			} else {
				DrawString(r.left, r.right, y, GetString(STR_VEHICLE_INFO_CAPACITY, u->cargo_type, u->cargo_cap, GetCargoSubtypeText(u)));
			}
			y += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;
		}

		if (u->cargo_cap != 0) {
			uint cargo_count = u->cargo.StoredCount();

			if (cargo_count != 0) {
				/* Cargo names (fix pluralness) */
				DrawString(r.left, r.right, y, GetString(STR_VEHICLE_DETAILS_CARGO_FROM, u->cargo_type, cargo_count, u->cargo.GetFirstStation()));
				y += GetCharacterHeight(FS_NORMAL);
				feeder_share += u->cargo.GetFeederShare();
			}
		}
	}

	y += WidgetDimensions::scaled.vsep_normal;
	DrawString(r.left, r.right, y, GetString(STR_VEHICLE_INFO_FEEDER_CARGO_VALUE, feeder_share));
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
	int y = ScaleSpriteTrad(-1) + CentreBounds(r.top, r.bottom, 0);
	bool helicopter = v->subtype == AIR_HELICOPTER;

	int heli_offs = 0;

	PaletteID pal = v->vehstatus.Test(VehState::Crashed) ? PALETTE_CRASH : GetVehiclePalette(v);
	seq.Draw(x, y, pal, v->vehstatus.Test(VehState::Crashed));

	/* Aircraft can store cargo in their shadow, show this if present. */
	const Vehicle *u = v->Next();
	assert(u != nullptr);
	int dx = 0;
	if (u->cargo_cap > 0 && u->cargo_type != v->cargo_type) {
		dx = GetLargestCargoIconSize().width / 2;
		DrawCargoIconOverlay(x + dx, y, u->cargo_type);
	}
	if (v->cargo_cap > 0) DrawCargoIconOverlay(x - dx, y, v->cargo_type);

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
		DrawFrameRect(hr.Expand(WidgetDimensions::scaled.bevel), COLOUR_WHITE, FrameFlag::BorderOnly);
	}
}
