/* $Id$ */

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

#include "table/strings.h"

/**
 * Draws an image of a ship
 * @param v         Front vehicle
 * @param left      The minimum horizontal position
 * @param right     The maximum horizontal position
 * @param y         Vertical position to draw at
 * @param selection Selected vehicle to draw a frame around
 */
void DrawShipImage(const Vehicle *v, int left, int right, int y, VehicleID selection)
{
	bool rtl = _current_text_dir == TD_RTL;

	SpriteID sprite = v->GetImage(rtl ? DIR_E : DIR_W);
	const Sprite *real_sprite = GetSprite(sprite, ST_NORMAL);

	int x = rtl ? right - real_sprite->width - real_sprite->x_offs : left - real_sprite->x_offs;

	DrawSprite(sprite, GetVehiclePalette(v), x, y + 10);

	if (v->index == selection) {
		x += real_sprite->x_offs;
		y += real_sprite->y_offs + 10;
		DrawFrameRect(x - 1, y - 1, x + real_sprite->width + 1, y + real_sprite->height + 1, COLOUR_WHITE, FR_BORDERONLY);
	}
}

/**
 * Draw the details for the given vehicle at the given position
 *
 * @param v     current vehicle
 * @param left  The left most coordinate to draw
 * @param right The right most coordinate to draw
 * @param y     The y coordinate
 */
void DrawShipDetails(const Vehicle *v, int left, int right, int y)
{
	SetDParam(0, v->engine_type);
	SetDParam(1, v->build_year);
	SetDParam(2, v->value);
	DrawString(left, right, y, STR_VEHICLE_INFO_BUILT_VALUE, TC_FROMSTRING, SA_LEFT | SA_STRIP);

	SetDParam(0, v->cargo_type);
	SetDParam(1, v->cargo_cap);
	SetDParam(4, GetCargoSubtypeText(v));
	DrawString(left, right, y + FONT_HEIGHT_NORMAL, STR_VEHICLE_INFO_CAPACITY);

	StringID str = STR_VEHICLE_DETAILS_CARGO_EMPTY;
	if (!v->cargo.Empty()) {
		SetDParam(0, v->cargo_type);
		SetDParam(1, v->cargo.Count());
		SetDParam(2, v->cargo.Source());
		str = STR_VEHICLE_DETAILS_CARGO_FROM;
	}
	DrawString(left, right, y + 2 * FONT_HEIGHT_NORMAL + 1, str);

	/* Draw Transfer credits text */
	SetDParam(0, v->cargo.FeederShare());
	DrawString(left, right, y + 3 * FONT_HEIGHT_NORMAL + 3, STR_VEHICLE_INFO_FEEDER_CARGO_VALUE);
}
