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
#include "newgrf_commons.h"
#include "strings_type.h"
#include "tile_type.h"

/** Flags controlling tree behaviour. */
enum class TreeFlag : uint8_t {
	Colour = 0, ///< Apply random 'company' recolour.
	MonthlyRefresh = 1, ///< Enable monthly refresh cycle.
};

/** Bitset of \c TreeFlag elements. */
using TreeFlags = EnumBitSet<TreeFlag, uint8_t>;

/** Tree class types. */
enum class TreeClass : uint8_t {
	Tem1, ///< Temperate type 1
	Tem2, ///< Temperate type 2
	Tem3, ///< Temperate type 3
	Arc1, ///< Arctic type 1
	Arc2, ///< Arctic type 2
	Arc3, ///< Arctic type 3
	Tro1, ///< Tropic type 1
	Tro2, ///< Tropic type 2
	Tro3, ///< Tropic type 3
	Cactus, ///< Cactus
};

/** Bitset of \c TreeClass elements. */
using TreeClasses = EnumBitSet<TreeClass, uint64_t>;

/** Information about an individual tree. */
struct TreeSpec {
	SpriteID normal{}; ///< Sprite for normal terrain.
	SpriteID snowy{}; ///< Sprite for snowy terrain.
	StringID name{}; ///< Name of tree.
	TreeFlags flags{}; ///< Tree flags
	uint8_t cycle_interval = 1; ///< Growth cycle update interval.
	uint8_t max_trees = 4; ///< Maximum number of trees on a tile.
	uint8_t height = 6; ///< Height of tree.
	LandscapeTypes landscapes{}; ///< Landscapes this tree tile may appear in.
	TropicZones tropiczones{}; ///< Tropical zones this tree tile may appear in.
	std::array<uint8_t, 4> probability{}; ///< Probability of this tree tile being randomly created (first entry) or being included in extra lots (remaining entries)
	std::array<TreeClasses, 4> classes{}; ///< Compatibility tree classes.
	SubstituteGRFFileProps grf_prop{}; ///< properties related the the grf file
};

/** Information about a tree tile. */
struct TreeTileSpec {
	static constexpr uint8_t NUM_TREES = 4; ///< Number of trees per tile.
	static constexpr uint8_t NUM_TREE_VARIANTS = 16; ///< Number of variants per tile.

	std::array<std::array<uint16_t, NUM_TREES>, NUM_TREE_VARIANTS> trees{}; ///< The trees of this layout.
	std::array<std::array<PaletteID, NUM_TREES>, NUM_TREE_VARIANTS> palettes{}; ///< The palette remaps of this layout.
	StringID name{}; ///< Name of tree.
	TreeFlags flags{}; ///< Combined tree flags.
	uint8_t max_trees = 0; ///< Maximum number of trees on a tile.
	uint8_t height = 0; ///< Height of tree tile.
	LandscapeTypes landscapes{}; ///< Landscapes this tree tile may appear in.
	TropicZones tropiczones{}; ///< Tropical zones this tree tile may appear in.
	uint8_t probability = 0; ///< Probability of this tree tile being randomly created.
	TreeClasses classes{}; ///< Combined classes of this layout
};

#endif /* TREE_TYPE_H */
