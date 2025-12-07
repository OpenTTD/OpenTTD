/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file zoom_type.h Types related to zooming in and out. */

#ifndef ZOOM_TYPE_H
#define ZOOM_TYPE_H

#include "core/enum_type.hpp"

/**
 * All zoom levels we know.
 *
 * The underlying type is signed so subtract-and-Clamp works without need for casting.
 */
enum class ZoomLevel : int8_t {
	/* Our possible zoom-levels */
	Begin = 0, ///< Begin for iteration.
	Min = Begin, ///< Minimum zoom level.
	In4x = Begin, ///< Zoomed 4 times in.
	In2x, ///< Zoomed 2 times in.
	Normal, ///< The normal zoom level.
	Out2x, ///< Zoomed 2 times out.
	Out4x, ///< Zoomed 4 times out.
	Out8x, ///< Zoomed 8 times out.
	Max = Out8x, ///< Maximum zoom level.
	End, ///< End for iteration.

	/* Here we define in which zoom viewports are */
	Viewport = Normal, ///< Default zoom level for viewports.
	News = Normal, ///< Default zoom level for the news messages.
	Industry = Out2x, ///< Default zoom level for the industry view.
	Town = Normal, ///< Default zoom level for the town view.
	Aircraft = Normal, ///< Default zoom level for the aircraft view.
	Ship = Normal, ///< Default zoom level for the ship view.
	Train = Normal, ///< Default zoom level for the train view.
	RoadVehicle = Normal, ///< Default zoom level for the road vehicle view.
	WorldScreenshot = Normal, ///< Default zoom level for the world screen shot.

	Detail = Out2x, ///< All zoom levels below or equal to this will result in details on the screen, like road-work, ...
	TextEffect = Out2x, ///< All zoom levels above this will not show text effects.
};
DECLARE_INCREMENT_DECREMENT_OPERATORS(ZoomLevel)
DECLARE_ENUM_AS_SEQUENTIAL(ZoomLevel)
using ZoomLevels = EnumBitSet<ZoomLevel, uint8_t>;

static const uint ZOOM_BASE_SHIFT = to_underlying(ZoomLevel::Normal);
static uint const ZOOM_BASE = 1U << ZOOM_BASE_SHIFT;

extern int _gui_scale;
extern int _gui_scale_cfg;

extern ZoomLevel _gui_zoom;
extern ZoomLevel _font_zoom;

static const int MIN_INTERFACE_SCALE = 100;
static const int MAX_INTERFACE_SCALE = 500;

#endif /* ZOOM_TYPE_H */
