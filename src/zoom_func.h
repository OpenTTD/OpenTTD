/* $Id$ */

/** @file zoom.hpp */

#ifndef ZOOM_FUNC_H
#define ZOOM_FUNC_H

#include "zoom_type.h"

extern ZoomLevel _saved_scrollpos_zoom;

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
