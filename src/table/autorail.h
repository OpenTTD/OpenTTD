/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file autorail.h Highlight/sprite information for autorail. */

/* Rail selection types (directions):
 *  / \    / \    / \    / \   / \   / \
 * /  /\  /\  \  /===\  /   \ /|  \ /  |\
 * \/  /  \  \/  \   /  \===/ \|  / \  |/
 *  \ /    \ /    \ /    \ /   \ /   \ /
 *   0      1      2      3     4     5
 */

/* mark invalid tiles red */
#define RED(c) -c

/* table maps each of the six rail directions and tileh combinations to a sprite
 * invalid entries are required to make sure that this array can be quickly accessed */
static const int _AutorailTilehSprite[][6] = {
/* type   0        1        2        3        4        5 */
	{       0,       8,      16,      25,      34,      42 }, // tileh = 0
	{       5,      13, RED(22), RED(31),      35,      42 }, // tileh = 1
	{       5,      10,      16,      26, RED(38), RED(46) }, // tileh = 2
	{       5,       9, RED(23),      26,      35, RED(46) }, // tileh = 3
	{       2,      10, RED(19), RED(28),      34,      43 }, // tileh = 4
	{       1,       9,      17,      26,      35,      43 }, // tileh = 5
	{       1,      10, RED(20),      26, RED(38),      43 }, // tileh = 6
	{       1,       9,      17,      26,      35,      43 }, // tileh = 7
	{       2,      13,      17,      25, RED(40), RED(48) }, // tileh = 8
	{       1,      13,      17, RED(32),      35, RED(48) }, // tileh = 9
	{       1,       9,      17,      26,      35,      43 }, // tileh = 10
	{       1,       9,      17,      26,      35,      43 }, // tileh = 11
	{       2,       9,      17, RED(29), RED(40),      43 }, // tileh = 12
	{       1,       9,      17,      26,      35,      43 }, // tileh = 13
	{       1,       9,      17,      26,      35,      43 }, // tileh = 14
	{       0,       1,       2,       3,       4,       5 }, // invalid (15)
	{       0,       1,       2,       3,       4,       5 }, // invalid (16)
	{       0,       1,       2,       3,       4,       5 }, // invalid (17)
	{       0,       1,       2,       3,       4,       5 }, // invalid (18)
	{       0,       1,       2,       3,       4,       5 }, // invalid (19)
	{       0,       1,       2,       3,       4,       5 }, // invalid (20)
	{       0,       1,       2,       3,       4,       5 }, // invalid (21)
	{       0,       1,       2,       3,       4,       5 }, // invalid (22)
	{       6,      11,      17,      27, RED(39), RED(47) }, // tileh = 23
	{       0,       1,       2,       3,       4,       5 }, // invalid (24)
	{       0,       1,       2,       3,       4,       5 }, // invalid (25)
	{       0,       1,       2,       3,       4,       5 }, // invalid (26)
	{       7,      15, RED(24), RED(33),      36,      44 }, // tileh = 27
	{       0,       1,       2,       3,       4,       5 }, // invalid (28)
	{       3,      14,      18,      26, RED(41), RED(49) }, // tileh = 29
	{       4,      12, RED(21), RED(30),      37,      45 }  // tileh = 30
};
#undef RED


/* maps each pixel of a tile (16x16) to a selection type
 * (0,0) is the top corner, (16,16) the bottom corner */
static const HighLightStyle _autorail_piece[][16] = {
	{ HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR },
	{ HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR },
	{ HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR },
	{ HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR },
	{ HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR },
	{ HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_HU, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR, HT_DIR_VR },
	{ HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_Y, HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y  },
	{ HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y, HT_DIR_X, HT_DIR_Y, HT_DIR_Y, HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y  },
	{ HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y, HT_DIR_Y, HT_DIR_X, HT_DIR_Y, HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y  },
	{ HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y,  HT_DIR_Y  },
	{ HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL },
	{ HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL },
	{ HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL },
	{ HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL },
	{ HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL },
	{ HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_VL, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_X, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL, HT_DIR_HL }
};
