/* $Id$ */

/** @file viewport_func.h Functions related to (drawing on) viewports. */

#ifndef VIEWPORT_FUNC_H
#define VIEWPORT_FUNC_H

#include "gfx_type.h"
#include "viewport_type.h"
#include "vehicle_type.h"
#include "strings_type.h"
#include "window_type.h"
#include "tile_type.h"

void SetSelectionRed(bool);

void DeleteWindowViewport(Window *w);
void InitializeWindowViewport(Window *w, int x, int y, int width, int height, uint32 follow_flags, ZoomLevel zoom);
ViewPort *IsPtInWindowViewport(const Window *w, int x, int y);
Point GetTileBelowCursor();
void UpdateViewportPosition(Window *w);
void UpdateViewportSignPos(ViewportSign *sign, int left, int top, StringID str);

bool DoZoomInOutWindow(int how, Window *w);
void ZoomInOrOutToCursorWindow(bool in, Window * w);
Point GetTileZoomCenterWindow(bool in, Window * w);
void HandleZoomMessage(Window *w, const ViewPort *vp, byte widget_zoom_in, byte widget_zoom_out);

static inline void MaxZoomInOut(int how, Window *w)
{
	while (DoZoomInOutWindow(how, w)) {};
}

void OffsetGroundSprite(int x, int y);

void DrawGroundSprite(SpriteID image, SpriteID pal, const SubSprite *sub = NULL);
void DrawGroundSpriteAt(SpriteID image, SpriteID pal, int32 x, int32 y, byte z, const SubSprite *sub = NULL);
void AddSortableSpriteToDraw(SpriteID image, SpriteID pal, int x, int y, int w, int h, int dz, int z, bool transparent = false, int bb_offset_x = 0, int bb_offset_y = 0, int bb_offset_z = 0, const SubSprite *sub = NULL);
void AddStringToDraw(int x, int y, StringID string, uint64 params_1, uint64 params_2, uint16 color = 0, uint16 width = 0);
void AddChildSpriteScreen(SpriteID image, SpriteID pal, int x, int y, bool transparent = false, const SubSprite *sub = NULL);


void StartSpriteCombine();
void EndSpriteCombine();

void HandleViewportClicked(const ViewPort *vp, int x, int y);
void PlaceObject();
void SetRedErrorSquare(TileIndex tile);
void SetTileSelectSize(int w, int h);
void SetTileSelectBigSize(int ox, int oy, int sx, int sy);

Vehicle *CheckMouseOverVehicle();

void ViewportDoDraw(const ViewPort *vp, int left, int top, int right, int bottom);

bool ScrollWindowTo(int x, int y, Window *w, bool instant = false);

bool ScrollMainWindowToTile(TileIndex tile, bool instant = false);
bool ScrollMainWindowTo(int x, int y, bool instant = false);

extern Point _tile_fract_coords;

#endif /* VIEWPORT_FUNC_H */
