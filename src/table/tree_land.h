/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tree_land.h Sprites to use and how to display them for tree tiles. */

#ifndef TREE_LAND_H
#define TREE_LAND_H

static const byte _tree_base_by_landscape[4] = {0, 12, 20, 32};
static const byte _tree_count_by_landscape[4] = {12, 8, 12, 9};

struct TreePos {
	uint8_t x;
	uint8_t y;
};

static const TreePos _tree_layout_xy[][4] = {
	{ { 9, 3 }, { 1, 8 }, { 0, 0 }, { 8, 9 } },
	{ { 4, 4 }, { 9, 1 }, { 6, 9 }, { 0, 9 } },
	{ { 9, 1 }, { 0, 9 }, { 6, 6 }, { 3, 0 } },
	{ { 3, 9 }, { 8, 2 }, { 9, 9 }, { 1, 5 } }
};

static const PalSpriteID _tree_layout_sprite[164 + (79 - 48 + 1)][4] = {
	{ { 0x652, PAL_NONE }, { 0x659, PAL_NONE }, { 0x660, PAL_NONE }, { 0x667, PAL_NONE } }, // 0
	{ { 0x652, PAL_NONE }, { 0x667, PAL_NONE }, { 0x66e, PAL_NONE }, { 0x675, PAL_NONE } }, // 1
	{ { 0x652, PAL_NONE }, { 0x66e, PAL_NONE }, { 0x659, PAL_NONE }, { 0x675, PAL_NONE } }, // 2
	{ { 0x652, PAL_NONE }, { 0x652, PAL_NONE }, { 0x660, PAL_NONE }, { 0x66e, PAL_NONE } }, // 3
	{ { 0x660, PAL_NONE }, { 0x667, PAL_NONE }, { 0x659, PAL_NONE }, { 0x652, PAL_NONE } }, // 4
	{ { 0x660, PAL_NONE }, { 0x675, PAL_NONE }, { 0x660, PAL_NONE }, { 0x660, PAL_NONE } }, // 5
	{ { 0x660, PAL_NONE }, { 0x652, PAL_NONE }, { 0x652, PAL_NONE }, { 0x66e, PAL_NONE } }, // 6
	{ { 0x660, PAL_NONE }, { 0x675, PAL_NONE }, { 0x667, PAL_NONE }, { 0x659, PAL_NONE } }, // 7
	{ { 0x675, PAL_NONE }, { 0x660, PAL_NONE }, { 0x675, PAL_NONE }, { 0x675, PAL_NONE } }, // 8
	{ { 0x675, PAL_NONE }, { 0x659, PAL_NONE }, { 0x652, PAL_NONE }, { 0x652, PAL_NONE } }, // 9
	{ { 0x675, PAL_NONE }, { 0x66e, PAL_NONE }, { 0x652, PAL_NONE }, { 0x652, PAL_NONE } }, // 10
	{ { 0x675, PAL_NONE }, { 0x667, PAL_NONE }, { 0x659, PAL_NONE }, { 0x667, PAL_NONE } }, // 11
	{ { 0x628, PAL_NONE }, { 0x652, PAL_NONE }, { 0x660, PAL_NONE }, { 0x62f, PAL_NONE } }, // 12
	{ { 0x628, PAL_NONE }, { 0x636, PAL_NONE }, { 0x675, PAL_NONE }, { 0x644, PAL_NONE } }, // 13
	{ { 0x628, PAL_NONE }, { 0x652, PAL_NONE }, { 0x63d, PAL_NONE }, { 0x66e, PAL_NONE } }, // 14
	{ { 0x628, PAL_NONE }, { 0x667, PAL_NONE }, { 0x644, PAL_NONE }, { 0x652, PAL_NONE } }, // 15
	{ { 0x644, PAL_NONE }, { 0x659, PAL_NONE }, { 0x660, PAL_NONE }, { 0x628, PAL_NONE } }, // 16
	{ { 0x644, PAL_NONE }, { 0x64b, PAL_NONE }, { 0x659, PAL_NONE }, { 0x636, PAL_NONE } }, // 17
	{ { 0x644, PAL_NONE }, { 0x675, PAL_NONE }, { 0x652, PAL_NONE }, { 0x63d, PAL_NONE } }, // 18
	{ { 0x644, PAL_NONE }, { 0x63d, PAL_NONE }, { 0x66e, PAL_NONE }, { 0x652, PAL_NONE } }, // 19
	{ { 0x636, PAL_NONE }, { 0x636, PAL_NONE }, { 0x628, PAL_NONE }, { 0x636, PAL_NONE } }, // 20
	{ { 0x636, PAL_NONE }, { 0x63d, PAL_NONE }, { 0x636, PAL_NONE }, { 0x636, PAL_NONE } }, // 21
	{ { 0x636, PAL_NONE }, { 0x64b, PAL_NONE }, { 0x636, PAL_NONE }, { 0x636, PAL_NONE } }, // 22
	{ { 0x636, PAL_NONE }, { 0x636, PAL_NONE }, { 0x636, PAL_NONE }, { 0x636, PAL_NONE } }, // 23
	{ { 0x64b, PAL_NONE }, { 0x628, PAL_NONE }, { 0x62f, PAL_NONE }, { 0x636, PAL_NONE } }, // 24
	{ { 0x64b, PAL_NONE }, { 0x63d, PAL_NONE }, { 0x644, PAL_NONE }, { 0x636, PAL_NONE } }, // 25
	{ { 0x64b, PAL_NONE }, { 0x636, PAL_NONE }, { 0x63d, PAL_NONE }, { 0x628, PAL_NONE } }, // 26
	{ { 0x64b, PAL_NONE }, { 0x64b, PAL_NONE }, { 0x636, PAL_NONE }, { 0x63d, PAL_NONE } }, // 27
	{ { 0x62f, PAL_NONE }, { 0x644, PAL_NONE }, { 0x644, PAL_NONE }, { 0x636, PAL_NONE } }, // 28
	{ { 0x62f, PAL_NONE }, { 0x62f, PAL_NONE }, { 0x636, PAL_NONE }, { 0x628, PAL_NONE } }, // 29
	{ { 0x62f, PAL_NONE }, { 0x64b, PAL_NONE }, { 0x636, PAL_NONE }, { 0x636, PAL_NONE } }, // 30
	{ { 0x62f, PAL_NONE }, { 0x636, PAL_NONE }, { 0x62f, PAL_NONE }, { 0x636, PAL_NONE } }, // 31
	{ { 0x67c, PAL_NONE }, { 0x675, PAL_NONE }, { 0x683, PAL_NONE }, { 0x67c, PAL_NONE } }, // 32
	{ { 0x67c, PAL_NONE }, { 0x69f, PAL_NONE }, { 0x67c, PAL_NONE }, { 0x659, PAL_NONE } }, // 33
	{ { 0x67c, PAL_NONE }, { 0x67c, PAL_NONE }, { 0x67c, PAL_NONE }, { 0x6a6, PAL_NONE } }, // 34
	{ { 0x67c, PAL_NONE }, { 0x691, PAL_NONE }, { 0x66e, PAL_NONE }, { 0x68a, PAL_NONE } }, // 35
	{ { 0x68a, PAL_NONE }, { 0x68a, PAL_NONE }, { 0x698, PAL_NONE }, { 0x68a, PAL_NONE } }, // 36
	{ { 0x68a, PAL_NONE }, { 0x698, PAL_NONE }, { 0x683, PAL_NONE }, { 0x68a, PAL_NONE } }, // 37
	{ { 0x68a, PAL_NONE }, { 0x67c, PAL_NONE }, { 0x691, PAL_NONE }, { 0x68a, PAL_NONE } }, // 38
	{ { 0x68a, PAL_NONE }, { 0x683, PAL_NONE }, { 0x6a6, PAL_NONE }, { 0x69f, PAL_NONE } }, // 39
	{ { 0x698, PAL_NONE }, { 0x68a, PAL_NONE }, { 0x698, PAL_NONE }, { 0x652, PAL_NONE } }, // 40
	{ { 0x698, PAL_NONE }, { 0x698, PAL_NONE }, { 0x660, PAL_NONE }, { 0x667, PAL_NONE } }, // 41
	{ { 0x698, PAL_NONE }, { 0x67c, PAL_NONE }, { 0x6a6, PAL_NONE }, { 0x698, PAL_NONE } }, // 42
	{ { 0x698, PAL_NONE }, { 0x698, PAL_NONE }, { 0x698, PAL_NONE }, { 0x691, PAL_NONE } }, // 43
	{ { 0x6a6, PAL_NONE }, { 0x6a6, PAL_NONE }, { 0x67c, PAL_NONE }, { 0x660, PAL_NONE } }, // 44
	{ { 0x6a6, PAL_NONE }, { 0x69f, PAL_NONE }, { 0x6a6, PAL_NONE }, { 0x652, PAL_NONE } }, // 45
	{ { 0x6a6, PAL_NONE }, { 0x67c, PAL_NONE }, { 0x6a6, PAL_NONE }, { 0x691, PAL_NONE } }, // 46
	{ { 0x6a6, PAL_NONE }, { 0x691, PAL_NONE }, { 0x69f, PAL_NONE }, { 0x6a6, PAL_NONE } }, // 47
	{ { 0x6ad, PAL_NONE }, { 0x6ad, PAL_NONE }, { 0x6ad, PAL_NONE }, { 0x6ad, PAL_NONE } }, // 48
	{ { 0x6ad, PAL_NONE }, { 0x6ad, PAL_NONE }, { 0x6c2, PAL_NONE }, { 0x6d0, PAL_NONE } }, // 49
	{ { 0x6ad, PAL_NONE }, { 0x6d7, PAL_NONE }, { 0x6ad, PAL_NONE }, { 0x6ad, PAL_NONE } }, // 50
	{ { 0x6ad, PAL_NONE }, { 0x6d0, PAL_NONE }, { 0x6c9, PAL_NONE }, { 0x6ad, PAL_NONE } }, // 51
	{ { 0x6d0, PAL_NONE }, { 0x6d0, PAL_NONE }, { 0x6d0, PAL_NONE }, { 0x6ad, PAL_NONE } }, // 52
	{ { 0x6d0, PAL_NONE }, { 0x6ad, PAL_NONE }, { 0x6d7, PAL_NONE }, { 0x6c9, PAL_NONE } }, // 53
	{ { 0x6d0, PAL_NONE }, { 0x6d7, PAL_NONE }, { 0x6d0, PAL_NONE }, { 0x6c2, PAL_NONE } }, // 54
	{ { 0x6d0, PAL_NONE }, { 0x6d0, PAL_NONE }, { 0x6d0, PAL_NONE }, { 0x6ad, PAL_NONE } }, // 55
	{ { 0x6d7, PAL_NONE }, { 0x6d7, PAL_NONE }, { 0x6d7, PAL_NONE }, { 0x6d7, PAL_NONE } }, // 56
	{ { 0x6d7, PAL_NONE }, { 0x6d7, PAL_NONE }, { 0x6ad, PAL_NONE }, { 0x6ad, PAL_NONE } }, // 57
	{ { 0x6d7, PAL_NONE }, { 0x6d0, PAL_NONE }, { 0x6d7, PAL_NONE }, { 0x6ad, PAL_NONE } }, // 58
	{ { 0x6d7, PAL_NONE }, { 0x6d7, PAL_NONE }, { 0x6d0, PAL_NONE }, { 0x6ad, PAL_NONE } }, // 59
	{ { 0x6c2, PAL_NONE }, { 0x6d0, PAL_NONE }, { 0x6c9, PAL_NONE }, { 0x6c2, PAL_NONE } }, // 60
	{ { 0x6c2, PAL_NONE }, { 0x6c9, PAL_NONE }, { 0x6c2, PAL_NONE }, { 0x6ad, PAL_NONE } }, // 61
	{ { 0x6c2, PAL_NONE }, { 0x6c2, PAL_NONE }, { 0x6c2, PAL_NONE }, { 0x6ad, PAL_NONE } }, // 62
	{ { 0x6c2, PAL_NONE }, { 0x6c2, PAL_NONE }, { 0x6c2, PAL_NONE }, { 0x6c9, PAL_NONE } }, // 63
	{ { 0x6c9, PAL_NONE }, { 0x6d0, PAL_NONE }, { 0x6b4, PAL_NONE }, { 0x6c2, PAL_NONE } }, // 64
	{ { 0x6c9, PAL_NONE }, { 0x6bb, PAL_NONE }, { 0x6de, PAL_NONE }, { 0x6d7, PAL_NONE } }, // 65
	{ { 0x6c9, PAL_NONE }, { 0x6c2, PAL_NONE }, { 0x6bb, PAL_NONE }, { 0x6b4, PAL_NONE } }, // 66
	{ { 0x6c9, PAL_NONE }, { 0x6bb, PAL_NONE }, { 0x6c2, PAL_NONE }, { 0x6de, PAL_NONE } }, // 67
	{ { 0x6b4, PAL_NONE }, { 0x6b4, PAL_NONE }, { 0x6de, PAL_NONE }, { 0x6c9, PAL_NONE } }, // 68
	{ { 0x6b4, PAL_NONE }, { 0x6bb, PAL_NONE }, { 0x6bb, PAL_NONE }, { 0x6ad, PAL_NONE } }, // 69
	{ { 0x6b4, PAL_NONE }, { 0x6de, PAL_NONE }, { 0x6bb, PAL_NONE }, { 0x6b4, PAL_NONE } }, // 70
	{ { 0x6b4, PAL_NONE }, { 0x6ad, PAL_NONE }, { 0x6c2, PAL_NONE }, { 0x6de, PAL_NONE } }, // 71
	{ { 0x6bb, PAL_NONE }, { 0x6d0, PAL_NONE }, { 0x6de, PAL_NONE }, { 0x6c2, PAL_NONE } }, // 72
	{ { 0x6bb, PAL_NONE }, { 0x6b4, PAL_NONE }, { 0x6bb, PAL_NONE }, { 0x6d7, PAL_NONE } }, // 73
	{ { 0x6bb, PAL_NONE }, { 0x6de, PAL_NONE }, { 0x6bb, PAL_NONE }, { 0x6b4, PAL_NONE } }, // 74
	{ { 0x6bb, PAL_NONE }, { 0x6c9, PAL_NONE }, { 0x6c2, PAL_NONE }, { 0x6de, PAL_NONE } }, // 75
	{ { 0x6de, PAL_NONE }, { 0x6d7, PAL_NONE }, { 0x6de, PAL_NONE }, { 0x6c2, PAL_NONE } }, // 76
	{ { 0x6de, PAL_NONE }, { 0x6bb, PAL_NONE }, { 0x6de, PAL_NONE }, { 0x6d0, PAL_NONE } }, // 77
	{ { 0x6de, PAL_NONE }, { 0x6de, PAL_NONE }, { 0x6bb, PAL_NONE }, { 0x6b4, PAL_NONE } }, // 78
	{ { 0x6de, PAL_NONE }, { 0x6c9, PAL_NONE }, { 0x6c2, PAL_NONE }, { 0x6de, PAL_NONE } }, // 79
	{ { 0x72b, PAL_NONE }, { 0x732, PAL_NONE }, { 0x72b, PAL_NONE }, { 0x739, PAL_NONE } }, // 80
	{ { 0x72b, PAL_NONE }, { 0x747, PAL_NONE }, { 0x755, PAL_NONE }, { 0x72b, PAL_NONE } }, // 81
	{ { 0x72b, PAL_NONE }, { 0x72b, PAL_NONE }, { 0x76a, PAL_NONE }, { 0x786, PAL_NONE } }, // 82
	{ { 0x72b, PAL_NONE }, { 0x74e, PAL_NONE }, { 0x72b, PAL_NONE }, { 0x72b, PAL_NONE } }, // 83
	{ { 0x732, PAL_NONE }, { 0x732, PAL_NONE }, { 0x72b, PAL_NONE }, { 0x739, PAL_NONE } }, // 84
	{ { 0x732, PAL_NONE }, { 0x747, PAL_NONE }, { 0x732, PAL_NONE }, { 0x732, PAL_NONE } }, // 85
	{ { 0x732, PAL_NONE }, { 0x732, PAL_NONE }, { 0x755, PAL_NONE }, { 0x794, PAL_NONE } }, // 86
	{ { 0x732, PAL_NONE }, { 0x74e, PAL_NONE }, { 0x732, PAL_NONE }, { 0x78d, PAL_NONE } }, // 87
	{ { 0x747, PAL_NONE }, { 0x732, PAL_NONE }, { 0x747, PAL_NONE }, { 0x740, PAL_NONE } }, // 88
	{ { 0x747, PAL_NONE }, { 0x747, PAL_NONE }, { 0x732, PAL_NONE }, { 0x76a, PAL_NONE } }, // 89
	{ { 0x747, PAL_NONE }, { 0x72b, PAL_NONE }, { 0x755, PAL_NONE }, { 0x747, PAL_NONE } }, // 90
	{ { 0x747, PAL_NONE }, { 0x786, PAL_NONE }, { 0x732, PAL_NONE }, { 0x747, PAL_NONE } }, // 91
	{ { 0x74e, PAL_NONE }, { 0x74e, PAL_NONE }, { 0x72b, PAL_NONE }, { 0x794, PAL_NONE } }, // 92
	{ { 0x74e, PAL_NONE }, { 0x755, PAL_NONE }, { 0x732, PAL_NONE }, { 0x74e, PAL_NONE } }, // 93
	{ { 0x74e, PAL_NONE }, { 0x72b, PAL_NONE }, { 0x786, PAL_NONE }, { 0x747, PAL_NONE } }, // 94
	{ { 0x74e, PAL_NONE }, { 0x74e, PAL_NONE }, { 0x732, PAL_NONE }, { 0x794, PAL_NONE } }, // 95
	{ { 0x76a, PAL_NONE }, { 0x76a, PAL_NONE }, { 0x74e, PAL_NONE }, { 0x74e, PAL_NONE } }, // 96
	{ { 0x76a, PAL_NONE }, { 0x794, PAL_NONE }, { 0x732, PAL_NONE }, { 0x76a, PAL_NONE } }, // 97
	{ { 0x76a, PAL_NONE }, { 0x732, PAL_NONE }, { 0x786, PAL_NONE }, { 0x76a, PAL_NONE } }, // 98
	{ { 0x76a, PAL_NONE }, { 0x786, PAL_NONE }, { 0x732, PAL_NONE }, { 0x78d, PAL_NONE } }, // 99
	{ { 0x78d, PAL_NONE }, { 0x78d, PAL_NONE }, { 0x74e, PAL_NONE }, { 0x794, PAL_NONE } }, // 100
	{ { 0x78d, PAL_NONE }, { 0x732, PAL_NONE }, { 0x739, PAL_NONE }, { 0x747, PAL_NONE } }, // 101
	{ { 0x78d, PAL_NONE }, { 0x732, PAL_NONE }, { 0x786, PAL_NONE }, { 0x76a, PAL_NONE } }, // 102
	{ { 0x78d, PAL_NONE }, { 0x786, PAL_NONE }, { 0x78d, PAL_NONE }, { 0x794, PAL_NONE } }, // 103
	{ { 0x786, PAL_NONE }, { 0x786, PAL_NONE }, { 0x740, PAL_NONE }, { 0x732, PAL_NONE } }, // 104
	{ { 0x786, PAL_NONE }, { 0x786, PAL_NONE }, { 0x72b, PAL_NONE }, { 0x732, PAL_NONE } }, // 105
	{ { 0x786, PAL_NONE }, { 0x732, PAL_NONE }, { 0x786, PAL_NONE }, { 0x786, PAL_NONE } }, // 106
	{ { 0x786, PAL_NONE }, { 0x786, PAL_NONE }, { 0x78d, PAL_NONE }, { 0x794, PAL_NONE } }, // 107
	{ { 0x778, PAL_NONE }, { 0x778, PAL_NONE }, { 0x77f, PAL_NONE }, { 0x778, PAL_NONE } }, // 108
	{ { 0x778, PAL_NONE }, { 0x77f, PAL_NONE }, { 0x778, PAL_NONE }, { 0x77f, PAL_NONE } }, // 109
	{ { 0x778, PAL_NONE }, { 0x77f, PAL_NONE }, { 0x77f, PAL_NONE }, { 0x778, PAL_NONE } }, // 110
	{ { 0x778, PAL_NONE }, { 0x778, PAL_NONE }, { 0x778, PAL_NONE }, { 0x77f, PAL_NONE } }, // 111
	{ { 0x75c, PAL_NONE }, { 0x71d, PAL_NONE }, { 0x75c, PAL_NONE }, { 0x724, PAL_NONE } }, // 112
	{ { 0x75c, PAL_NONE }, { 0x72b, PAL_NONE }, { 0x75c, PAL_NONE }, { 0x763, PAL_NONE } }, // 113
	{ { 0x75c, PAL_NONE }, { 0x75c, PAL_NONE }, { 0x771, PAL_NONE }, { 0x71d, PAL_NONE } }, // 114
	{ { 0x75c, PAL_NONE }, { 0x771, PAL_NONE }, { 0x75c, PAL_NONE }, { 0x75c, PAL_NONE } }, // 115
	{ { 0x771, PAL_NONE }, { 0x771, PAL_NONE }, { 0x75c, PAL_NONE }, { 0x71d, PAL_NONE } }, // 116
	{ { 0x771, PAL_NONE }, { 0x747, PAL_NONE }, { 0x75c, PAL_NONE }, { 0x771, PAL_NONE } }, // 117
	{ { 0x771, PAL_NONE }, { 0x75c, PAL_NONE }, { 0x771, PAL_NONE }, { 0x724, PAL_NONE } }, // 118
	{ { 0x771, PAL_NONE }, { 0x771, PAL_NONE }, { 0x75c, PAL_NONE }, { 0x763, PAL_NONE } }, // 119
	{ { 0x71d, PAL_NONE }, { 0x71d, PAL_NONE }, { 0x771, PAL_NONE }, { 0x724, PAL_NONE } }, // 120
	{ { 0x71d, PAL_NONE }, { 0x74e, PAL_NONE }, { 0x763, PAL_NONE }, { 0x71d, PAL_NONE } }, // 121
	{ { 0x71d, PAL_NONE }, { 0x724, PAL_NONE }, { 0x794, PAL_NONE }, { 0x71d, PAL_NONE } }, // 122
	{ { 0x71d, PAL_NONE }, { 0x71d, PAL_NONE }, { 0x75c, PAL_NONE }, { 0x78d, PAL_NONE } }, // 123
	{ { 0x794, PAL_NONE }, { 0x724, PAL_NONE }, { 0x75c, PAL_NONE }, { 0x794, PAL_NONE } }, // 124
	{ { 0x794, PAL_NONE }, { 0x794, PAL_NONE }, { 0x75c, PAL_NONE }, { 0x71d, PAL_NONE } }, // 125
	{ { 0x794, PAL_NONE }, { 0x724, PAL_NONE }, { 0x794, PAL_NONE }, { 0x71d, PAL_NONE } }, // 126
	{ { 0x794, PAL_NONE }, { 0x794, PAL_NONE }, { 0x771, PAL_NONE }, { 0x78d, PAL_NONE } }, // 127
	{ { 0x79b, PALETTE_TO_RED },    { 0x79b, PALETTE_TO_PALE_GREEN }, { 0x79b, PALETTE_TO_MAUVE },      { 0x79b, PALETTE_TO_PURPLE } }, // 128
	{ { 0x79b, PAL_NONE },          { 0x79b, PALETTE_TO_GREY },       { 0x79b, PALETTE_TO_GREEN },      { 0x79b, PALETTE_TO_WHITE } },  // 129
	{ { 0x79b, PALETTE_TO_GREEN },  { 0x79b, PALETTE_TO_ORANGE },     { 0x79b, PALETTE_TO_PINK },       { 0x79b, PAL_NONE } },          // 130
	{ { 0x79b, PALETTE_TO_YELLOW }, { 0x79b, PALETTE_TO_RED },        { 0x79b, PALETTE_TO_CREAM },      { 0x79b, PALETTE_TO_RED } },    // 131
	{ { 0x7a2, PAL_NONE },          { 0x7a2, PALETTE_TO_RED },        { 0x7a2, PALETTE_TO_PINK },       { 0x7a2, PALETTE_TO_PURPLE } }, // 132
	{ { 0x7a2, PALETTE_TO_MAUVE },  { 0x7a2, PALETTE_TO_GREEN },      { 0x7a2, PALETTE_TO_PINK },       { 0x7a2, PALETTE_TO_GREY } },   // 133
	{ { 0x7a2, PALETTE_TO_RED },    { 0x7a2, PALETTE_TO_PALE_GREEN }, { 0x7a2, PALETTE_TO_YELLOW },     { 0x7a2, PALETTE_TO_WHITE } },  // 134
	{ { 0x7a2, PALETTE_TO_ORANGE }, { 0x7a2, PALETTE_TO_MAUVE },      { 0x7a2, PALETTE_TO_CREAM },      { 0x7a2, PALETTE_TO_BROWN } },  // 135
	{ { 0x7a9, PALETTE_TO_RED },    { 0x7a9, PAL_NONE },              { 0x7a9, PALETTE_TO_ORANGE },     { 0x7a9, PALETTE_TO_GREY } },   // 136
	{ { 0x7a9, PALETTE_TO_ORANGE }, { 0x7a9, PALETTE_TO_GREEN },      { 0x7a9, PALETTE_TO_PALE_GREEN }, { 0x7a9, PALETTE_TO_MAUVE } },  // 137
	{ { 0x7a9, PALETTE_TO_PINK },   { 0x7a9, PALETTE_TO_RED },        { 0x7a9, PALETTE_TO_GREEN },      { 0x7a9, PALETTE_TO_BROWN } },  // 138
	{ { 0x7a9, PALETTE_TO_GREEN },  { 0x7a9, PAL_NONE },              { 0x7a9, PALETTE_TO_RED },        { 0x7a9, PALETTE_TO_CREAM } },  // 139
	{ { 0x7b0, PAL_NONE },          { 0x7b0, PAL_NONE },              { 0x7b0, PAL_NONE },              { 0x7b0, PAL_NONE } },          // 140
	{ { 0x7b0, PAL_NONE },          { 0x7b0, PAL_NONE },              { 0x7b0, PAL_NONE },              { 0x7b0, PAL_NONE } },          // 141
	{ { 0x7b0, PAL_NONE },          { 0x7b0, PAL_NONE },              { 0x7b0, PAL_NONE },              { 0x7b0, PAL_NONE } },          // 142
	{ { 0x7b0, PAL_NONE },          { 0x7b0, PAL_NONE },              { 0x7b0, PAL_NONE },              { 0x7b0, PAL_NONE } },          // 143
	{ { 0x7b7, PALETTE_TO_PINK },   { 0x7b7, PALETTE_TO_RED },        { 0x7b7, PALETTE_TO_ORANGE },     { 0x7b7, PALETTE_TO_MAUVE } },  // 144
	{ { 0x7b7, PALETTE_TO_RED },    { 0x7b7, PAL_NONE },              { 0x7b7, PALETTE_TO_GREY },       { 0x7b7, PALETTE_TO_CREAM } },  // 145
	{ { 0x7b7, PALETTE_TO_GREEN },  { 0x7b7, PALETTE_TO_BROWN },      { 0x7b7, PALETTE_TO_PINK },       { 0x7b7, PALETTE_TO_RED } },    // 146
	{ { 0x7b7, PAL_NONE },          { 0x7b7, PALETTE_TO_PALE_GREEN }, { 0x7b7, PALETTE_TO_ORANGE },     { 0x7b7, PALETTE_TO_RED } },    // 147
	{ { 0x7be, PALETTE_TO_RED },    { 0x7be, PALETTE_TO_PINK },       { 0x7be, PALETTE_TO_GREEN },      { 0x7be, PAL_NONE } },          // 148
	{ { 0x7be, PALETTE_TO_GREEN },  { 0x7be, PALETTE_TO_BROWN },      { 0x7be, PALETTE_TO_PURPLE },     { 0x7be, PALETTE_TO_GREY } },   // 149
	{ { 0x7be, PALETTE_TO_MAUVE },  { 0x7be, PALETTE_TO_CREAM },      { 0x7be, PALETTE_TO_ORANGE },     { 0x7be, PALETTE_TO_RED } },    // 150
	{ { 0x7be, PAL_NONE },          { 0x7be, PALETTE_TO_RED },        { 0x7be, PALETTE_TO_PALE_GREEN }, { 0x7be, PALETTE_TO_PINK } },   // 151
	{ { 0x7c5, PALETTE_TO_YELLOW }, { 0x7c5, PALETTE_TO_RED },        { 0x7c5, PALETTE_TO_WHITE },      { 0x7c5, PALETTE_TO_CREAM } },  // 152
	{ { 0x7c5, PALETTE_TO_RED },    { 0x7c5, PALETTE_TO_PALE_GREEN }, { 0x7c5, PALETTE_TO_BROWN },      { 0x7c5, PALETTE_TO_YELLOW } }, // 153
	{ { 0x7c5, PAL_NONE },          { 0x7c5, PALETTE_TO_PURPLE },     { 0x7c5, PALETTE_TO_GREEN },      { 0x7c5, PALETTE_TO_YELLOW } }, // 154
	{ { 0x7c5, PALETTE_TO_PINK },   { 0x7c5, PALETTE_TO_CREAM },      { 0x7c5, PAL_NONE },              { 0x7c5, PALETTE_TO_GREY } },   // 155
	{ { 0x7cc, PALETTE_TO_YELLOW }, { 0x7cc, PALETTE_TO_GREY },       { 0x7cc, PALETTE_TO_PURPLE },     { 0x7cc, PALETTE_TO_BROWN } },  // 156
	{ { 0x7cc, PALETTE_TO_GREEN },  { 0x7cc, PAL_NONE },              { 0x7cc, PALETTE_TO_CREAM },      { 0x7cc, PALETTE_TO_WHITE } },  // 157
	{ { 0x7cc, PALETTE_TO_RED },    { 0x7cc, PALETTE_TO_PALE_GREEN }, { 0x7cc, PALETTE_TO_MAUVE },      { 0x7cc, PALETTE_TO_RED } },    // 158
	{ { 0x7cc, PALETTE_TO_PINK },   { 0x7cc, PALETTE_TO_ORANGE },     { 0x7cc, PALETTE_TO_GREEN },      { 0x7cc, PALETTE_TO_YELLOW } }, // 159
	{ { 0x7d3, PALETTE_TO_RED },    { 0x7d3, PALETTE_TO_PINK },       { 0x7d3, PALETTE_TO_BROWN },      { 0x7d3, PALETTE_TO_WHITE } },  // 160
	{ { 0x7d3, PALETTE_TO_GREEN },  { 0x7d3, PALETTE_TO_ORANGE },     { 0x7d3, PALETTE_TO_GREY },       { 0x7d3, PALETTE_TO_MAUVE } },  // 161
	{ { 0x7d3, PALETTE_TO_YELLOW }, { 0x7d3, PALETTE_TO_PALE_GREEN }, { 0x7d3, PAL_NONE },              { 0x7d3, PALETTE_TO_CREAM } },  // 162
	{ { 0x7d3, PALETTE_TO_GREY },   { 0x7d3, PALETTE_TO_RED },        { 0x7d3, PALETTE_TO_WHITE },      { 0x7d3, PAL_NONE } },          // 163
	/* the extra things follow */
	{ { 0x6e5, PAL_NONE }, { 0x6e5, PAL_NONE }, { 0x6e5, PAL_NONE }, { 0x6e5, PAL_NONE } }, // 0
	{ { 0x6e5, PAL_NONE }, { 0x6e5, PAL_NONE }, { 0x6fa, PAL_NONE }, { 0x708, PAL_NONE } }, // 1
	{ { 0x6e5, PAL_NONE }, { 0x70f, PAL_NONE }, { 0x6e5, PAL_NONE }, { 0x6e5, PAL_NONE } }, // 2
	{ { 0x6e5, PAL_NONE }, { 0x708, PAL_NONE }, { 0x701, PAL_NONE }, { 0x6e5, PAL_NONE } }, // 3
	{ { 0x708, PAL_NONE }, { 0x708, PAL_NONE }, { 0x708, PAL_NONE }, { 0x6e5, PAL_NONE } }, // 4
	{ { 0x708, PAL_NONE }, { 0x6e5, PAL_NONE }, { 0x70f, PAL_NONE }, { 0x701, PAL_NONE } }, // 5
	{ { 0x708, PAL_NONE }, { 0x70f, PAL_NONE }, { 0x708, PAL_NONE }, { 0x6fa, PAL_NONE } }, // 6
	{ { 0x708, PAL_NONE }, { 0x708, PAL_NONE }, { 0x708, PAL_NONE }, { 0x6e5, PAL_NONE } }, // 7
	{ { 0x70f, PAL_NONE }, { 0x70f, PAL_NONE }, { 0x70f, PAL_NONE }, { 0x70f, PAL_NONE } }, // 8
	{ { 0x70f, PAL_NONE }, { 0x70f, PAL_NONE }, { 0x6e5, PAL_NONE }, { 0x6e5, PAL_NONE } }, // 9
	{ { 0x70f, PAL_NONE }, { 0x708, PAL_NONE }, { 0x70f, PAL_NONE }, { 0x6e5, PAL_NONE } }, // 10
	{ { 0x70f, PAL_NONE }, { 0x70f, PAL_NONE }, { 0x708, PAL_NONE }, { 0x6e5, PAL_NONE } }, // 11
	{ { 0x6fa, PAL_NONE }, { 0x708, PAL_NONE }, { 0x701, PAL_NONE }, { 0x6fa, PAL_NONE } }, // 12
	{ { 0x6fa, PAL_NONE }, { 0x701, PAL_NONE }, { 0x6fa, PAL_NONE }, { 0x6e5, PAL_NONE } }, // 13
	{ { 0x6fa, PAL_NONE }, { 0x6fa, PAL_NONE }, { 0x6fa, PAL_NONE }, { 0x6e5, PAL_NONE } }, // 14
	{ { 0x6fa, PAL_NONE }, { 0x6fa, PAL_NONE }, { 0x6fa, PAL_NONE }, { 0x701, PAL_NONE } }, // 15
	{ { 0x701, PAL_NONE }, { 0x708, PAL_NONE }, { 0x6ec, PAL_NONE }, { 0x6fa, PAL_NONE } }, // 16
	{ { 0x701, PAL_NONE }, { 0x6f3, PAL_NONE }, { 0x716, PAL_NONE }, { 0x70f, PAL_NONE } }, // 17
	{ { 0x701, PAL_NONE }, { 0x6fa, PAL_NONE }, { 0x6f3, PAL_NONE }, { 0x6ec, PAL_NONE } }, // 18
	{ { 0x701, PAL_NONE }, { 0x6f3, PAL_NONE }, { 0x6fa, PAL_NONE }, { 0x716, PAL_NONE } }, // 19
	{ { 0x6ec, PAL_NONE }, { 0x6ec, PAL_NONE }, { 0x716, PAL_NONE }, { 0x701, PAL_NONE } }, // 20
	{ { 0x6ec, PAL_NONE }, { 0x6f3, PAL_NONE }, { 0x6f3, PAL_NONE }, { 0x6e5, PAL_NONE } }, // 21
	{ { 0x6ec, PAL_NONE }, { 0x716, PAL_NONE }, { 0x6f3, PAL_NONE }, { 0x6ec, PAL_NONE } }, // 22
	{ { 0x6ec, PAL_NONE }, { 0x6e5, PAL_NONE }, { 0x6fa, PAL_NONE }, { 0x716, PAL_NONE } }, // 23
	{ { 0x6f3, PAL_NONE }, { 0x708, PAL_NONE }, { 0x716, PAL_NONE }, { 0x6fa, PAL_NONE } }, // 24
	{ { 0x6f3, PAL_NONE }, { 0x6ec, PAL_NONE }, { 0x6f3, PAL_NONE }, { 0x70f, PAL_NONE } }, // 25
	{ { 0x6f3, PAL_NONE }, { 0x716, PAL_NONE }, { 0x6f3, PAL_NONE }, { 0x6ec, PAL_NONE } }, // 26
	{ { 0x6f3, PAL_NONE }, { 0x701, PAL_NONE }, { 0x6fa, PAL_NONE }, { 0x716, PAL_NONE } }, // 27
	{ { 0x716, PAL_NONE }, { 0x70f, PAL_NONE }, { 0x716, PAL_NONE }, { 0x6fa, PAL_NONE } }, // 28
	{ { 0x716, PAL_NONE }, { 0x6f3, PAL_NONE }, { 0x716, PAL_NONE }, { 0x708, PAL_NONE } }, // 29
	{ { 0x716, PAL_NONE }, { 0x716, PAL_NONE }, { 0x6f3, PAL_NONE }, { 0x6ec, PAL_NONE } }, // 30
	{ { 0x716, PAL_NONE }, { 0x701, PAL_NONE }, { 0x6fa, PAL_NONE }, { 0x716, PAL_NONE } }, // 31
};

#endif /* TREE_LAND_H */
