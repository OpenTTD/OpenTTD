/* $Id$ */

#ifndef VIEWPORT_H
#define VIEWPORT_H

struct ViewPort {
	int left,top;                       // screen coordinates for the viewport
	int width, height;                  // screen width/height for the viewport

	int virtual_left, virtual_top;      // virtual coordinates
	int virtual_width, virtual_height;  // these are just width << zoom, height << zoom

	byte zoom;
};

void SetSelectionRed(bool);

/* viewport.c */
void InitViewports(void);
void DeleteWindowViewport(Window *w);
void AssignWindowViewport(Window *w, int x, int y,
	int width, int height, uint32 follow_flags, byte zoom);
ViewPort *IsPtInWindowViewport(const Window *w, int x, int y);
Point GetTileBelowCursor(void);
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
	while (DoZoomInOutWindow(how, w) ) {};
}

void OffsetGroundSprite(int x, int y);

void DrawGroundSprite(uint32 image);
void DrawGroundSpriteAt(uint32 image, int32 x, int32 y, byte z);
void AddSortableSpriteToDraw(uint32 image, int x, int y, int w, int h, byte dz, byte z);
void *AddStringToDraw(int x, int y, StringID string, uint32 params_1, uint32 params_2);
void AddChildSpriteScreen(uint32 image, int x, int y);


void StartSpriteCombine(void);
void EndSpriteCombine(void);

void HandleViewportClicked(const ViewPort *vp, int x, int y);
void PlaceObject(void);
void SetRedErrorSquare(TileIndex tile);
void SetTileSelectSize(int w, int h);
void SetTileSelectBigSize(int ox, int oy, int sx, int sy);

void VpStartPlaceSizing(TileIndex tile, int user);
void VpSetPresizeRange(uint from, uint to);
void VpSetPlaceSizingLimit(int limit);

Vehicle *CheckMouseOverVehicle(void);

enum {
	VPM_X_OR_Y          = 0,
	VPM_FIX_X           = 1,
	VPM_FIX_Y           = 2,
	VPM_RAILDIRS        = 3,
	VPM_X_AND_Y         = 4,
	VPM_X_AND_Y_LIMITED = 5,
	VPM_SIGNALDIRS      = 6
};

// viewport highlight mode (for highlighting tiles below cursor)
enum {
	VHM_NONE    = 0, // default
	VHM_RECT    = 1, // rectangle (stations, depots, ...)
	VHM_POINT   = 2, // point (lower land, raise land, level land, ...)
	VHM_SPECIAL = 3, // special mode used for highlighting while dragging (and for tunnels/docks)
	VHM_DRAG    = 4, // dragging items in the depot windows
	VHM_RAIL    = 5, // rail pieces
};

void VpSelectTilesWithMethod(int x, int y, int method);

// highlighting draw styles
typedef byte HighLightStyle;
enum HighLightStyles {
	HT_NONE   = 0x00,
	HT_RECT   = 0x80,
	HT_POINT  = 0x40,
	HT_LINE   = 0x20, /* used for autorail highlighting (longer streches)
	                   * (uses lower bits to indicate direction) */
	HT_RAIL   = 0x10, /* autorail (one piece)
	                  * (uses lower bits to indicate direction) */
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

typedef struct TileHighlightData {
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

	byte place_mode;
	bool make_square_red;
	WindowClass window_class;
	WindowNumber window_number;

	int userdata;
	TileIndex redsq;
} TileHighlightData;


// common button handler
bool HandlePlacePushButton(Window *w, int widget, CursorID cursor, int mode, PlaceProc *placeproc);

VARDEF Point _tile_fract_coords;

extern TileHighlightData _thd;


void ViewportDoDraw(const ViewPort *vp, int left, int top, int right, int bottom);

#endif /* VIEWPORT_H */
