/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file viewport_func.h Functions related to (drawing on) viewports. */

#ifndef VIEWPORT_FUNC_H
#define VIEWPORT_FUNC_H

#include "gfx_type.h"
#include "viewport_type.h"
#include "vehicle_type.h"
#include "window_type.h"
#include "tile_type.h"

static const int TILE_HEIGHT_STEP = 50; ///< One Z unit tile height difference is displayed as 50m.

void SetSelectionRed(bool);

void DeleteWindowViewport(Window *w);
void InitializeWindowViewport(Window *w, int x, int y, int width, int height, uint32 follow_flags, ZoomLevel zoom);
ViewPort *IsPtInWindowViewport(const Window *w, int x, int y);
Point GetTileBelowCursor();
void UpdateViewportPosition(Window *w);

void MarkAllViewportsDirty(int left, int top, int right, int bottom);

bool DoZoomInOutWindow(ZoomStateChange how, Window *w);
void ZoomInOrOutToCursorWindow(bool in, Window * w);
Point GetTileZoomCenterWindow(bool in, Window * w);
void HandleZoomMessage(Window *w, const ViewPort *vp, byte widget_zoom_in, byte widget_zoom_out);

/**
 * Zoom a viewport as far as possible in the given direction.
 * @param how Zooming direction.
 * @param w   Window owning the viewport.
 * @pre \a how should not be #ZOOM_NONE.
 */
static inline void MaxZoomInOut(ZoomStateChange how, Window *w)
{
	while (DoZoomInOutWindow(how, w)) {};
}

void OffsetGroundSprite(int x, int y);

void DrawGroundSprite(SpriteID image, PaletteID pal, const SubSprite *sub = NULL, int extra_offs_x = 0, int extra_offs_y = 0);
void DrawGroundSpriteAt(SpriteID image, PaletteID pal, int32 x, int32 y, int z, const SubSprite *sub = NULL, int extra_offs_x = 0, int extra_offs_y = 0);
void AddSortableSpriteToDraw(SpriteID image, PaletteID pal, int x, int y, int w, int h, int dz, int z, bool transparent = false, int bb_offset_x = 0, int bb_offset_y = 0, int bb_offset_z = 0, const SubSprite *sub = NULL);
void AddChildSpriteScreen(SpriteID image, PaletteID pal, int x, int y, bool transparent = false, const SubSprite *sub = NULL);
void ViewportAddString(const DrawPixelInfo *dpi, ZoomLevel small_from, const ViewportSign *sign, StringID string_normal, StringID string_small, StringID string_small_shadow, uint64 params_1, uint64 params_2 = 0, Colours colour = INVALID_COLOUR);


void StartSpriteCombine();
void EndSpriteCombine();

bool HandleViewportClicked(const ViewPort *vp, int x, int y);
void SetRedErrorSquare(TileIndex tile);
void SetTileSelectSize(int w, int h);
void SetTileSelectBigSize(int ox, int oy, int sx, int sy);

void ViewportDoDraw(const ViewPort *vp, int left, int top, int right, int bottom);

bool ScrollWindowToTile(TileIndex tile, Window *w, bool instant = false);
bool ScrollWindowTo(int x, int y, int z, Window *w, bool instant = false);

bool ScrollMainWindowToTile(TileIndex tile, bool instant = false);
bool ScrollMainWindowTo(int x, int y, int z = -1, bool instant = false);

void UpdateAllVirtCoords();

extern Point _tile_fract_coords;

#endif /* VIEWPORT_FUNC_H */
