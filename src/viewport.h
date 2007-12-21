/* $Id$ */

/** @file viewport.h */

#ifndef VIEWPORT_H
#define VIEWPORT_H

#include "zoom.hpp"
#include "window_type.h"
#include "vehicle_type.h"
#include "gfx.h"

struct ViewPort {
	int left,top;                       // screen coordinates for the viewport
	int width, height;                  // screen width/height for the viewport

	int virtual_left, virtual_top;      // virtual coordinates
	int virtual_width, virtual_height;  // these are just width << zoom, height << zoom

	ZoomLevel zoom;
};

void SetSelectionRed(bool);

/* viewport.cpp */
void InitViewports();
void DeleteWindowViewport(Window *w);
void AssignWindowViewport(Window *w, int x, int y,
	int width, int height, uint32 follow_flags, ZoomLevel zoom);
ViewPort *IsPtInWindowViewport(const Window *w, int x, int y);
Point GetTileBelowCursor();
void UpdateViewportPosition(Window *w);

enum {
	ZOOM_IN   = 0,
	ZOOM_OUT  = 1,
	ZOOM_NONE = 2, // hack, used to update the button status
};

bool DoZoomInOutWindow(int how, Window *w);
void ZoomInOrOutToCursorWindow(bool in, Window * w);
Point GetTileZoomCenterWindow(bool in, Window * w);
void HandleZoomMessage(Window *w, const ViewPort *vp, byte widget_zoom_in, byte widget_zoom_out);

static inline void MaxZoomInOut(int how, Window *w)
{
	while (DoZoomInOutWindow(how, w)) {};
}

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

void OffsetGroundSprite(int x, int y);

void DrawGroundSprite(SpriteID image, SpriteID pal, const SubSprite *sub = NULL);
void DrawGroundSpriteAt(SpriteID image, SpriteID pal, int32 x, int32 y, byte z, const SubSprite *sub = NULL);
void AddSortableSpriteToDraw(SpriteID image, SpriteID pal, int x, int y, int w, int h, int dz, int z, bool transparent = false, int bb_offset_x = 0, int bb_offset_y = 0, int bb_offset_z = 0, const SubSprite *sub = NULL);
void *AddStringToDraw(int x, int y, StringID string, uint64 params_1, uint64 params_2);
void AddChildSpriteScreen(SpriteID image, SpriteID pal, int x, int y, bool transparent = false, const SubSprite *sub = NULL);


void StartSpriteCombine();
void EndSpriteCombine();

void HandleViewportClicked(const ViewPort *vp, int x, int y);
void PlaceObject();
void SetRedErrorSquare(TileIndex tile);
void SetTileSelectSize(int w, int h);
void SetTileSelectBigSize(int ox, int oy, int sx, int sy);

Vehicle *CheckMouseOverVehicle();

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

/** Viewport highlight mode (for highlighting tiles below cursor) */
enum ViewportHighlightMode {
	VHM_NONE    = 0, ///< default
	VHM_RECT    = 1, ///< rectangle (stations, depots, ...)
	VHM_POINT   = 2, ///< point (lower land, raise land, level land, ...)
	VHM_SPECIAL = 3, ///< special mode used for highlighting while dragging (and for tunnels/docks)
	VHM_DRAG    = 4, ///< dragging items in the depot windows
	VHM_RAIL    = 5, ///< rail pieces
};

void VpSelectTilesWithMethod(int x, int y, ViewportPlaceMethod method);
void VpStartPlaceSizing(TileIndex tile, ViewportPlaceMethod method, byte process);
void VpSetPresizeRange(uint from, uint to);
void VpSetPlaceSizingLimit(int limit);

/* highlighting draw styles */
typedef byte HighLightStyle;
enum HighLightStyles {
	HT_NONE   = 0x00,
	HT_RECT   = 0x80,
	HT_POINT  = 0x40,
	HT_LINE   = 0x20,    ///< used for autorail highlighting (longer streches)
	                     ///< (uses lower bits to indicate direction)
	HT_RAIL   = 0x10,    ///< autorail (one piece)
	                     ///< (uses lower bits to indicate direction)
	HT_DRAG_MASK = 0xF0, ///< masks the drag-type

	/* lower bits (used with HT_LINE and HT_RAIL):
	 * (see ASCII art in autorail.h for a visual interpretation) */
	HT_DIR_X  = 0,    ///< X direction
	HT_DIR_Y  = 1,    ///< Y direction
	HT_DIR_HU = 2,    ///< horizontal upper
	HT_DIR_HL = 3,    ///< horizontal lower
	HT_DIR_VL = 4,    ///< vertical left
	HT_DIR_VR = 5,    ///< vertical right
	HT_DIR_MASK = 0x7 ///< masks the drag-direction
};

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

	byte drawstyle;      // lower bits 0-3 are reserved for detailed highlight information information
	byte new_drawstyle;  // only used in UpdateTileSelection() to as a buffer to compare if there was a change between old and new
	byte next_drawstyle; // queued, but not yet drawn style

	ViewportHighlightMode place_mode;
	bool make_square_red;
	WindowClass window_class;
	WindowNumber window_number;

	ViewportPlaceMethod select_method;
	byte select_proc;

	TileIndex redsq;
};


/* common button handler */
bool HandlePlacePushButton(Window *w, int widget, CursorID cursor, ViewportHighlightMode mode, PlaceProc *placeproc);

VARDEF Point _tile_fract_coords;

extern TileHighlightData _thd;


void ViewportDoDraw(const ViewPort *vp, int left, int top, int right, int bottom);

#endif /* VIEWPORT_H */
