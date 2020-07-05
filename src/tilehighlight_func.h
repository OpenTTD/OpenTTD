/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tilehighlight_func.h Functions related to tile highlights. */

#ifndef TILEHIGHLIGHT_FUNC_H
#define TILEHIGHLIGHT_FUNC_H

#include "gfx_type.h"
#include "tilehighlight_type.h"
#include "track_type.h"

#include <queue>
#include <vector>
#include <unordered_map>

// try to compress the size of this struct
struct _pathNode {
	_pathNode *prev;
	TileIndex tile; // location of node on map
	uint16 g_cost; // weighted cost to this node, I really hope no path gets longer than this...
	uint16 h_cost; // (under)estimate cost to goal unless you're not bothered with accuracy :^)
	Trackdir direction; // direction of the rail on the node, use Trackdir enums
	uint8 turn_left;
	uint8 turn_right;
	uint8 hill_up;
	uint8 hill_down;
};

typedef struct _pathNode *PathNode;

typedef std::unordered_map<TileIndex, Track> _path_set;
typedef std::unordered_map<uint64, PathNode> _node_map;

static uint64 HashPathNode(const TileIndex& tile, const Trackdir& dir)
{
	return (tile << 4) + dir;
}

static uint64 HashPathNode(const PathNode& node)
{
	return HashPathNode(node->tile, node->direction);
}

extern _path_set PathHighlightSet; // map of Tile to Track of finished path
extern _node_map AllNodes; // unordered map of all nodes (so we can search/delete them later
extern TileIndex railplanner_tile_start;
extern Trackdir railplanner_dir_start;
extern TileIndex railplanner_tile_end;
extern Trackdir railplanner_dir_end;
extern RailplannerPhase rp_phase;

void PlaceProc_DemolishArea(TileIndex tile);
bool GUIPlaceProcDragXY(ViewportDragDropSelectionProcess proc, TileIndex start_tile, TileIndex end_tile);

bool HandlePlacePushButton(Window *w, int widget, CursorID cursor, HighLightStyle mode);
void SetObjectToPlaceWnd(CursorID icon, PaletteID pal, HighLightStyle mode, Window *w);
void SetObjectToPlace(CursorID icon, PaletteID pal, HighLightStyle mode, WindowClass window_class, WindowNumber window_num);
void ResetObjectToPlace();

void VpSelectTilesWithMethod(int x, int y, ViewportPlaceMethod method);
void VpStartPlaceSizing(TileIndex tile, ViewportPlaceMethod method, ViewportDragDropSelectionProcess process);
void VpSetPresizeRange(TileIndex from, TileIndex to);
void VpSetPlaceSizingLimit(int limit);

void UpdateTileSelection();

extern TileHighlightData _thd;

#endif /* TILEHIGHLIGHT_FUNC_H */
