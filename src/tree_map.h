/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tree_map.h Map accessors for tree tiles. */

#ifndef TREE_MAP_H
#define TREE_MAP_H

#include "tile_map.h"
#include "water_map.h"

/**
 * List of tree types along all landscape types.
 *
 * This enumeration contains a list of the different tree types along
 * all landscape types. The values for the enumerations may be used for
 * offsets from the grfs files. These points to the start of
 * the tree list for a landscape. See the TREE_COUNT_* enumerations
 * for the amount of different trees for a specific landscape.
 * TREE_RANDOM_* are special values used in the map array that signify that
 * exact tree type is not stored and should be determined procedurally.
 */
enum TreeType {
	TREE_TEMPERATE    = 0x00, ///< temperate tree
	TREE_SUB_ARCTIC   = 0x0C, ///< tree on a sub_arctic landscape
	TREE_RAINFOREST   = 0x14, ///< tree on the 'green part' on a sub-tropical map
	TREE_CACTUS       = 0x1B, ///< a cactus for the 'desert part' on a sub-tropical map
	TREE_SUB_TROPICAL = 0x1C, ///< tree on a sub-tropical map, non-rainforest, non-desert
	TREE_TOYLAND      = 0x20, ///< tree on a toyland map
	TREE_RANDOM_TEMPERATE = 0xFA, ///< procedural TREE_TEMPERATE
	TREE_RANDOM_BEGIN = TREE_RANDOM_TEMPERATE,
	TREE_RANDOM_ARCTIC = 0xFB, ///< procedural TREE_SUB_ARCTIC
	TREE_RANDOM_TROPIC_NORMAL = 0xFC, ///< procedural tropic tree
	TREE_RANDOM_TROPIC_RAINFOREST = 0xFD, ///< procedural TREE_RAINFOREST
	TREE_RANDOM_TOYLAND = 0xFE, ///< procedural TREE_TOYLAND
	TREE_INVALID      = 0xFF, ///< An invalid tree
	TREE_RANDOM_END   = TREE_INVALID,
};

/* Counts the number of tree types for each landscape.
 *
 * This list contains the counts of different tree types for each landscape. This list contains
 * 5 entries instead of 4 (as there are only 4 landscape types) as the sub tropic landscape
 * has two types of area, one for normal trees and one only for cacti.
 */
static const uint TREE_COUNT_TEMPERATE    = TREE_SUB_ARCTIC - TREE_TEMPERATE;    ///< number of tree types on a temperate map.
static const uint TREE_COUNT_SUB_ARCTIC   = TREE_RAINFOREST - TREE_SUB_ARCTIC;   ///< number of tree types on a sub arctic map.
static const uint TREE_COUNT_RAINFOREST   = TREE_CACTUS     - TREE_RAINFOREST;   ///< number of tree types for the 'rainforest part' of a sub-tropic map.
static const uint TREE_COUNT_SUB_TROPICAL = TREE_TOYLAND    - TREE_SUB_TROPICAL; ///< number of tree types for the 'sub-tropic part' of a sub-tropic map.
static const uint TREE_COUNT_TOYLAND      = 9;                                   ///< number of tree types on a toyland map.

/**
 * Enumeration for ground types of tiles with trees.
 *
 * This enumeration defines the ground types for tiles with trees on it.
 */
enum TreeGround {
	TREE_GROUND_GRASS       = 0, ///< normal grass
	TREE_GROUND_ROUGH       = 1, ///< some rough tile
	TREE_GROUND_SNOW_DESERT = 2, ///< a desert or snow tile, depend on landscape
	TREE_GROUND_SHORE       = 3, ///< shore
	TREE_GROUND_ROUGH_SNOW  = 4, ///< A snow tile that is rough underneath.
};

/**
 * Enumeration for tree growth stages.
 *
 * This enumeration defines the stages of tree growth for tiles with trees on it.
 */
enum class TreeGrowthStage : uint {
	Growing1 = 0,  ///< First stage of growth
	Growing2 = 1,  ///< Second stage of growth
	Growing3 = 2,  ///< Third stage of growth
	Grown = 3,  ///< Fully grown tree
	Dying1 = 4,  ///< First stage of dying
	Dying2 = 5,  ///< Second stage of dying
	Dead = 6,  ///< Dead tree
	Procedural = 7,  ///< Magic value that signifies that tree growth stage is determined procedurally, not stored in the map array.
};

/**
 * Returns the treetype of a tile.
 *
 * This function returns the treetype of a given tile. As there are more
 * possible treetypes for a tile in a game as the enumeration #TreeType defines
 * this function may be return a value which isn't catch by an entry of the
 * enumeration #TreeType. But there is no problem known about it.
 *
 * @param t The tile to get the treetype from
 * @return The treetype of the given tile with trees
 * @pre Tile t must be of type MP_TREES
 */
inline TreeType GetTreeType(Tile t)
{
	assert(IsTileType(t, MP_TREES));
	return (TreeType)t.m3();
}

/**
 * Returns the groundtype for tree tiles.
 *
 * This function returns the groundtype of a tile with trees.
 *
 * @param t The tile to get the groundtype from
 * @return The groundtype of the tile
 * @pre Tile must be of type MP_TREES
 */
