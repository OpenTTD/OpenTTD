/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file autoroad.h Highlight/sprite information for autoroad. */

/* Road selection types (directions) for isometric diamond-shaped tiles:
 *  / \    / \    / \    / \   / \   / \
 * /  /\  /\  \  /  /\  /\  \ /   \ /   \
 * \/  /  \  \/  \   /  \   / \  \/ \/  /
 *  \ /    \ /    \ /    \ /   \ /   \ /
 *   0      1      2      3     4     5
 *
 *  0 = X full tile [SW-NE] (X)
 *  1 = Y full tile [NW-SE] (Y)
 *  2 = X half tile NE edge (X_NE)
 *  3 = Y half tile NW edge (Y_NW)
 *  4 = Y half tile SE edge (Y_SE)
 *  5 = X half tile SW edge (X_SW)
 */

/**
 * Table maps each of the six road directions and tileh combinations to a sprite.
 * Invalid entries are required to make sure that this array can be quickly accessed.
 * Sprites 0-5 are flat, sprites 6-9 are for sloped tiles (NE, SE, SW, NW slopes).
 */
static const int _AutoroadTilehSprite[][6] = {
/* typ   X(0)     Y(1)  X_NE(2)  Y_NW(3)  Y_SE(4)  X_SW(5) */
	{      1,       0,       4,       3,       5,       2 }, // tileh = 0 (SLOPE_FLAT)
	{      8,       9,       8,       3,       9,       2 }, // tileh = 1 (SLOPE_W)
	{      8,       7,       8,       7,       5,       2 }, // tileh = 2 (SLOPE_S)
	{      8,       0,       8,       3,       5,       2 }, // tileh = 3 (SLOPE_SW)
	{      6,       7,       4,       7,       5,       6 }, // tileh = 4 (SLOPE_E)
	{      1,       0,       4,       3,       5,       2 }, // tileh = 5 (SLOPE_EW)
	{      1,       7,       4,       7,       5,       2 }, // tileh = 6 (SLOPE_SE)
	{      1,       0,       4,       3,       5,       2 }, // tileh = 7 (SLOPE_WSE)
	{      6,       9,       4,       3,       9,       6 }, // tileh = 8 (SLOPE_N)
	{      1,       9,       4,       3,       9,       2 }, // tileh = 9 (SLOPE_NW)
	{      1,       0,       4,       3,       5,       2 }, // tileh = 10 (SLOPE_NS)
	{      1,       0,       4,       3,       5,       2 }, // tileh = 11 (SLOPE_ENW)
	{      6,       0,       4,       3,       5,       6 }, // tileh = 12 (SLOPE_NE)
	{      1,       0,       4,       3,       5,       2 }, // tileh = 13 (SLOPE_SEN)
	{      1,       0,       4,       3,       5,       2 }, // tileh = 14 (SLOPE_NWS)
	{      0,       0,       0,       0,       0,       0 }, // invalid (15)
	{      0,       0,       0,       0,       0,       0 }, // invalid (16)
	{      0,       0,       0,       0,       0,       0 }, // invalid (17)
	{      0,       0,       0,       0,       0,       0 }, // invalid (18)
	{      0,       0,       0,       0,       0,       0 }, // invalid (19)
	{      0,       0,       0,       0,       0,       0 }, // invalid (20)
	{      0,       0,       0,       0,       0,       0 }, // invalid (21)
	{      0,       0,       0,       0,       0,       0 }, // invalid (22)
	{      8,       7,       8,       7,       5,       2 }, // tileh = 23 (SLOPE_STEEP_S)
	{      0,       0,       0,       0,       0,       0 }, // invalid (24)
	{      0,       0,       0,       0,       0,       0 }, // invalid (25)
	{      0,       0,       0,       0,       0,       0 }, // invalid (26)
	{      8,       9,       8,       3,       9,       2 }, // tileh = 27 (SLOPE_STEEP_W)
	{      0,       0,       0,       0,       0,       0 }, // invalid (28)
	{      6,       9,       4,       3,       9,       6 }, // tileh = 29 (SLOPE_STEEP_N)
	{      6,       7,       4,       7,       5,       6 }  // tileh = 30 (SLOPE_STEEP_E)
};

/**
 * Table maps each of the six road directions and tileh combinations to a Z-offset.
 * Baseline offset for sprites, for on flat terrain, is Z=7.
 */
