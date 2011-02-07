/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file animated_tile.cpp Everything related to animated tiles. */

#include "stdafx.h"
#include "core/alloc_func.hpp"
#include "tile_cmd.h"
#include "viewport_func.h"

/** The table/list with animated tiles. */
TileIndex *_animated_tile_list = NULL;
/** The number of animated tiles in the current state. */
uint _animated_tile_count = 0;
/** The number of slots for animated tiles allocated currently. */
uint _animated_tile_allocated = 0;

/**
 * Removes the given tile from the animated tile table.
 * @param tile the tile to remove
 */
void DeleteAnimatedTile(TileIndex tile)
{
	for (TileIndex *ti = _animated_tile_list; ti < _animated_tile_list + _animated_tile_count; ti++) {
		if (tile == *ti) {
			/* Remove the hole
			 * The order of the remaining elements must stay the same, otherwise the animation loop
			 * may miss a tile; that's why we must use memmove instead of just moving the last element.
			 */
			memmove(ti, ti + 1, (_animated_tile_list + _animated_tile_count - (ti + 1)) * sizeof(*ti));
			_animated_tile_count--;
			MarkTileDirtyByTile(tile);
			return;
		}
	}
}

/**
 * Add the given tile to the animated tile table (if it does not exist
 * on that table yet). Also increases the size of the table if necessary.
 * @param tile the tile to make animated
 */
void AddAnimatedTile(TileIndex tile)
{
	MarkTileDirtyByTile(tile);

	for (const TileIndex *ti = _animated_tile_list; ti < _animated_tile_list + _animated_tile_count; ti++) {
		if (tile == *ti) return;
	}

	/* Table not large enough, so make it larger */
	if (_animated_tile_count == _animated_tile_allocated) {
		_animated_tile_allocated *= 2;
		_animated_tile_list = ReallocT<TileIndex>(_animated_tile_list, _animated_tile_allocated);
	}

	_animated_tile_list[_animated_tile_count] = tile;
	_animated_tile_count++;
}

/**
 * Animate all tiles in the animated tile list, i.e.\ call AnimateTile on them.
 */
void AnimateAnimatedTiles()
{
	const TileIndex *ti = _animated_tile_list;
	while (ti < _animated_tile_list + _animated_tile_count) {
		const TileIndex curr = *ti;
		AnimateTile(curr);
		/* During the AnimateTile call, DeleteAnimatedTile could have been called,
		 * deleting an element we've already processed and pushing the rest one
		 * slot to the left. We can detect this by checking whether the index
		 * in the current slot has changed - if it has, an element has been deleted,
		 * and we should process the current slot again instead of going forward.
		 * NOTE: this will still break if more than one animated tile is being
		 *       deleted during the same AnimateTile call, but no code seems to
		 *       be doing this anyway.
		 */
		if (*ti == curr) ++ti;
	}
}

/**
 * Initialize all animated tile variables to some known begin point
 */
void InitializeAnimatedTiles()
{
	_animated_tile_list = ReallocT<TileIndex>(_animated_tile_list, 256);
	_animated_tile_count = 0;
	_animated_tile_allocated = 256;
}
