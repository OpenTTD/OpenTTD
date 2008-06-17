/* $Id$ */

/** @file zoom_type.h Types related to zooming in and out. */

#ifndef ZOOM_TYPE_H
#define ZOOM_TYPE_H

#include "core/enum_type.hpp"

enum ZoomLevel {
	/* Our possible zoom-levels */
	ZOOM_LVL_BEGIN  = 0,
	ZOOM_LVL_NORMAL = 0,
	ZOOM_LVL_OUT_2X,
	ZOOM_LVL_OUT_4X,
	ZOOM_LVL_OUT_8X,
	ZOOM_LVL_END,

	/* Number of zoom levels */
	ZOOM_LVL_COUNT = ZOOM_LVL_END - ZOOM_LVL_BEGIN,

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

	ZOOM_LVL_DETAIL   = ZOOM_LVL_OUT_2X, ///< All zoomlevels below or equal to this, will result in details on the screen, like road-work, ...

	ZOOM_LVL_MIN      = ZOOM_LVL_NORMAL,
	ZOOM_LVL_MAX      = ZOOM_LVL_OUT_8X,
};
DECLARE_POSTFIX_INCREMENT(ZoomLevel)

#endif /* ZOOM_TYPE_H */
