/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ship_gui.cpp GUI for ships. */

#include "stdafx.h"
#include "vehicle_base.h"
#include "window_gui.h"
#include "gfx_func.h"
#include "vehicle_gui.h"
#include "strings_func.h"
#include "vehicle_func.h"
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
	int y = ScaleSpriteTrad(-1) + CentreBounds(r.top, r.bottom, 0);

	seq.Draw(x, y, GetVehiclePalette(v), false);
	if (v->cargo_cap > 0) DrawCargoIconOverlay(x, y, v->cargo_type);

	if (v->index == selection) {
		x += x_offs;
		y += UnScaleGUI(rect.top);
		Rect hr = {x, y, x + width - 1, y + UnScaleGUI(rect.Height()) - 1};
		DrawFrameRect(hr.Expand(WidgetDimensions::scaled.bevel), COLOUR_WHITE, FrameFlag::BorderOnly);
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

	DrawString(r.left, r.right, y, GetString(STR_VEHICLE_INFO_BUILT_VALUE, PackEngineNameDParam(v->engine_type, EngineNameContext::VehicleDetails), v->build_year, v->value));
	y += GetCharacterHeight(FS_NORMAL);

	DrawString(r.left, r.right, y, GetString(STR_VEHICLE_INFO_CAPACITY, v->cargo_type, v->cargo_cap, GetCargoSubtypeText(v)));
	y += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;

	if (v->cargo.StoredCount() > 0) {
		DrawString(r.left, r.right, y, GetString(STR_VEHICLE_DETAILS_CARGO_FROM, v->cargo_type, v->cargo.StoredCount(), v->cargo.GetFirstStation()));
	} else {
		DrawString(r.left, r.right, y, STR_VEHICLE_DETAILS_CARGO_EMPTY);
	}
	y += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;

	/* Draw Transfer credits text */
	DrawString(r.left, r.right, y, GetString(STR_VEHICLE_INFO_FEEDER_CARGO_VALUE, v->cargo.GetFeederShare()));
}
