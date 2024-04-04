/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file zoom_func.h Functions related to zooming. */

#ifndef ZOOM_FUNC_H
#define ZOOM_FUNC_H

#include "zoom_type.h"

/**
 * Scale by zoom level, usually shift left (when zoom > ZOOM_LVL_MIN)
 * When shifting right, value is rounded up
 * @param value value to shift
 * @param zoom  zoom level to shift to
 * @return shifted value
 */
inline int ScaleByZoom(int value, ZoomLevel zoom)
{
	return value << zoom;
}

/**
 * Scale by zoom level, usually shift right (when zoom > ZOOM_LVL_MIN)
 * When shifting right, value is rounded up
 * @param value value to shift
 * @param zoom  zoom level to shift to
 * @return shifted value
 */
inline int UnScaleByZoom(int value, ZoomLevel zoom)
{
	return (value + (1 << zoom) - 1) >> zoom;
}

/**
 * Adjust by zoom level; zoom < 0 shifts right, zoom >= 0 shifts left
 * @param value value to shift
 * @param zoom zoom level to shift to
 * @return shifted value
 */
inline int AdjustByZoom(int value, int zoom)
{
	return zoom < 0 ? UnScaleByZoom(value, ZoomLevel(-zoom)) : ScaleByZoom(value, ZoomLevel(zoom));
}

/**
 * Scale by zoom level, usually shift left (when zoom > ZOOM_LVL_MIN)
 * @param value value to shift
 * @param zoom  zoom level to shift to
 * @return shifted value
 */
inline int ScaleByZoomLower(int value, ZoomLevel zoom)
{
	return value << zoom;
}

/**
 * Scale by zoom level, usually shift right (when zoom > ZOOM_LVL_MIN)
 * @param value value to shift
 * @param zoom  zoom level to shift to
 * @return shifted value
 */
inline int UnScaleByZoomLower(int value, ZoomLevel zoom)
{
	return value >> zoom;
}

/**
 * Short-hand to apply GUI zoom level.
 * @param value Pixel amount at #ZOOM_LVL_MIN (full zoom in).
 * @return Pixel amount at #ZOOM_LVL_GUI (current interface size).
 */
inline int UnScaleGUI(int value)
{
	return UnScaleByZoom(value, ZOOM_LVL_GUI);
}

/**
 * Scale zoom level relative to GUI zoom.
 * @param value zoom level to scale
 * @return scaled zoom level
 */
inline ZoomLevel ScaleZoomGUI(ZoomLevel value)
{
	return std::clamp(ZoomLevel(value + (ZOOM_LVL_GUI - ZOOM_LVL_NORMAL)), ZOOM_LVL_MIN, ZOOM_LVL_MAX);
}

/**
 * UnScale zoom level relative to GUI zoom.
 * @param value zoom level to scale
 * @return un-scaled zoom level
 */
inline ZoomLevel UnScaleZoomGUI(ZoomLevel value)
{
	return std::clamp(ZoomLevel(value - (ZOOM_LVL_GUI - ZOOM_LVL_NORMAL)), ZOOM_LVL_MIN, ZOOM_LVL_MAX);
}

/**
 * Scale traditional pixel dimensions to GUI zoom level, for drawing sprites.
 * @param value Pixel amount at #ZOOM_BASE (traditional "normal" interface size).
 * @return Pixel amount at #ZOOM_LVL_GUI (current interface size).
 */
inline int ScaleSpriteTrad(int value)
{
	return UnScaleGUI(value * ZOOM_BASE);
}

/**
 * Scale traditional pixel dimensions to GUI zoom level.
 * @param value Pixel amount at #ZOOM_BASE (traditional "normal" interface size).
 * @return Pixel amount at #ZOOM_LVL_GUI (current interface size).
 */
inline int ScaleGUITrad(int value)
{
	return value * _gui_scale / 100;
}

#endif /* ZOOM_FUNC_H */
