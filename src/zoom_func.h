/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file zoom_func.h Functions related to zooming. */

#ifndef ZOOM_FUNC_H
#define ZOOM_FUNC_H

#include "zoom_type.h"

/**
 * Scale by zoom level, usually shift left (when zoom > ZoomLevel::Min)
 * When shifting right, value is rounded up
 * @param value value to shift
 * @param zoom  zoom level to shift to
 * @return shifted value
 */
inline int ScaleByZoom(int value, ZoomLevel zoom)
{
	return value << to_underlying(zoom);
}

/**
 * Scale by zoom level, usually shift right (when zoom > ZoomLevel::Min)
 * When shifting right, value is rounded up
 * @param value value to shift
 * @param zoom  zoom level to shift to
 * @return shifted value
 */
inline int UnScaleByZoom(int value, ZoomLevel zoom)
{
	return (value + (1 << to_underlying(zoom)) - 1) >> to_underlying(zoom);
}

/**
 * Adjust by zoom level; zoom < 0 shifts right, zoom >= 0 shifts left
 * @param value value to shift
 * @param zoom zoom level to shift to
 * @return shifted value
 */
inline int AdjustByZoom(int value, int zoom)
{
	return zoom < 0 ? UnScaleByZoom(value, static_cast<ZoomLevel>(-zoom)) : ScaleByZoom(value, static_cast<ZoomLevel>(zoom));
}

/**
 * Scale by zoom level, usually shift left (when zoom > ZoomLevel::Min)
 * @param value value to shift
 * @param zoom  zoom level to shift to
 * @return shifted value
 */
inline int ScaleByZoomLower(int value, ZoomLevel zoom)
{
	return value << to_underlying(zoom);
}

/**
 * Scale by zoom level, usually shift right (when zoom > ZoomLevel::Min)
 * @param value value to shift
 * @param zoom  zoom level to shift to
 * @return shifted value
 */
inline int UnScaleByZoomLower(int value, ZoomLevel zoom)
{
	return value >> to_underlying(zoom);
}

/**
 * Short-hand to apply GUI zoom level.
 * @param value Pixel amount at #ZoomLevel::Min (full zoom in).
 * @return Pixel amount at current interface size.
 */
inline int UnScaleGUI(int value)
{
	return UnScaleByZoom(value, _gui_zoom);
}

/**
 * Scale zoom level relative to GUI zoom.
 * @param value zoom level to scale
 * @return scaled zoom level
 */
inline ZoomLevel ScaleZoomGUI(ZoomLevel value)
{
	return std::clamp(value + (_gui_zoom - ZoomLevel::Normal), ZoomLevel::Min, ZoomLevel::Max);
}

/**
 * UnScale zoom level relative to GUI zoom.
 * @param value zoom level to scale
 * @return un-scaled zoom level
 */
inline ZoomLevel UnScaleZoomGUI(ZoomLevel value)
{
	return std::clamp(value - (_gui_zoom - ZoomLevel::Normal), ZoomLevel::Min, ZoomLevel::Max);
}

/**
 * Scale traditional pixel dimensions to GUI zoom level, for drawing sprites.
 * @param value Pixel amount at #ZOOM_BASE (traditional "normal" interface size).
 * @return Pixel amount at current interface size.
 */
inline int ScaleSpriteTrad(int value)
{
	return UnScaleGUI(value * ZOOM_BASE);
}

/**
 * Scale traditional pixel dimensions to GUI zoom level.
 * @param value Pixel amount at #ZOOM_BASE (traditional "normal" interface size).
 * @return Pixel amount at current interface size.
 */
inline int ScaleGUITrad(int value)
{
	return value * _gui_scale / 100;
}

/**
 * Scale traditional pixel dimensions to font zoom level, for drawing sprite fonts.
 * @param value Pixel amount at #ZOOM_BASE (traditional "normal" interface size).
 * @return Pixel amount at _font_zoom (current interface size).
 */
inline int ScaleFontTrad(int value)
{
	return UnScaleByZoom(value * ZOOM_BASE, _font_zoom);
}

#endif /* ZOOM_FUNC_H */
