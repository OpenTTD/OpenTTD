/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file animated_tile.cpp Everything related to animated tiles. */

#include "stdafx.h"
#include "animated_tile_func.h"
#include "animated_tile_map.h"
#include "tile_cmd.h"
#include "viewport_func.h"
#include "framerate_type.h"

#include "safeguards.h"

/** The table/list with animated tiles. */
std::vector<TileIndex> _animated_tiles;

/**
 * Stops animation on the given tile.
 * @param tile the tile to remove
 * @param immediate immediately delete the tile from the animated tile list instead of waiting for the next tick.
 */
void DeleteAnimatedTile(TileIndex tile, bool immediate)
{
	if (immediate) {
		if (GetAnimatedTileState(tile) == AnimatedTileState::None) return;

		/* The tile may be switched to a non-animatable tile soon, so we should remove it from the
		 * animated tile list early. */
		SetAnimatedTileState(tile, AnimatedTileState::None);

		/* To avoid having to move everything after this tile in the animated tile list, look for this tile
		 * in the animated tile list and replace with last entry if not last. */
		auto it = std::ranges::find(_animated_tiles, tile);
		if (it == std::end(_animated_tiles)) return;

		if (std::next(it) != std::end(_animated_tiles)) *it = _animated_tiles.back();
		_animated_tiles.pop_back();

		return;
	}

	/* If the tile was animated, mark it for deletion from the tile list on the next animation loop. */
	if (GetAnimatedTileState(tile) == AnimatedTileState::Animated) SetAnimatedTileState(tile, AnimatedTileState::Deleted);
}

/**
 * Add the given tile to the animated tile table (if it does not exist yet).
 * @param tile the tile to make animated
 * @param mark_dirty whether to also mark the tile dirty.
 */
void AddAnimatedTile(TileIndex tile, bool mark_dirty)
{
	if (mark_dirty) MarkTileDirtyByTile(tile);

	const AnimatedTileState state = GetAnimatedTileState(tile);

	/* Tile is already animated so nothing needs to happen. */
	if (state == AnimatedTileState::Animated) return;

	/* Tile has no previous animation state, so add to the tile list. If the state is anything
	 * other than None (e.g. Deleted) then the tile will still be in the list and does not need to be added again. */
	if (state == AnimatedTileState::None) _animated_tiles.push_back(tile);

	SetAnimatedTileState(tile, AnimatedTileState::Animated);
}

/**
 * Animate all tiles in the animated tile list, i.e.\ call AnimateTile on them.
 */
void AnimateAnimatedTiles()
{
	PerformanceAccumulator landscape_framerate(PFE_GL_LANDSCAPE);

	for (auto it = std::begin(_animated_tiles); it != std::end(_animated_tiles); /* nothing */) {
		TileIndex &tile = *it;

		if (GetAnimatedTileState(tile) != AnimatedTileState::Animated) {
			/* Tile should not be animated any more, mark it as not animated and erase it from the list. */
			SetAnimatedTileState(tile, AnimatedTileState::None);

			/* Removing the last entry, no need to swap and continue. */
			if (std::next(it) == std::end(_animated_tiles)) {
				_animated_tiles.pop_back();
				break;
			}

			/* Replace the current list entry with the back of the list to avoid moving elements. */
			*it = _animated_tiles.back();
			_animated_tiles.pop_back();
			continue;
		}

		AnimateTile(tile);
		++it;
	}
}

/**
 * Initialize all animated tile variables to some known begin point
 */
void InitializeAnimatedTiles()
{
	_animated_tiles.clear();
}
