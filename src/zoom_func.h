/* $Id$ */

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
	if (zoom == ZOOM_LVL_NORMAL) return value;
	int izoom = zoom - ZOOM_LVL_NORMAL;
	return (zoom > ZOOM_LVL_NORMAL) ? value << izoom : (value + (1 << -izoom) - 1) >> -izoom;
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
	if (zoom == ZOOM_LVL_NORMAL) return value;
	int izoom = zoom - ZOOM_LVL_NORMAL;
	return (zoom > ZOOM_LVL_NORMAL) ? (value + (1 << izoom) - 1) >> izoom : value << -izoom;
}

/**
 * Scale by zoom level, usually shift left (when zoom > ZOOM_LVL_NORMAL)
 * @param value value to shift
 * @param zoom  zoom level to shift to
 * @return shifted value
 */
static inline int ScaleByZoomLower(int value, ZoomLevel zoom)
{
	if (zoom == ZOOM_LVL_NORMAL) return value;
	int izoom = zoom - ZOOM_LVL_NORMAL;
	return (zoom > ZOOM_LVL_NORMAL) ? value << izoom : value >> -izoom;
}

/**
 * Scale by zoom level, usually shift right (when zoom > ZOOM_LVL_NORMAL)
 * @param value value to shift
 * @param zoom  zoom level to shift to
 * @return shifted value
 */
static inline int UnScaleByZoomLower(int value, ZoomLevel zoom)
{
	if (zoom == ZOOM_LVL_NORMAL) return value;
	int izoom = zoom - ZOOM_LVL_NORMAL;
	return (zoom > ZOOM_LVL_NORMAL) ? value >> izoom : value << -izoom;
}

#endif /* ZOOM_FUNC_H */
