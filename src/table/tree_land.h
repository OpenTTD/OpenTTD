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
 * Make a TreeSprite for a default tree.
 * @param index Sprite offset index for normal tree.
 * @return TreeSprite for default tree.
 */
static constexpr TreeSprite MakeDefaultTree(uint8_t index)
{
	return {SPR_TREE_BASE + (TREE_GROWTH_COUNT * index), 0};
}

/**
 * Make a TreeSprite for a default snowy tree.
 * @param normal_index Sprite offset index for normal tree.
 * @param snowy_index Sprite offset index for snowy tree.
 * @return TreeSprite for default snowy tree.
 */
static constexpr TreeSprite MakeDefaultSnowyTree(uint8_t normal_index, uint8_t snowy_index)
{
	return {SPR_TREE_BASE + (TREE_GROWTH_COUNT * normal_index), SPR_TREE_BASE + (TREE_GROWTH_COUNT * snowy_index)};
}

/** Sprite IDs of original trees, normal and snowy variants. */
static constexpr std::array<TreeSprite, 54> _tree_sprites = {
	/* Temperate */
	MakeDefaultTree(0),
	MakeDefaultTree(1),
	MakeDefaultTree(2),
	MakeDefaultTree(3),
	MakeDefaultTree(4),
	MakeDefaultTree(5),
	MakeDefaultTree(6),
	MakeDefaultTree(7),
	MakeDefaultTree(8),
	MakeDefaultTree(9),
	MakeDefaultTree(10),
	MakeDefaultTree(11),
	MakeDefaultTree(12),
	MakeDefaultTree(13),
	MakeDefaultTree(14),
	MakeDefaultTree(15),
	MakeDefaultTree(16),
	MakeDefaultTree(17),
	MakeDefaultTree(18),

	/* Arctic */
	MakeDefaultSnowyTree(19, 27),
	MakeDefaultSnowyTree(20, 28),
	MakeDefaultSnowyTree(21, 29),
	MakeDefaultSnowyTree(22, 30),
	MakeDefaultSnowyTree(23, 31),
	MakeDefaultSnowyTree(24, 32),
	MakeDefaultSnowyTree(25, 33),
	MakeDefaultSnowyTree(26, 34),

	/* Sub-tropic */
	MakeDefaultTree(35),
	MakeDefaultTree(36),
	MakeDefaultTree(37),
	MakeDefaultTree(38),
	MakeDefaultTree(39),
	MakeDefaultTree(40),
	MakeDefaultTree(41),
	MakeDefaultTree(42),
	MakeDefaultTree(43),
	MakeDefaultTree(44),
	MakeDefaultTree(45),
	MakeDefaultTree(46),
	MakeDefaultTree(47),
	MakeDefaultTree(48),
	MakeDefaultTree(49),
	MakeDefaultTree(50),
	MakeDefaultTree(51),
	MakeDefaultTree(52),

	/* Toyland */
	MakeDefaultTree(53),
	MakeDefaultTree(54),
	MakeDefaultTree(55),
	MakeDefaultTree(56),
	MakeDefaultTree(57),
	MakeDefaultTree(58),
	MakeDefaultTree(59),
	MakeDefaultTree(60),
	MakeDefaultTree(61),
};

static constexpr uint8_t TREE_BASE_T = 0; ///< Base tree sprite index for default temperate trees.
static constexpr uint8_t TREE_BASE_A = 19; ///< Base tree sprite index for default arctic trees.
static constexpr uint8_t TREE_BASE_S = 27; ///< Base tree sprite index for default subtropic trees.
static constexpr uint8_t TREE_BASE_Y = 45; ///< Base tree sprite index for default toyland trees.

static constexpr std::array<SoundID, 4> TREE_SOUNDS_RAINFOREST{SND_42_RAINFOREST_1, SND_43_RAINFOREST_2, SND_44_RAINFOREST_3, SND_48_RAINFOREST_4};

