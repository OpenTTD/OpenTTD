/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tree_land.h Sprites to use and how to display them for tree tiles. */

#ifndef TREE_LAND_H
#define TREE_LAND_H

static const Coord2D<uint8_t> _tree_layout_xy[][4] = {
	{ { 9, 3 }, { 1, 8 }, { 0, 0 }, { 8, 9 } },
	{ { 4, 4 }, { 9, 1 }, { 6, 9 }, { 0, 9 } },
	{ { 9, 1 }, { 0, 9 }, { 6, 6 }, { 3, 0 } },
	{ { 3, 9 }, { 8, 2 }, { 9, 9 }, { 1, 5 } }
};

static constexpr uint8_t TREE_GROWTH_COUNT = 7; ///< Number of tree growth stages.

/**
 * Get the SpriteID for a default tree.
 * @param index Offset of tree.
 * @return SpriteID for default tree.
 */
static constexpr SpriteID MakeTree(uint8_t index)
{
	return SPR_TREE_BASE + (TREE_GROWTH_COUNT * index);
}

/** TreeSpec of default trees. Trees used to build layouts are listed in order that matches the original _tree_layout_sprite data. */
static constexpr std::array<TreeSpec, 54> _original_tree_specs = {{
	/* Temperate. */
	{MakeTree(6), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {255, 32, 0, 0}, {TreeClass::Tem1, TreeClass::Tem1, TreeClass::Tem1, TreeClass::Tem1}}, // 6
	{MakeTree(8), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {255, 0, 32, 32}, {TreeClass::Tem1, TreeClass::Tem1, TreeClass::Tem1, TreeClass::Tem1}}, // 8
	{MakeTree(11), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {255, 0, 32, 32}, {TreeClass::Tem1, TreeClass::Tem1, TreeClass::Tem1, TreeClass::Tem1}}, // 11
	{MakeTree(0), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {255, 0, 0, 0}, {TreeClass::Tem2, TreeClass::Tem2, TreeClass::Tem2, TreeClass::Tem2}}, // 0
	{MakeTree(4), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {255, 0, 0, 0}, {TreeClass::Tem2, TreeClass::Tem2, TreeClass::Tem2, TreeClass::Tem2}}, // 4
	{MakeTree(2), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {255, 64, 96, 128}, {TreeClass::Tem2, TreeClass::Tem2, TreeClass::Tem2, TreeClass::Tem2}}, // 2
	{MakeTree(5), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {255, 32, 0, 0}, {TreeClass::Tem2, TreeClass::Tem2, TreeClass::Tem2, TreeClass::Tem2}}, // 5
	{MakeTree(1), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {255, 32, 32, 0}, {TreeClass::Tem2, TreeClass::Tem2, TreeClass::Tem2, TreeClass::Tem2}}, // 1
	{MakeTree(12), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {255, 32, 64, 32}, {TreeClass::Tem3, TreeClass::Tem3, TreeClass::Tem3, TreeClass::Tem3}}, // 12
	{MakeTree(14), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {255, 32, 0, 96}, {TreeClass::Tem3, TreeClass::Tem3, TreeClass::Tem3, TreeClass::Tem3}}, // 14
	{MakeTree(16), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {255, 96, 64, 32}, {TreeClass::Tem3, TreeClass::Tem3, TreeClass::Tem3, TreeClass::Tem3}}, // 16
	{MakeTree(18), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {255, 32, 64, 32}, {TreeClass::Tem3, TreeClass::Tem3, TreeClass::Tem3, TreeClass::Tem3}}, // 18

	/* Temperate trees that only appear alongside other trees. */
	{MakeTree(3), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {}, {TreeClass::Tem2}}, // 3
	{MakeTree(7), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {}, {TreeClass::Tem1}}, // 7
	{MakeTree(9), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {}, {TreeClass::Tem1}}, // 9
	{MakeTree(10), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {}, {TreeClass::Tem1}}, // 10
	{MakeTree(13), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {}, {TreeClass::Tem3}}, // 13
	{MakeTree(15), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {}, {TreeClass::Tem3}}, // 15
	{MakeTree(17), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Temperate, {}, {}, {TreeClass::Tem3}}, // 17

	/* Arctic. */
	{MakeTree(19), MakeTree(27), STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Arctic, {}, {255, 64, 64, 96}, {TreeClass::Arc1, TreeClass::Arc1, TreeClass::Arc2, TreeClass::Arc1}}, // 0
	{MakeTree(24), MakeTree(32), STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Arctic, {}, {255, 64, 96, 0}, {TreeClass::Arc1, TreeClass::Arc1, TreeClass::Arc1, TreeClass::Arc2}}, // 5
	{MakeTree(25), MakeTree(33), STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Arctic, {}, {255, 96, 64, 32}, {TreeClass::Arc1, TreeClass::Arc1, TreeClass::Arc1, TreeClass::Arc1}}, // 6
	{MakeTree(22), MakeTree(30), STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Arctic, {}, {255, 64, 96, 32}, {TreeClass::Arc2, TreeClass::Arc2, TreeClass::Arc2, {TreeClass::Arc1, TreeClass::Arc2}}}, // 3
	{MakeTree(23), MakeTree(31), STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Arctic, {}, {255, 0, 0, 0}, {TreeClass::Arc2, TreeClass::Arc1, TreeClass::Arc1, TreeClass::Arc1}}, // 4
	{MakeTree(20), MakeTree(28), STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Arctic, {}, {255, 32, 0, 32}, {TreeClass::Arc3, {TreeClass::Arc2, TreeClass::Arc3}, TreeClass::Arc3, {TreeClass::Arc2, TreeClass::Arc3}}}, // 1
	{MakeTree(21), MakeTree(29), STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Arctic, {}, {255, 0, 64, 0}, {TreeClass::Arc3, {TreeClass::Arc2, TreeClass::Arc3}, TreeClass::Arc3, {TreeClass::Arc1, TreeClass::Arc2, TreeClass::Arc3}}}, // 2
	{MakeTree(26), MakeTree(34), STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Arctic, {}, {255, 32, 64, 32}, {TreeClass::Arc3, {TreeClass::Arc2, TreeClass::Arc3}, TreeClass::Arc3, {TreeClass::Arc1, TreeClass::Arc2, TreeClass::Arc3}}}, // 7

	/* Sub-tropic. */
	{MakeTree(37), 0, STR_LAI_TREE_NAME_RAINFOREST, {}, 1, 4, 6, LandscapeType::Tropic, TropicZone::Rainforest, {255, 32, 64, 64}, {TreeClass::Tro1, TreeClass::Tro1, TreeClass::Tro1, TreeClass::Tro1}}, // 2
	{MakeTree(38), 0, STR_LAI_TREE_NAME_RAINFOREST, {}, 1, 4, 6, LandscapeType::Tropic, TropicZone::Rainforest, {255, 64, 64, 32}, {TreeClass::Tro1, TreeClass::Tro1, TreeClass::Tro1, TreeClass::Tro1}}, // 3
	{MakeTree(41), 0, STR_LAI_TREE_NAME_RAINFOREST, {}, 1, 4, 6, LandscapeType::Tropic, TropicZone::Rainforest, {255, 32, 32, 64}, {TreeClass::Tro1, TreeClass::Tro1, TreeClass::Tro1, TreeClass::Tro1}}, // 6
	{MakeTree(42), 0, STR_LAI_TREE_NAME_RAINFOREST, {}, 1, 4, 6, LandscapeType::Tropic, TropicZone::Rainforest, {255, 64, 0, 32}, {TreeClass::Tro1, TreeClass::Tro1, TreeClass::Tro1, TreeClass::Tro1}}, // 7
	{MakeTree(46), 0, STR_LAI_TREE_NAME_RAINFOREST, {}, 1, 4, 6, LandscapeType::Tropic, TropicZone::Rainforest, {255, 32, 0, 64}, {TreeClass::Tro1, TreeClass::Tro1, TreeClass::Tro1, TreeClass::Tro1}}, // 11
	{MakeTree(51), 0, STR_LAI_TREE_NAME_RAINFOREST, {}, 1, 4, 6, LandscapeType::Tropic, TropicZone::Rainforest, {255, 32, 32, 0}, {TreeClass::Tro1, TreeClass::Tro1, TreeClass::Tro1, TreeClass::Tro1}}, // 16
	{MakeTree(50), 0, STR_LAI_TREE_NAME_RAINFOREST, {}, 1, 4, 6, LandscapeType::Tropic, TropicZone::Rainforest, {255, 96, 32, 32}, {TreeClass::Tro1, TreeClass::Tro1, TreeClass::Tro1, TreeClass::Tro1}}, // 15
	{MakeTree(48), 0, STR_LAI_TREE_NAME_CACTUS_PLANTS, {}, 1, 4, 2, LandscapeType::Tropic, TropicZone::Desert, {12, 64, 64, 64}, {TreeClass::Cactus, TreeClass::Cactus, TreeClass::Cactus, TreeClass::Cactus}}, // 13
	{MakeTree(44), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Tropic, TropicZone::Normal, {255, 32, 96, 32}, {TreeClass::Tro3, TreeClass::Tro3, TreeClass::Tro3, TreeClass::Tro3}}, // 9
	{MakeTree(47), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Tropic, TropicZone::Normal, {255, 64, 32, 32}, {TreeClass::Tro3, TreeClass::Tro3, TreeClass::Tro3, TreeClass::Tro3}}, // 12
	{MakeTree(35), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Tropic, TropicZone::Normal, {255, 64, 0, 64}, {TreeClass::Tro3, TreeClass::Tro3, TreeClass::Tro3, TreeClass::Tro3}}, // 0
	{MakeTree(52), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Tropic, TropicZone::Normal, {255, 64, 32, 32}, {TreeClass::Tro3, TreeClass::Tro3, TreeClass::Tro3, TreeClass::Tro3}}, // 17

	/* Sub-tropic trees that only appear alongside other trees. */
	{MakeTree(36), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Tropic, TropicZone::Normal, {}, {TreeClass::Tro3}}, // 1
	{MakeTree(39), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Tropic, TropicZone::Rainforest, {}, {TreeClass::Tro3}}, // 4
	{MakeTree(40), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Tropic, TropicZone::Rainforest, {}, {TreeClass::Tro1}}, // 5
	{MakeTree(43), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Tropic, TropicZone::Rainforest, {}, {TreeClass::Tro1}}, // 8
	{MakeTree(45), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Tropic, TropicZone::Normal, {}, {TreeClass::Tro3}}, // 10
	{MakeTree(49), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 2, LandscapeType::Tropic, TropicZone::Desert, {}, {TreeClass::Cactus}}, // 14

	/* Toyland. */
	{MakeTree(53), 0, STR_LAI_TREE_NAME_TREES, {TreeFlag::Colour}, 1, 4, 6, LandscapeType::Toyland, {}, {255}}, // 0
	{MakeTree(54), 0, STR_LAI_TREE_NAME_TREES, {TreeFlag::Colour}, 1, 4, 6, LandscapeType::Toyland, {}, {255}}, // 1
	{MakeTree(55), 0, STR_LAI_TREE_NAME_TREES, {TreeFlag::Colour}, 1, 4, 6, LandscapeType::Toyland, {}, {255}}, // 2
	{MakeTree(56), 0, STR_LAI_TREE_NAME_TREES, {}, 1, 4, 6, LandscapeType::Toyland, {}, {255}}, // 3
	{MakeTree(57), 0, STR_LAI_TREE_NAME_TREES, {TreeFlag::Colour}, 1, 4, 6, LandscapeType::Toyland, {}, {255}}, // 4
	{MakeTree(58), 0, STR_LAI_TREE_NAME_TREES, {TreeFlag::Colour}, 1, 4, 6, LandscapeType::Toyland, {}, {255}}, // 5
	{MakeTree(59), 0, STR_LAI_TREE_NAME_TREES, {TreeFlag::Colour}, 1, 4, 6, LandscapeType::Toyland, {}, {255}}, // 6
	{MakeTree(60), 0, STR_LAI_TREE_NAME_TREES, {TreeFlag::Colour}, 1, 4, 6, LandscapeType::Toyland, {}, {255}}, // 7
	{MakeTree(61), 0, STR_LAI_TREE_NAME_TREES, {TreeFlag::Colour}, 1, 4, 6, LandscapeType::Toyland, {}, {255}}, // 8
}};

#endif /* TREE_LAND_H */
