/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tree_type.h Type definitions related to tree tiles. */

#ifndef TREE_TYPE_H
#define TREE_TYPE_H

#include "gfx_type.h"
#include "landscape_type.h"
#include "strings_type.h"
#include "tile_type.h"

/** Information about an individual tree. */
struct TreeSprite {
	SpriteID normal; ///< Sprite for normal terrain.
	SpriteID snowy; ///< Sprite for snowy terrain.
};

/** Information about a tree tile. */
struct TreeSpec {
	static constexpr uint8_t NUM_TREES = 4; ///< Number of trees per tile.
	static constexpr uint8_t NUM_TREE_VARIANTS = 4; ///< Number of variants per tile.

	std::array<std::array<uint8_t, NUM_TREES>, NUM_TREE_VARIANTS> trees{}; ///< TreeSprite for each tree of the tile.
	std::array<std::array<PaletteID, NUM_TREES>, NUM_TREE_VARIANTS> palettes{}; ///< Palette remaps for each sprite.
	StringID name{}; ///< Name of tree.
	LandscapeTypes landscapes{}; ///< Landscapes this tree tile may appear in.
	TropicZones tropiczones{}; ///< Tropical zones this tree tile may appear in.
	uint8_t probability = 255; ///< Probability of this tree tile being randomly created.
};

#endif /* TREE_TYPE_H */
