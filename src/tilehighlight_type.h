/* $Id$ */

/** @file tilehighlight_type.h Types related to highlighting tiles. */

#ifndef TILEHIGHLIGHT_TYPE_H
#define TILEHIGHLIGHT_TYPE_H

#include "core/geometry_type.hpp"
#include "zoom_type.h"
#include "window_type.h"
#include "tile_type.h"

/** Highlighting draw styles */
enum HighLightStyle {
	HT_NONE      = 0x00, ///< default
	HT_RECT      = 0x10, ///< rectangle (stations, depots, ...)
	HT_POINT     = 0x20, ///< point (lower land, raise land, level land, ...)
	HT_SPECIAL   = 0x30, ///< special mode used for highlighting while dragging (and for tunnels/docks)
	HT_DRAG      = 0x40, ///< dragging items in the depot windows
	HT_LINE      = 0x08, ///< used for autorail highlighting (longer streches), lower bits: direction
	HT_RAIL      = 0x80, ///< autorail (one piece), lower bits: direction
	HT_DRAG_MASK = 0xF8, ///< masks the drag-type

	/* lower bits (used with HT_LINE and HT_RAIL):
	 * (see ASCII art in autorail.h for a visual interpretation) */
	HT_DIR_X  = 0,    ///< X direction
	HT_DIR_Y  = 1,    ///< Y direction
	HT_DIR_HU = 2,    ///< horizontal upper
	HT_DIR_HL = 3,    ///< horizontal lower
	HT_DIR_VL = 4,    ///< vertical left
	HT_DIR_VR = 5,    ///< vertical right
	HT_DIR_END,       ///< end marker
	HT_DIR_MASK = 0x7 ///< masks the drag-direction
};
DECLARE_ENUM_AS_BIT_SET(HighLightStyle);


struct TileHighlightData {
	Point size;
	Point outersize;
	Point pos;
	Point offs;

	Point new_pos;
	Point new_size;
	Point new_outersize;

	Point selend, selstart;

	byte dirty;
	byte sizelimit;

	HighLightStyle drawstyle;      // lower bits 0-3 are reserved for detailed highlight information information
	HighLightStyle new_drawstyle;  // only used in UpdateTileSelection() to as a buffer to compare if there was a change between old and new
	HighLightStyle next_drawstyle; // queued, but not yet drawn style

	HighLightStyle place_mode;
	bool make_square_red;
	WindowClass window_class;
	WindowNumber window_number;

	ViewportPlaceMethod select_method;
	ViewportDragDropSelectionProcess select_proc;

	TileIndex redsq;
};

#endif /* TILEHIGHLIGHT_TYPE_H */
