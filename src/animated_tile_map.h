/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file animated_tile_map.h Maps accessors for animated tiles. */

#ifndef ANIMATED_TILE_MAP_H
#define ANIMATED_TILE_MAP_H

#include "core/bitmath_func.hpp"
#include "map_func.h"

/**
 * Animation state of a possibly-animated tile.
 */
enum class AnimatedTileState : uint8_t {
	None = 0, ///< Tile is not animated.
	Deleted = 1, ///< Tile was animated but should be removed.
	Animated = 3, ///< Tile is animated.
};

/**
 * Get the animated state of a tile.
 * @param t The tile.
 * @returns true iff the tile is animated.
 */
inline AnimatedTileState GetAnimatedTileState(Tile t)
{
	return static_cast<AnimatedTileState>(GB(t.m6(), 0, 2));
}

/**
 * Set the animated state of a tile.
 * @param t The tile.
 */
inline void SetAnimatedTileState(Tile t, AnimatedTileState state)
{
	SB(t.m6(), 0, 2, to_underlying(state));
}

#endif /* ANIMATED_TILE_MAP_H */
