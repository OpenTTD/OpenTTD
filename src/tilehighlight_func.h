/* $Id$ */

/** @file tilehighlight_func.h Functions related to tile highlights. */

#ifndef TILEHIGHLIGHT_FUNC_H
#define TILEHIGHLIGHT_FUNC_H

#include "gfx_type.h"
#include "window_type.h"
#include "viewport_type.h"
#include "tilehighlight_type.h"

typedef void PlaceProc(TileIndex tile);
void PlaceProc_DemolishArea(TileIndex tile);
bool GUIPlaceProcDragXY(ViewportDragDropSelectionProcess proc, TileIndex start_tile, TileIndex end_tile);

bool HandlePlacePushButton(Window *w, int widget, CursorID cursor, ViewportHighlightMode mode, PlaceProc *placeproc);
void SetObjectToPlaceWnd(CursorID icon, SpriteID pal, ViewportHighlightMode mode, Window *w);
void SetObjectToPlace(CursorID icon, SpriteID pal, ViewportHighlightMode mode, WindowClass window_class, WindowNumber window_num);
void ResetObjectToPlace();

void VpSelectTilesWithMethod(int x, int y, ViewportPlaceMethod method);
void VpStartPlaceSizing(TileIndex tile, ViewportPlaceMethod method, ViewportDragDropSelectionProcess process);
void VpSetPresizeRange(TileIndex from, TileIndex to);
void VpSetPlaceSizingLimit(int limit);

extern PlaceProc *_place_proc;
extern TileHighlightData _thd;

#endif /* TILEHIGHLIGHT_FUNC_H */
