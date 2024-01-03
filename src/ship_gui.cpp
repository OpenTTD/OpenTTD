/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ship_gui.cpp GUI for ships. */

#include "stdafx.h"
#include "vehicle_base.h"
#include "window_gui.h"
#include "gfx_func.h"
#include "vehicle_gui.h"
#include "strings_func.h"
#include "vehicle_func.h"
#include "spritecache.h"
#include "zoom_func.h"

#include "table/strings.h"

#include "safeguards.h"

/**
 * Draws an image of a ship
 * @param v         Front vehicle
 * @param r         Rect to draw at
 * @param selection Selected vehicle to draw a frame around
 */
void DrawShipImage(const Vehicle *v, const Rect &r, VehicleID selection, EngineImageType image_type)
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

	seq.Draw(x, y, GetVehiclePalette(v), false);

	if (v->index == selection) {
		x += x_offs;
		y += UnScaleGUI(rect.top);
		Rect hr = {x, y, x + width - 1, y + UnScaleGUI(rect.Height()) - 1};
		DrawFrameRect(hr.Expand(WidgetDimensions::scaled.bevel), COLOUR_WHITE, FR_BORDERONLY);
	}
}

/**
 * Draw the details for the given vehicle at the given position
 *
 * @param v     current vehicle
 * @param r     the Rect to draw within
 */
void DrawShipDetails(const Vehicle *v, const Rect &r)
{
	int y = r.top;

	SetDParam(0, PackEngineNameDParam(v->engine_type, EngineNameContext::VehicleDetails));
	SetDParam(1, v->build_year);
	SetDParam(2, v->value);
	DrawString(r.left, r.right, y, STR_VEHICLE_INFO_BUILT_VALUE);
	y += GetCharacterHeight(FS_NORMAL);

	SetDParam(0, v->cargo_type);
	SetDParam(1, v->cargo_cap);
	SetDParam(4, GetCargoSubtypeText(v));
	DrawString(r.left, r.right, y, STR_VEHICLE_INFO_CAPACITY);
	y += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;

	StringID str = STR_VEHICLE_DETAILS_CARGO_EMPTY;
	if (v->cargo.StoredCount() > 0) {
		SetDParam(0, v->cargo_type);
		SetDParam(1, v->cargo.StoredCount());
		SetDParam(2, v->cargo.GetFirstStation());
		str = STR_VEHICLE_DETAILS_CARGO_FROM;
	}
	DrawString(r.left, r.right, y, str);
	y += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;

	/* Draw Transfer credits text */
	SetDParam(0, v->cargo.GetFeederShare());
	DrawString(r.left, r.right, y, STR_VEHICLE_INFO_FEEDER_CARGO_VALUE);
}
