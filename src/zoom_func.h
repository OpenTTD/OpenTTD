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
 * Scale by zoom level, usually shift left (when zoom > ZOOM_LVL_NORMAL)
 * When shifting right, value is rounded up
 * @param value value to shift
 * @param zoom  zoom level to shift to
 * @return shifted value
 */
static inline int ScaleByZoom(int value, ZoomLevel zoom)
{
	return value << zoom;
}

/**
 * Scale by zoom level, usually shift right (when zoom > ZOOM_LVL_NORMAL)
 * When shifting right, value is rounded up
 * @param value value to shift
 * @param zoom  zoom level to shift to
 * @return shifted value
 */
static inline int UnScaleByZoom(int value, ZoomLevel zoom)
{
	return (value + (1 << zoom) - 1) >> zoom;
}

/**
 * Scale by zoom level, usually shift left (when zoom > ZOOM_LVL_NORMAL)
 * @param value value to shift
 * @param zoom  zoom level to shift to
 * @return shifted value
 */
static inline int ScaleByZoomLower(int value, ZoomLevel zoom)
{
	return value << zoom;
}

/**
 * Scale by zoom level, usually shift right (when zoom > ZOOM_LVL_NORMAL)
 * @param value value to shift
 * @param zoom  zoom level to shift to
 * @return shifted value
 */
static inline int UnScaleByZoomLower(int value, ZoomLevel zoom)
{
	return value >> zoom;
}

/**
 * Short-hand to apply GUI zoom level.
 * @param value Pixel amount at #ZOOM_LVL_BEGIN (full zoom in).
 * @return Pixel amount at #ZOOM_LVL_GUI (current interface size).
 */
static inline int UnScaleGUI(int value)
{
	return UnScaleByZoom(value, ZOOM_LVL_GUI);
}

/**
 * Scale traditional pixel dimensions to GUI zoom level.
 * @param value Pixel amount at #ZOOM_LVL_BASE (traditional "normal" interface size).
 * @return Pixel amount at #ZOOM_LVL_GUI (current interface size).
 */
static inline int ScaleGUITrad(int value)
{
	return UnScaleGUI(value * ZOOM_LVL_BASE);
}

/**
 * Short-hand to apply font zoom level.
 * @param value Pixel amount at #ZOOM_LVL_BEGIN (full zoom in).
 * @return Pixel amount at #ZOOM_LVL_FONT (current interface size).
 */
static inline int UnScaleFont(int value)
{
	return UnScaleByZoom(value, ZOOM_LVL_FONT);
}

/**
 * Scale traditional pixel dimensions to Font zoom level.
 * @param value Pixel amount at #ZOOM_LVL_BASE (traditional "normal" interface size).
 * @return Pixel amount at #ZOOM_LVL_FONT (current interface size).
 */
static inline int ScaleFontTrad(int value)
{
	return UnScaleFont(value * ZOOM_LVL_BASE);
}

#endif /* ZOOM_FUNC_H */
