#ifndef VIEWPORT_H
#define VIEWPORT_H

struct ViewPort {
	int left,top;												// screen coordinates for the viewport
	int width, height;									// screen width/height for the viewport

	int virtual_left, virtual_top;			// virtual coordinates
	int virtual_width, virtual_height;	// these are just width << zoom, height << zoom

	byte zoom;
};

/* viewport.c */
Point MapXYZToViewport(ViewPort *vp, uint x, uint y, uint z);
void AssignWindowViewport(Window *w, int x, int y,
	int width, int height, uint32 follow_flags, byte zoom);
void SetViewportPosition(Window *w, int x, int y);
ViewPort *IsPtInWindowViewport(Window *w, int x, int y);
Point GetTileBelowCursor();
void ZoomInOrOutToCursorWindow(bool in, Window * w);
Point GetTileZoomCenterWindow(bool in, Window * w);
void UpdateViewportPosition(Window *w);

void OffsetGroundSprite(int x, int y);

void DrawGroundSprite(uint32 image);
void DrawGroundSpriteAt(uint32 image, int32 x, int32 y, byte z);
void AddSortableSpriteToDraw(uint32 image, int x, int y, int w, int h, byte dz, byte z);
void *AddStringToDraw(int x, int y, StringID string, uint32 params_1, uint32 params_2, uint32 params_3);
void AddChildSpriteScreen(uint32 image, int x, int y);


void StartSpriteCombine();
void EndSpriteCombine();

void HandleViewportClicked(ViewPort *vp, int x, int y);
void PlaceObject();
void SetRedErrorSquare(TileIndex tile);
void SetTileSelectSize(int w, int h);
void SetTileSelectBigSize(int ox, int oy, int sx, int sy);

void VpStartPlaceSizing(uint tile, int user);
void VpStartPreSizing();
void VpSetPresizeRange(uint from, uint to);
void VpSetPlaceSizingLimit(int limit);

Vehicle *CheckMouseOverVehicle();

enum {
	VPM_X_OR_Y = 0,
	VPM_FIX_X = 1,
	VPM_FIX_Y = 2,
	VPM_RAILDIRS = 3,
	VPM_X_AND_Y = 4,
	VPM_X_AND_Y_LIMITED = 5,
	VPM_SIGNALDIRS = 6
};

// viewport highlight mode (for highlighting tiles below cursor)
enum {
	VHM_NONE = 0,    // default
	VHM_RECT = 1,    // rectangle (stations, depots, ...)
	VHM_POINT = 2,   // point (lower land, raise land, level land, ...)
	VHM_SPECIAL = 3, // special mode used for highlighting while dragging (and for tunnels/docks)
	VHM_DRAG = 4,    // dragging items in the depot windows
	VHM_RAIL = 5,    // rail pieces
};

void VpSelectTilesWithMethod(int x, int y, int method);

// highlighting draw styles
enum {
	HT_NONE = 0,
	HT_RECT = 0x80,
	HT_POINT = 0x40,
	HT_LINE = 0x20, /* used for autorail highlighting (longer streches)
									 * (uses lower bits to indicate direction) */
	HT_RAIL = 0x10, /* autorail (one piece)
									 * (uses lower bits to indicate direction) */

	/* lower bits (used with HT_LINE and HT_RAIL): 
	 * (see ASCII art in autorail.h for a visual interpretation) */
	HT_DIR_X = 0,  // X direction
	HT_DIR_Y = 1,  // Y direction
	HT_DIR_HU = 2, // horizontal upper
	HT_DIR_HL = 3, // horizontal lower
	HT_DIR_VL = 4, // vertical left
	HT_DIR_VR = 5, // vertical right
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
bool HandlePlacePushButton(Window *w, int widget, uint32 cursor, int mode, PlaceProc *placeproc);

/* viewport.c */
// XXX - maximum viewports is maximum windows - 2 (main toolbar + status bar)
VARDEF ViewPort _viewports[25 - 2];
VARDEF TileHighlightData _thd;
VARDEF uint32 _active_viewports;

VARDEF Point _tile_fract_coords;

extern TileHighlightData * const _thd_ptr;


void ViewportDoDraw(const ViewPort *vp, int left, int top, int right, int bottom);

#endif /* VIEWPORT_H */
