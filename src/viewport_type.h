/* $Id$ */

/** @file viewport_type.h Types related to viewports. */

#ifndef VIEWPORT_TYPE_H
#define VIEWPORT_TYPE_H

#include "zoom_type.h"

/**
 * Data structure for viewport, display of a part of the world
 */
struct ViewPort {
	int left;    ///< Screen coordinate left egde of the viewport
	int top;     ///< Screen coordinate top edge of the viewport
	int width;   ///< Screen width of the viewport
	int height;  ///< Screen height of the viewport

	int virtual_left;    ///< Virtual left coordinate
	int virtual_top;     ///< Virtual top coordinate
	int virtual_width;   ///< width << zoom
	int virtual_height;  ///< height << zoom

	ZoomLevel zoom;
};

struct ViewportSign {
	int32 left;
	int32 top;
	byte width_1, width_2;
};

enum {
	ZOOM_IN   = 0,
	ZOOM_OUT  = 1,
	ZOOM_NONE = 2, // hack, used to update the button status
};

/**
 * Some values for constructing bounding boxes (BB). The Z positions under bridges are:
 * z=0..5  Everything that can be built under low bridges.
 * z=6     reserved, currently unused.
 * z=7     Z separator between bridge/tunnel and the things under/above it.
 */
enum {
	BB_HEIGHT_UNDER_BRIDGE = 6, ///< Everything that can be built under low bridges, must not exceed this Z height.
	BB_Z_SEPARATOR  = 7,        ///< Separates the bridge/tunnel from the things under/above it.
};

/** Viewport place method (type of highlighted area and placed objects) */
enum ViewportPlaceMethod {
	VPM_X_OR_Y          = 0, ///< drag in X or Y direction
	VPM_FIX_X           = 1, ///< drag only in X axis
	VPM_FIX_Y           = 2, ///< drag only in Y axis
	VPM_RAILDIRS        = 3, ///< all rail directions
	VPM_X_AND_Y         = 4, ///< area of land in X and Y directions
	VPM_X_AND_Y_LIMITED = 5, ///< area of land of limited size
	VPM_SIGNALDIRS      = 6, ///< similiar to VMP_RAILDIRS, but with different cursor
};

/** Drag and drop selection process, or, what to do with an area of land when
 * you've selected it. */
enum ViewportDragDropSelectionProcess {
	DDSP_DEMOLISH_AREA,
	DDSP_RAISE_AND_LEVEL_AREA,
	DDSP_LOWER_AND_LEVEL_AREA,
	DDSP_LEVEL_AREA,
	DDSP_CREATE_DESERT,
	DDSP_CREATE_ROCKS,
	DDSP_CREATE_WATER,
	DDSP_CREATE_RIVER,
	DDSP_PLANT_TREES,
	DDSP_BUILD_BRIDGE,

	/* Rail specific actions */
	DDSP_PLACE_RAIL_NE,
	DDSP_PLACE_RAIL_NW,
	DDSP_PLACE_AUTORAIL,
	DDSP_BUILD_SIGNALS,
	DDSP_BUILD_STATION,
	DDSP_REMOVE_STATION,
	DDSP_CONVERT_RAIL,

	/* Road specific actions */
	DDSP_PLACE_ROAD_X_DIR,
	DDSP_PLACE_ROAD_Y_DIR,
	DDSP_PLACE_AUTOROAD,
};

#endif /* VIEWPORT_TYPE_H */