inline TreeGround GetTreeGround(Tile t)
{
	assert(IsTileType(t, MP_TREES));
	return (TreeGround)GB(t.m2(), 6, 3);
}

/**
 * Returns the 'density' of a tile with trees.
 *
 * This function returns the density of a tile which got trees. Note
 * that this value doesn't count the number of trees on a tile, use
 * #GetTreeCount instead. This function instead returns some kind of
 * groundtype of the tile. As the map-array is finite in size and
 * the information about the trees must be saved somehow other
 * information about a tile must be saved somewhere encoded in the
 * tile. So this function returns the density of a tile for sub arctic
 * and sub tropical games. This means for sub arctic the type of snowline
 * (0 to 3 for all 4 types of snowtiles) and for sub tropical the value
 * 3 for a desert (and 0 for non-desert). The function name is not read as
 * "get the tree density of a tile" but "get the density of a tile which got trees".
 *
 * @param t The tile to get the 'density'
 * @pre Tile must be of type MP_TREES
 * @see GetTreeCount
 */
inline uint GetTreeDensity(Tile t)
{
	assert(IsTileType(t, MP_TREES));
	return GB(t.m2(), 4, 2);
}

/**
 * Set the density and ground type of a tile with trees.
 *
 * This functions saves the ground type and the density which belongs to it
 * for a given tile.
 *
 * @param t The tile to set the density and ground type
 * @param g The ground type to save
 * @param d The density to save with
 * @pre Tile must be of type MP_TREES
 */
inline void SetTreeGroundDensity(Tile t, TreeGround g, uint d)
{
	assert(IsTileType(t, MP_TREES)); // XXX incomplete
	SB(t.m2(), 4, 2, d);
	SB(t.m2(), 6, 3, g);
	SetWaterClass(t, g == TREE_GROUND_SHORE ? WATER_CLASS_SEA : WATER_CLASS_INVALID);
}

/**
 * Returns the number of trees on a tile.
 *
 * This function returns the number of trees of a tile (1-4).
 * The tile must be contains at least one tree or be more specific: it must be
 * of type MP_TREES.
 *
 * @param t The index to get the number of trees
 * @return The number of trees (1-4)
 * @pre Tile must be of type MP_TREES
 */
inline uint GetTreeCount(Tile t)
{
	assert(IsTileType(t, MP_TREES));
	return GB(t.m5(), 6, 2) + 1;
}

/**
 * Add a amount to the tree-count value of a tile with trees.
 *
 * This function add a value to the tree-count value of a tile. This
 * value may be negative to reduce the tree-counter. If the resulting
 * value reach 0 it doesn't get converted to a "normal" tile.
 *
 * @param t The tile to change the tree amount
 * @param c The value to add (or reduce) on the tree-count value
 * @pre Tile must be of type MP_TREES
 */
inline void AddTreeCount(Tile t, int c)
{
	assert(IsTileType(t, MP_TREES)); // XXX incomplete
	t.m5() += c << 6;
}

/**
 * Returns the tree growth stage.
 *
 * This function returns the tree growth stage of a tile with trees.
 *
 * @param t The tile to get the tree growth stage
 * @return The tree growth stage
 * @pre Tile must be of type MP_TREES
 */
inline TreeGrowthStage GetTreeGrowth(Tile t)
{
	assert(IsTileType(t, MP_TREES));
	return static_cast<TreeGrowthStage>(GB(t.m5(), 0, 3));
}

/**
 * Add a value to the tree growth stage.
 *
 * This function adds a value to the tree grow stage of a tile.
 *
 * @param t The tile to add the value on
 * @param a The value to add on the tree growth stage
 * @pre Tile must be of type MP_TREES
 */
inline void AddTreeGrowth(Tile t, int a)
{
	assert(IsTileType(t, MP_TREES)); // XXX incomplete
	t.m5() += a;
}

/**
 * Sets the tree growth stage of a tile.
 *
 * This function sets the tree growth stage of a tile directly with
 * the given value.
 *
 * @param t The tile to change the tree growth stage
 * @param g The new value
 * @pre Tile must be of type MP_TREES
 */
inline void SetTreeGrowth(Tile t, TreeGrowthStage g)
{
	assert(IsTileType(t, MP_TREES)); // XXX incomplete
	SB(t.m5(), 0, 3, static_cast<uint>(g));
}

/**
 * Make a tree-tile.
 *
 * This functions change the tile to a tile with trees and all information which belongs to it.
 *
 * @param t The tile to make a tree-tile from
 * @param type The type of the tree
 * @param count the number of trees
 * @param growth the growth stage
 * @param ground the ground type
 * @param density the density (not the number of trees)
 */
inline void MakeTree(Tile t, TreeType type, uint count, TreeGrowthStage growth, TreeGround ground, uint density)
{
	SetTileType(t, MP_TREES);
	SetTileOwner(t, OWNER_NONE);
	SetWaterClass(t, ground == TREE_GROUND_SHORE ? WATER_CLASS_SEA : WATER_CLASS_INVALID);
	t.m2() = ground << 6 | density << 4 | 0;
	t.m3() = type;
	t.m4() = 0 << 5 | 0 << 2;
	t.m5() = count << 6 | static_cast<uint>(growth);
	SB(t.m6(), 2, 4, 0);
	t.m7() = 0;
}

#endif /* TREE_MAP_H */