static const int _AutoroadTilehZOffset[][6] = {
/* type  X(0)     Y(1)  X_NE(2)  Y_NW(3)  Y_SE(4)  X_SW(5) */
	{      7,       7,       7,       7,       7,       7 }, // tileh = 0 (SLOPE_FLAT)
	{      7,      15,       7,      15,      15,      15 }, // tileh = 1 (SLOPE_W)
	{      7,       7,       7,       7,      15,      15 }, // tileh = 2 (SLOPE_S)
	{      7,      15,       7,      15,      15,      15 }, // tileh = 3 (SLOPE_SW)
	{     15,       7,      15,       7,      15,      15 }, // tileh = 4 (SLOPE_E)
	{     15,      15,      15,      15,      15,      15 }, // tileh = 5 (SLOPE_EW)
	{     15,       7,      15,       7,      15,      15 }, // tileh = 6 (SLOPE_SE)
	{     15,      15,      15,      15,      15,      15 }, // tileh = 7 (SLOPE_WSE)
	{     15,      15,      15,      15,      15,      15 }, // tileh = 8 (SLOPE_N)
	{     15,      15,      15,      15,      15,      15 }, // tileh = 9 (SLOPE_NW)
	{     15,      15,      15,      15,      15,      15 }, // tileh = 10 (SLOPE_NS)
	{     15,      15,      15,      15,      15,      15 }, // tileh = 11 (SLOPE_ENW)
	{     15,      15,      15,      15,      15,      15 }, // tileh = 12 (SLOPE_NE)
	{     15,      15,      15,      15,      15,      15 }, // tileh = 13 (SLOPE_SEN)
	{     15,      15,      15,      15,      15,      15 }, // tileh = 14 (SLOPE_NWS)
	{      0,       0,       0,       0,       0,       0 }, // invalid (15)
	{      0,       0,       0,       0,       0,       0 }, // invalid (16)
	{      0,       0,       0,       0,       0,       0 }, // invalid (17)
	{      0,       0,       0,       0,       0,       0 }, // invalid (18)
	{      0,       0,       0,       0,       0,       0 }, // invalid (19)
	{      0,       0,       0,       0,       0,       0 }, // invalid (20)
	{      0,       0,       0,       0,       0,       0 }, // invalid (21)
	{      0,       0,       0,       0,       0,       0 }, // invalid (22)
	{     15,      15,      15,      15,      23,      23 }, // tileh = 23 (SLOPE_STEEP_S)
	{      0,       0,       0,       0,       0,       0 }, // invalid (24)
	{      0,       0,       0,       0,       0,       0 }, // invalid (25)
	{      0,       0,       0,       0,       0,       0 }, // invalid (26)
	{     15,      23,      15,      23,      23,      23 }, // tileh = 27 (SLOPE_STEEP_W)
	{      0,       0,       0,       0,       0,       0 }, // invalid (28)
	{     23,      23,      23,      23,      23,      23 }, // tileh = 29 (SLOPE_STEEP_N)
	{     23,      15,      23,      15,      23,      23 }  // tileh = 30 (SLOPE_STEEP_E)
};


/** Maps each pixel of a tile (16x16) to a selection type (0,0) is the top corner, (16,16) the bottom corner. */
static const HighLightStyle _autoroad_piece[][16] = {
	{ HT_RD_DIR_Y_NW, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE },
	{ HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_Y_SE },
	{ HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE },
	{ HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE },
	{ HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE },
	{ HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE },
	{ HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_X_NE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE },
	{ HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_X_NE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE },
	{ HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE },
	{ HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_X_SW, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE },
	{ HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE },
	{ HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE },
	{ HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE },
	{ HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE },
	{ HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_Y_SE, HT_RD_DIR_Y_SE },
	{ HT_RD_DIR_Y_NW, HT_RD_DIR_Y_NW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_X_SW, HT_RD_DIR_Y_SE }
};

static_assert(std::size(_AutoroadTilehSprite[0]) == HT_RD_DIR_END, "Autoroad sprite table column count must match HT_RD_DIR_END");
static_assert(std::size(_AutoroadTilehZOffset[0]) == HT_RD_DIR_END, "Autoroad Z-offset table column count must match HT_RD_DIR_END");
