/* $Id$ */

/** @file zoom.hpp */

#ifndef ZOOM_HPP
#define ZOOM_HPP

enum ZoomLevel {
	/* Our possible zoom-levels */
	ZOOM_LVL_NORMAL = 0,
	ZOOM_LVL_OUT_2X,
	ZOOM_LVL_OUT_4X,
	ZOOM_LVL_OUT_8X,
	ZOOM_LVL_OUT_16X,
	ZOOM_LVL_END,

	/* Here we define in which zoom viewports are */
	ZOOM_LVL_VIEWPORT = ZOOM_LVL_NORMAL,
	ZOOM_LVL_NEWS     = ZOOM_LVL_NORMAL,
	ZOOM_LVL_INDUSTRY = ZOOM_LVL_OUT_2X,
	ZOOM_LVL_TOWN     = ZOOM_LVL_OUT_2X,
	ZOOM_LVL_AIRCRAFT = ZOOM_LVL_NORMAL,
	ZOOM_LVL_SHIP     = ZOOM_LVL_NORMAL,
	ZOOM_LVL_TRAIN    = ZOOM_LVL_NORMAL,
	ZOOM_LVL_ROADVEH  = ZOOM_LVL_NORMAL,
	ZOOM_LVL_WORLD_SCREENSHOT = ZOOM_LVL_NORMAL,

	ZOOM_LVL_DETAIL   = ZOOM_LVL_OUT_2X, //! All zoomlevels below or equal to this, will result in details on the screen, like road-work, ...

	ZOOM_LVL_MIN      = ZOOM_LVL_NORMAL,
	ZOOM_LVL_MAX      = ZOOM_LVL_OUT_16X,
};

extern ZoomLevel _saved_scrollpos_zoom;

static inline int ScaleByZoom(int value, ZoomLevel zoom)
{
	if (zoom == ZOOM_LVL_NORMAL) return value;
	int izoom = (int)zoom - (int)ZOOM_LVL_NORMAL;
	return (zoom > ZOOM_LVL_NORMAL) ? value << izoom : value >> -izoom;
}

static inline int UnScaleByZoom(int value, ZoomLevel zoom)
{
	if (zoom == ZOOM_LVL_NORMAL) return value;
	int izoom = (int)zoom - (int)ZOOM_LVL_NORMAL;
	return (zoom > ZOOM_LVL_NORMAL) ? value >> izoom : value << -izoom;
}

#endif /* ZOOM_HPP */