/** Tree tile information for original trees. */
static constexpr std::initializer_list<TreeSpec> _original_tree_specs = {
	// Temperate
	{
		.trees = {{
			{TREE_BASE_T + 6, TREE_BASE_T + 7, TREE_BASE_T + 8, TREE_BASE_T + 9},
			{TREE_BASE_T + 6, TREE_BASE_T + 9, TREE_BASE_T + 10, TREE_BASE_T + 11},
			{TREE_BASE_T + 6, TREE_BASE_T + 10, TREE_BASE_T + 7, TREE_BASE_T + 11},
			{TREE_BASE_T + 6, TREE_BASE_T + 6, TREE_BASE_T + 8, TREE_BASE_T + 10},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREE_BASE_T + 8, TREE_BASE_T + 9, TREE_BASE_T + 7, TREE_BASE_T + 6},
			{TREE_BASE_T + 8, TREE_BASE_T + 11, TREE_BASE_T + 8, TREE_BASE_T + 8},
			{TREE_BASE_T + 8, TREE_BASE_T + 6, TREE_BASE_T + 6, TREE_BASE_T + 10},
			{TREE_BASE_T + 8, TREE_BASE_T + 11, TREE_BASE_T + 9, TREE_BASE_T + 7},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREE_BASE_T + 11, TREE_BASE_T + 8, TREE_BASE_T + 11, TREE_BASE_T + 11},
			{TREE_BASE_T + 11, TREE_BASE_T + 7, TREE_BASE_T + 6, TREE_BASE_T + 6},
			{TREE_BASE_T + 11, TREE_BASE_T + 10, TREE_BASE_T + 6, TREE_BASE_T + 6},
			{TREE_BASE_T + 11, TREE_BASE_T + 9, TREE_BASE_T + 7, TREE_BASE_T + 9},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREE_BASE_T + 0, TREE_BASE_T + 6, TREE_BASE_T + 8, TREE_BASE_T + 1},
			{TREE_BASE_T + 0, TREE_BASE_T + 2, TREE_BASE_T + 11, TREE_BASE_T + 4},
			{TREE_BASE_T + 0, TREE_BASE_T + 6, TREE_BASE_T + 3, TREE_BASE_T + 10},
			{TREE_BASE_T + 0, TREE_BASE_T + 9, TREE_BASE_T + 4, TREE_BASE_T + 6},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREE_BASE_T + 4, TREE_BASE_T + 7, TREE_BASE_T + 8, TREE_BASE_T + 0},
			{TREE_BASE_T + 4, TREE_BASE_T + 5, TREE_BASE_T + 7, TREE_BASE_T + 2},
			{TREE_BASE_T + 4, TREE_BASE_T + 11, TREE_BASE_T + 6, TREE_BASE_T + 3},
			{TREE_BASE_T + 4, TREE_BASE_T + 3, TREE_BASE_T + 10, TREE_BASE_T + 6},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREE_BASE_T + 2, TREE_BASE_T + 2, TREE_BASE_T + 0, TREE_BASE_T + 2},
			{TREE_BASE_T + 2, TREE_BASE_T + 3, TREE_BASE_T + 2, TREE_BASE_T + 2},
			{TREE_BASE_T + 2, TREE_BASE_T + 5, TREE_BASE_T + 2, TREE_BASE_T + 2},
			{TREE_BASE_T + 2, TREE_BASE_T + 2, TREE_BASE_T + 2, TREE_BASE_T + 2},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREE_BASE_T + 5, TREE_BASE_T + 0, TREE_BASE_T + 1, TREE_BASE_T + 2},
			{TREE_BASE_T + 5, TREE_BASE_T + 3, TREE_BASE_T + 4, TREE_BASE_T + 2},
			{TREE_BASE_T + 5, TREE_BASE_T + 2, TREE_BASE_T + 3, TREE_BASE_T + 0},
			{TREE_BASE_T + 5, TREE_BASE_T + 5, TREE_BASE_T + 2, TREE_BASE_T + 3},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREE_BASE_T + 1, TREE_BASE_T + 4, TREE_BASE_T + 4, TREE_BASE_T + 2},
			{TREE_BASE_T + 1, TREE_BASE_T + 1, TREE_BASE_T + 2, TREE_BASE_T + 0},
			{TREE_BASE_T + 1, TREE_BASE_T + 5, TREE_BASE_T + 2, TREE_BASE_T + 2},
			{TREE_BASE_T + 1, TREE_BASE_T + 2, TREE_BASE_T + 1, TREE_BASE_T + 2},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREE_BASE_T + 12, TREE_BASE_T + 11, TREE_BASE_T + 13, TREE_BASE_T + 12},
			{TREE_BASE_T + 12, TREE_BASE_T + 17, TREE_BASE_T + 12, TREE_BASE_T + 7},
			{TREE_BASE_T + 12, TREE_BASE_T + 12, TREE_BASE_T + 12, TREE_BASE_T + 18},
			{TREE_BASE_T + 12, TREE_BASE_T + 15, TREE_BASE_T + 10, TREE_BASE_T + 14},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREE_BASE_T + 14, TREE_BASE_T + 14, TREE_BASE_T + 16, TREE_BASE_T + 14},
			{TREE_BASE_T + 14, TREE_BASE_T + 16, TREE_BASE_T + 13, TREE_BASE_T + 14},
			{TREE_BASE_T + 14, TREE_BASE_T + 12, TREE_BASE_T + 15, TREE_BASE_T + 14},
			{TREE_BASE_T + 14, TREE_BASE_T + 13, TREE_BASE_T + 18, TREE_BASE_T + 17},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREE_BASE_T + 16, TREE_BASE_T + 14, TREE_BASE_T + 16, TREE_BASE_T + 6},
			{TREE_BASE_T + 16, TREE_BASE_T + 16, TREE_BASE_T + 8, TREE_BASE_T + 9},
			{TREE_BASE_T + 16, TREE_BASE_T + 12, TREE_BASE_T + 18, TREE_BASE_T + 16},
			{TREE_BASE_T + 16, TREE_BASE_T + 16, TREE_BASE_T + 16, TREE_BASE_T + 15},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREE_BASE_T + 18, TREE_BASE_T + 18, TREE_BASE_T + 12, TREE_BASE_T + 8},
			{TREE_BASE_T + 18, TREE_BASE_T + 17, TREE_BASE_T + 18, TREE_BASE_T + 6},
			{TREE_BASE_T + 18, TREE_BASE_T + 12, TREE_BASE_T + 18, TREE_BASE_T + 15},
			{TREE_BASE_T + 18, TREE_BASE_T + 15, TREE_BASE_T + 17, TREE_BASE_T + 18},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Temperate,
	},
	// Arctic
	{
		.trees = {{
			{TREE_BASE_A + 0, TREE_BASE_A + 0, TREE_BASE_A + 0, TREE_BASE_A + 0},
			{TREE_BASE_A + 0, TREE_BASE_A + 0, TREE_BASE_A + 3, TREE_BASE_A + 5},
			{TREE_BASE_A + 0, TREE_BASE_A + 6, TREE_BASE_A + 0, TREE_BASE_A + 0},
			{TREE_BASE_A + 0, TREE_BASE_A + 5, TREE_BASE_A + 4, TREE_BASE_A + 0},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Arctic,
	},
	{
		.trees = {{
			{TREE_BASE_A + 5, TREE_BASE_A + 5, TREE_BASE_A + 5, TREE_BASE_A + 0},
			{TREE_BASE_A + 5, TREE_BASE_A + 0, TREE_BASE_A + 6, TREE_BASE_A + 4},
			{TREE_BASE_A + 5, TREE_BASE_A + 6, TREE_BASE_A + 5, TREE_BASE_A + 3},
			{TREE_BASE_A + 5, TREE_BASE_A + 5, TREE_BASE_A + 5, TREE_BASE_A + 0},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Arctic,
	},
	{
		.trees = {{
			{TREE_BASE_A + 6, TREE_BASE_A + 6, TREE_BASE_A + 6, TREE_BASE_A + 6},
			{TREE_BASE_A + 6, TREE_BASE_A + 6, TREE_BASE_A + 0, TREE_BASE_A + 0},
			{TREE_BASE_A + 6, TREE_BASE_A + 5, TREE_BASE_A + 6, TREE_BASE_A + 0},
			{TREE_BASE_A + 6, TREE_BASE_A + 6, TREE_BASE_A + 5, TREE_BASE_A + 0},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Arctic,
	},
	{
		.trees = {{
			{TREE_BASE_A + 3, TREE_BASE_A + 5, TREE_BASE_A + 4, TREE_BASE_A + 3},
			{TREE_BASE_A + 3, TREE_BASE_A + 4, TREE_BASE_A + 3, TREE_BASE_A + 0},
			{TREE_BASE_A + 3, TREE_BASE_A + 3, TREE_BASE_A + 3, TREE_BASE_A + 0},
			{TREE_BASE_A + 3, TREE_BASE_A + 3, TREE_BASE_A + 3, TREE_BASE_A + 4},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Arctic,
	},
	{
		.trees = {{
			{TREE_BASE_A + 4, TREE_BASE_A + 5, TREE_BASE_A + 1, TREE_BASE_A + 3},
			{TREE_BASE_A + 4, TREE_BASE_A + 2, TREE_BASE_A + 7, TREE_BASE_A + 6},
			{TREE_BASE_A + 4, TREE_BASE_A + 3, TREE_BASE_A + 2, TREE_BASE_A + 1},
			{TREE_BASE_A + 4, TREE_BASE_A + 2, TREE_BASE_A + 3, TREE_BASE_A + 7},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Arctic,
	},
	{
		.trees = {{
			{TREE_BASE_A + 1, TREE_BASE_A + 1, TREE_BASE_A + 7, TREE_BASE_A + 4},
			{TREE_BASE_A + 1, TREE_BASE_A + 2, TREE_BASE_A + 2, TREE_BASE_A + 0},
			{TREE_BASE_A + 1, TREE_BASE_A + 7, TREE_BASE_A + 2, TREE_BASE_A + 1},
			{TREE_BASE_A + 1, TREE_BASE_A + 0, TREE_BASE_A + 3, TREE_BASE_A + 7},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Arctic,
	},
	{
		.trees = {{
			{TREE_BASE_A + 2, TREE_BASE_A + 5, TREE_BASE_A + 7, TREE_BASE_A + 3},
			{TREE_BASE_A + 2, TREE_BASE_A + 1, TREE_BASE_A + 2, TREE_BASE_A + 6},
			{TREE_BASE_A + 2, TREE_BASE_A + 7, TREE_BASE_A + 2, TREE_BASE_A + 1},
			{TREE_BASE_A + 2, TREE_BASE_A + 4, TREE_BASE_A + 3, TREE_BASE_A + 7},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Arctic,
	},
	{
		.trees = {{
			{TREE_BASE_A + 7, TREE_BASE_A + 6, TREE_BASE_A + 7, TREE_BASE_A + 3},
			{TREE_BASE_A + 7, TREE_BASE_A + 2, TREE_BASE_A + 7, TREE_BASE_A + 5},
			{TREE_BASE_A + 7, TREE_BASE_A + 7, TREE_BASE_A + 2, TREE_BASE_A + 1},
			{TREE_BASE_A + 7, TREE_BASE_A + 4, TREE_BASE_A + 3, TREE_BASE_A + 7},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Arctic,
	},
	// Sub-tropic, rainforest
	{
		.trees = {{
			{TREE_BASE_S + 2, TREE_BASE_S + 3, TREE_BASE_S + 2, TREE_BASE_S + 4},
			{TREE_BASE_S + 2, TREE_BASE_S + 6, TREE_BASE_S + 8, TREE_BASE_S + 2},
			{TREE_BASE_S + 2, TREE_BASE_S + 2, TREE_BASE_S + 11, TREE_BASE_S + 15},
			{TREE_BASE_S + 2, TREE_BASE_S + 7, TREE_BASE_S + 2, TREE_BASE_S + 2},
		}},
		.random_sounds = TREE_SOUNDS_RAINFOREST,
		.name = STR_LAI_TREE_NAME_RAINFOREST,
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Rainforest,
	},
	{
		.trees = {{
			{TREE_BASE_S + 3, TREE_BASE_S + 3, TREE_BASE_S + 2, TREE_BASE_S + 4},
			{TREE_BASE_S + 3, TREE_BASE_S + 6, TREE_BASE_S + 3, TREE_BASE_S + 3},
			{TREE_BASE_S + 3, TREE_BASE_S + 3, TREE_BASE_S + 8, TREE_BASE_S + 17},
			{TREE_BASE_S + 3, TREE_BASE_S + 7, TREE_BASE_S + 3, TREE_BASE_S + 16},
		}},
		.random_sounds = TREE_SOUNDS_RAINFOREST,
		.name = STR_LAI_TREE_NAME_RAINFOREST,
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Rainforest,
	},
	{
		.trees = {{
			{TREE_BASE_S + 6, TREE_BASE_S + 3, TREE_BASE_S + 6, TREE_BASE_S + 5},
			{TREE_BASE_S + 6, TREE_BASE_S + 6, TREE_BASE_S + 3, TREE_BASE_S + 11},
			{TREE_BASE_S + 6, TREE_BASE_S + 2, TREE_BASE_S + 8, TREE_BASE_S + 6},
			{TREE_BASE_S + 6, TREE_BASE_S + 15, TREE_BASE_S + 3, TREE_BASE_S + 6},
		}},
		.random_sounds = TREE_SOUNDS_RAINFOREST,
		.name = STR_LAI_TREE_NAME_RAINFOREST,
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Rainforest,
	},
	{
		.trees = {{
			{TREE_BASE_S + 7, TREE_BASE_S + 7, TREE_BASE_S + 2, TREE_BASE_S + 17},
			{TREE_BASE_S + 7, TREE_BASE_S + 8, TREE_BASE_S + 3, TREE_BASE_S + 7},
			{TREE_BASE_S + 7, TREE_BASE_S + 2, TREE_BASE_S + 15, TREE_BASE_S + 6},
			{TREE_BASE_S + 7, TREE_BASE_S + 7, TREE_BASE_S + 3, TREE_BASE_S + 17},
		}},
		.random_sounds = TREE_SOUNDS_RAINFOREST,
		.name = STR_LAI_TREE_NAME_RAINFOREST,
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Rainforest,
	},
	{
		.trees = {{
			{TREE_BASE_S + 11, TREE_BASE_S + 11, TREE_BASE_S + 7, TREE_BASE_S + 7},
			{TREE_BASE_S + 11, TREE_BASE_S + 17, TREE_BASE_S + 3, TREE_BASE_S + 11},
			{TREE_BASE_S + 11, TREE_BASE_S + 3, TREE_BASE_S + 15, TREE_BASE_S + 11},
			{TREE_BASE_S + 11, TREE_BASE_S + 15, TREE_BASE_S + 3, TREE_BASE_S + 16},
		}},
		.random_sounds = TREE_SOUNDS_RAINFOREST,
		.name = STR_LAI_TREE_NAME_RAINFOREST,
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Rainforest,
	},
	{
		.trees = {{
			{TREE_BASE_S + 16, TREE_BASE_S + 16, TREE_BASE_S + 7, TREE_BASE_S + 17},
			{TREE_BASE_S + 16, TREE_BASE_S + 3, TREE_BASE_S + 4, TREE_BASE_S + 6},
			{TREE_BASE_S + 16, TREE_BASE_S + 3, TREE_BASE_S + 15, TREE_BASE_S + 11},
			{TREE_BASE_S + 16, TREE_BASE_S + 15, TREE_BASE_S + 16, TREE_BASE_S + 17},
		}},
		.random_sounds = TREE_SOUNDS_RAINFOREST,
		.name = STR_LAI_TREE_NAME_RAINFOREST,
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Rainforest,
	},
	{
		.trees = {{
			{TREE_BASE_S + 15, TREE_BASE_S + 15, TREE_BASE_S + 5, TREE_BASE_S + 3},
			{TREE_BASE_S + 15, TREE_BASE_S + 15, TREE_BASE_S + 2, TREE_BASE_S + 3},
			{TREE_BASE_S + 15, TREE_BASE_S + 3, TREE_BASE_S + 15, TREE_BASE_S + 15},
			{TREE_BASE_S + 15, TREE_BASE_S + 15, TREE_BASE_S + 16, TREE_BASE_S + 17},
		}},
		.random_sounds = TREE_SOUNDS_RAINFOREST,
		.name = STR_LAI_TREE_NAME_RAINFOREST,
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Rainforest,
	},
	// Sub-tropic, cactus
	{
		.trees = {{
			{TREE_BASE_S + 13, TREE_BASE_S + 13, TREE_BASE_S + 14, TREE_BASE_S + 13},
			{TREE_BASE_S + 13, TREE_BASE_S + 14, TREE_BASE_S + 13, TREE_BASE_S + 14},
			{TREE_BASE_S + 13, TREE_BASE_S + 14, TREE_BASE_S + 14, TREE_BASE_S + 13},
			{TREE_BASE_S + 13, TREE_BASE_S + 13, TREE_BASE_S + 13, TREE_BASE_S + 14},
		}},
		.name = STR_LAI_TREE_NAME_CACTUS_PLANTS,
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Desert,
		.probability = 12,
	},
	// Sub-tropic, non-tropical
	{
		.trees = {{
			{TREE_BASE_S + 9, TREE_BASE_S + 0, TREE_BASE_S + 9, TREE_BASE_S + 1},
			{TREE_BASE_S + 9, TREE_BASE_S + 2, TREE_BASE_S + 9, TREE_BASE_S + 10},
			{TREE_BASE_S + 9, TREE_BASE_S + 9, TREE_BASE_S + 12, TREE_BASE_S + 0},
			{TREE_BASE_S + 9, TREE_BASE_S + 12, TREE_BASE_S + 9, TREE_BASE_S + 9},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Normal,
	},
	{
		.trees = {{
			{TREE_BASE_S + 12, TREE_BASE_S + 12, TREE_BASE_S + 9, TREE_BASE_S + 0},
			{TREE_BASE_S + 12, TREE_BASE_S + 6, TREE_BASE_S + 9, TREE_BASE_S + 12},
			{TREE_BASE_S + 12, TREE_BASE_S + 9, TREE_BASE_S + 12, TREE_BASE_S + 1},
			{TREE_BASE_S + 12, TREE_BASE_S + 12, TREE_BASE_S + 9, TREE_BASE_S + 10},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Normal,
	},
	{
		.trees = {{
			{TREE_BASE_S + 0, TREE_BASE_S + 0, TREE_BASE_S + 12, TREE_BASE_S + 1},
			{TREE_BASE_S + 0, TREE_BASE_S + 7, TREE_BASE_S + 10, TREE_BASE_S + 0},
			{TREE_BASE_S + 0, TREE_BASE_S + 1, TREE_BASE_S + 17, TREE_BASE_S + 0},
			{TREE_BASE_S + 0, TREE_BASE_S + 0, TREE_BASE_S + 9, TREE_BASE_S + 16},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Normal,
	},
	{
		.trees = {{
			{TREE_BASE_S + 17, TREE_BASE_S + 1, TREE_BASE_S + 9, TREE_BASE_S + 17},
			{TREE_BASE_S + 17, TREE_BASE_S + 17, TREE_BASE_S + 9, TREE_BASE_S + 0},
			{TREE_BASE_S + 17, TREE_BASE_S + 1, TREE_BASE_S + 17, TREE_BASE_S + 0},
			{TREE_BASE_S + 17, TREE_BASE_S + 17, TREE_BASE_S + 12, TREE_BASE_S + 16},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Normal,
	},
	// Toyland
	{
		.trees = {{
			{TREE_BASE_Y + 0, TREE_BASE_Y + 0, TREE_BASE_Y + 0, TREE_BASE_Y + 0},
			{TREE_BASE_Y + 0, TREE_BASE_Y + 0, TREE_BASE_Y + 0, TREE_BASE_Y + 0},
			{TREE_BASE_Y + 0, TREE_BASE_Y + 0, TREE_BASE_Y + 0, TREE_BASE_Y + 0},
			{TREE_BASE_Y + 0, TREE_BASE_Y + 0, TREE_BASE_Y + 0, TREE_BASE_Y + 0},
		}},
		.palettes = {{
			{PALETTE_TO_RED,    PALETTE_TO_PALE_GREEN, PALETTE_TO_MAUVE, PALETTE_TO_PURPLE},
			{PAL_NONE,          PALETTE_TO_GREY,       PALETTE_TO_GREEN, PALETTE_TO_WHITE},
			{PALETTE_TO_GREEN,  PALETTE_TO_ORANGE,     PALETTE_TO_PINK,  PAL_NONE},
			{PALETTE_TO_YELLOW, PALETTE_TO_RED,        PALETTE_TO_CREAM, PALETTE_TO_RED},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Toyland,
	},
	{
		.trees = {{
			{TREE_BASE_Y + 1, TREE_BASE_Y + 1, TREE_BASE_Y + 1, TREE_BASE_Y + 1},
			{TREE_BASE_Y + 1, TREE_BASE_Y + 1, TREE_BASE_Y + 1, TREE_BASE_Y + 1},
			{TREE_BASE_Y + 1, TREE_BASE_Y + 1, TREE_BASE_Y + 1, TREE_BASE_Y + 1},
			{TREE_BASE_Y + 1, TREE_BASE_Y + 1, TREE_BASE_Y + 1, TREE_BASE_Y + 1},
		}},
		.palettes = {{
			{PAL_NONE,          PALETTE_TO_RED,        PALETTE_TO_PINK,   PALETTE_TO_PURPLE},
			{PALETTE_TO_MAUVE,  PALETTE_TO_GREEN,      PALETTE_TO_PINK,   PALETTE_TO_GREY},
			{PALETTE_TO_RED,    PALETTE_TO_PALE_GREEN, PALETTE_TO_YELLOW, PALETTE_TO_WHITE},
			{PALETTE_TO_ORANGE, PALETTE_TO_MAUVE,      PALETTE_TO_CREAM,  PALETTE_TO_BROWN},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Toyland,
	},
	{
		.trees = {{
			{TREE_BASE_Y + 2, TREE_BASE_Y + 2, TREE_BASE_Y + 2, TREE_BASE_Y + 2},
			{TREE_BASE_Y + 2, TREE_BASE_Y + 2, TREE_BASE_Y + 2, TREE_BASE_Y + 2},
			{TREE_BASE_Y + 2, TREE_BASE_Y + 2, TREE_BASE_Y + 2, TREE_BASE_Y + 2},
			{TREE_BASE_Y + 2, TREE_BASE_Y + 2, TREE_BASE_Y + 2, TREE_BASE_Y + 2},
		}},
		.palettes = {{
			{PALETTE_TO_RED,    PAL_NONE,         PALETTE_TO_ORANGE,     PALETTE_TO_GREY},
			{PALETTE_TO_ORANGE, PALETTE_TO_GREEN, PALETTE_TO_PALE_GREEN, PALETTE_TO_MAUVE},
			{PALETTE_TO_PINK,   PALETTE_TO_RED,   PALETTE_TO_GREEN,      PALETTE_TO_BROWN},
			{PALETTE_TO_GREEN,  PAL_NONE,         PALETTE_TO_RED,        PALETTE_TO_CREAM},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Toyland,
	},
	{
		.trees = {{
			{TREE_BASE_Y + 3, TREE_BASE_Y + 3, TREE_BASE_Y + 3, TREE_BASE_Y + 3},
			{TREE_BASE_Y + 3, TREE_BASE_Y + 3, TREE_BASE_Y + 3, TREE_BASE_Y + 3},
			{TREE_BASE_Y + 3, TREE_BASE_Y + 3, TREE_BASE_Y + 3, TREE_BASE_Y + 3},
			{TREE_BASE_Y + 3, TREE_BASE_Y + 3, TREE_BASE_Y + 3, TREE_BASE_Y + 3},
		}},
		/* No palettes */
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Toyland,
	},
	{
		.trees = {{
			{TREE_BASE_Y + 4, TREE_BASE_Y + 4, TREE_BASE_Y + 4, TREE_BASE_Y + 4},
			{TREE_BASE_Y + 4, TREE_BASE_Y + 4, TREE_BASE_Y + 4, TREE_BASE_Y + 4},
			{TREE_BASE_Y + 4, TREE_BASE_Y + 4, TREE_BASE_Y + 4, TREE_BASE_Y + 4},
			{TREE_BASE_Y + 4, TREE_BASE_Y + 4, TREE_BASE_Y + 4, TREE_BASE_Y + 4},
		}},
		.palettes = {{
			{PALETTE_TO_PINK,  PALETTE_TO_RED,        PALETTE_TO_ORANGE, PALETTE_TO_MAUVE},
			{PALETTE_TO_RED,   PAL_NONE,              PALETTE_TO_GREY,   PALETTE_TO_CREAM},
			{PALETTE_TO_GREEN, PALETTE_TO_BROWN,      PALETTE_TO_PINK,   PALETTE_TO_RED},
			{PAL_NONE,         PALETTE_TO_PALE_GREEN, PALETTE_TO_ORANGE, PALETTE_TO_RED},

		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Toyland,
	},
	{
		.trees = {{
			{TREE_BASE_Y + 5, TREE_BASE_Y + 5, TREE_BASE_Y + 5, TREE_BASE_Y + 5},
			{TREE_BASE_Y + 5, TREE_BASE_Y + 5, TREE_BASE_Y + 5, TREE_BASE_Y + 5},
			{TREE_BASE_Y + 5, TREE_BASE_Y + 5, TREE_BASE_Y + 5, TREE_BASE_Y + 5},
			{TREE_BASE_Y + 5, TREE_BASE_Y + 5, TREE_BASE_Y + 5, TREE_BASE_Y + 5},
		}},
		.palettes = {{
			{PALETTE_TO_RED,   PALETTE_TO_PINK,  PALETTE_TO_GREEN,      PAL_NONE},
			{PALETTE_TO_GREEN, PALETTE_TO_BROWN, PALETTE_TO_PURPLE,     PALETTE_TO_GREY},
			{PALETTE_TO_MAUVE, PALETTE_TO_CREAM, PALETTE_TO_ORANGE,     PALETTE_TO_RED},
			{PAL_NONE,         PALETTE_TO_RED,   PALETTE_TO_PALE_GREEN, PALETTE_TO_PINK},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Toyland,
	},
	{
		.trees = {{
			{TREE_BASE_Y + 6, TREE_BASE_Y + 6, TREE_BASE_Y + 6, TREE_BASE_Y + 6},
			{TREE_BASE_Y + 6, TREE_BASE_Y + 6, TREE_BASE_Y + 6, TREE_BASE_Y + 6},
			{TREE_BASE_Y + 6, TREE_BASE_Y + 6, TREE_BASE_Y + 6, TREE_BASE_Y + 6},
			{TREE_BASE_Y + 6, TREE_BASE_Y + 6, TREE_BASE_Y + 6, TREE_BASE_Y + 6},
		}},
		.palettes = {{
			{PALETTE_TO_YELLOW, PALETTE_TO_RED,        PALETTE_TO_WHITE, PALETTE_TO_CREAM},
			{PALETTE_TO_RED,    PALETTE_TO_PALE_GREEN, PALETTE_TO_BROWN, PALETTE_TO_YELLOW},
			{PAL_NONE,          PALETTE_TO_PURPLE,     PALETTE_TO_GREEN, PALETTE_TO_YELLOW},
			{PALETTE_TO_PINK,   PALETTE_TO_CREAM,      PAL_NONE,         PALETTE_TO_GREY},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Toyland,
	},
	{
		.trees = {{
			{TREE_BASE_Y + 7, TREE_BASE_Y + 7, TREE_BASE_Y + 7, TREE_BASE_Y + 7},
			{TREE_BASE_Y + 7, TREE_BASE_Y + 7, TREE_BASE_Y + 7, TREE_BASE_Y + 7},
			{TREE_BASE_Y + 7, TREE_BASE_Y + 7, TREE_BASE_Y + 7, TREE_BASE_Y + 7},
			{TREE_BASE_Y + 7, TREE_BASE_Y + 7, TREE_BASE_Y + 7, TREE_BASE_Y + 7},
		}},
		.palettes = {{
			{PALETTE_TO_YELLOW, PALETTE_TO_GREY,       PALETTE_TO_PURPLE, PALETTE_TO_BROWN},
			{PALETTE_TO_GREEN,  PAL_NONE,              PALETTE_TO_CREAM,  PALETTE_TO_WHITE},
			{PALETTE_TO_RED,    PALETTE_TO_PALE_GREEN, PALETTE_TO_MAUVE,  PALETTE_TO_RED},
			{PALETTE_TO_PINK,   PALETTE_TO_ORANGE,     PALETTE_TO_GREEN,  PALETTE_TO_YELLOW},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Toyland,
	},
	{
		.trees = {{
			{TREE_BASE_Y + 8, TREE_BASE_Y + 8, TREE_BASE_Y + 8, TREE_BASE_Y + 8},
			{TREE_BASE_Y + 8, TREE_BASE_Y + 8, TREE_BASE_Y + 8, TREE_BASE_Y + 8},
			{TREE_BASE_Y + 8, TREE_BASE_Y + 8, TREE_BASE_Y + 8, TREE_BASE_Y + 8},
			{TREE_BASE_Y + 8, TREE_BASE_Y + 8, TREE_BASE_Y + 8, TREE_BASE_Y + 8},
		}},
		.palettes = {{
			{PALETTE_TO_RED,    PALETTE_TO_PINK,       PALETTE_TO_BROWN, PALETTE_TO_WHITE},
			{PALETTE_TO_GREEN,  PALETTE_TO_ORANGE,     PALETTE_TO_GREY,  PALETTE_TO_MAUVE},
			{PALETTE_TO_YELLOW, PALETTE_TO_PALE_GREEN, PAL_NONE,         PALETTE_TO_CREAM},
			{PALETTE_TO_GREY,   PALETTE_TO_RED,        PALETTE_TO_WHITE, PAL_NONE},
		}},
		.name = STR_LAI_TREE_NAME_TREES,
		.landscapes = LandscapeType::Toyland,
	},
};

#endif /* TREE_LAND_H */
