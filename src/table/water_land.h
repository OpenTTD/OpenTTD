/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file water_land.h Sprites to use and how to display them for water tiles (depots/locks). */

/**
 * Constructor macro for an image without a palette in a DrawTileSeqStruct array.
 * @param dx  Offset in x direction
 * @param dy  Offset in y direction
 * @param dz  Offset in z direction
 * @param sx  Size in x direction
 * @param sy  Size in y direction
 * @param sz  Size in z direction
 * @param img Sprite to draw
 */
#define TILE_SEQ_LINE(dx, dy, dz, sx, sy, sz, img) { dx, dy, dz, sx, sy, sz, {img, PAL_NONE} },

/** Constructor macro for a terminating DrawTileSeqStruct entry in an array */
#define TILE_SEQ_END() { (int8_t)0x80, 0, 0, 0, 0, 0, {0, 0} }

/**
 * Constructor macro of a DrawTileSprites structure
 * @param img   Ground sprite without palette of the tile
 * @param dtss  Sequence child sprites of the tile
 */
#define TILE_SPRITE_LINE(img, dtss) { {img, PAL_NONE}, dtss },

static const DrawTileSeqStruct _shipdepot_display_seq_1[] = {
	TILE_SEQ_LINE( 0, 15, 0, 16, 1, 0x14, 0xFE8 | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _shipdepot_display_seq_2[] = {
	TILE_SEQ_LINE( 0,  0, 0, 16, 1, 0x14, 0xFEA)
	TILE_SEQ_LINE( 0, 15, 0, 16, 1, 0x14, 0xFE6 | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _shipdepot_display_seq_3[] = {
	TILE_SEQ_LINE( 15, 0, 0, 1, 0x10, 0x14, 0xFE9 | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _shipdepot_display_seq_4[] = {
	TILE_SEQ_LINE(  0, 0, 0, 1, 16, 0x14, 0xFEB)
	TILE_SEQ_LINE( 15, 0, 0, 1, 16, 0x14, 0xFE7 | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSprites _shipdepot_display_data[][DEPOT_PART_END] = {
	{ // AXIS_X
		TILE_SPRITE_LINE(0xFDD, _shipdepot_display_seq_1) // DEPOT_PART_NORTH
		TILE_SPRITE_LINE(0xFDD, _shipdepot_display_seq_2) // DEPOT_PART_SOUTH
	},
	{ // AXIS_Y
		TILE_SPRITE_LINE(0xFDD, _shipdepot_display_seq_3) // DEPOT_PART_NORTH
		TILE_SPRITE_LINE(0xFDD, _shipdepot_display_seq_4) // DEPOT_PART_SOUTH
	},
};

static const DrawTileSeqStruct _lock_display_seq_0[] = {
	TILE_SEQ_LINE( 0,   0, 0, 0x10, 1, 0x14, 0 + 1)
	TILE_SEQ_LINE( 0, 0xF, 0, 0x10, 1, 0x14, 4 + 1)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _lock_display_seq_1[] = {
	TILE_SEQ_LINE(   0, 0, 0, 1, 0x10, 0x14, 0)
	TILE_SEQ_LINE( 0xF, 0, 0, 1, 0x10, 0x14, 4)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _lock_display_seq_2[] = {
	TILE_SEQ_LINE( 0,   0, 0, 0x10, 1, 0x14, 0 + 2)
	TILE_SEQ_LINE( 0, 0xF, 0, 0x10, 1, 0x14, 4 + 2)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _lock_display_seq_3[] = {
	TILE_SEQ_LINE(   0, 0, 0, 1, 0x10, 0x14, 0 + 3)
	TILE_SEQ_LINE( 0xF, 0, 0, 1, 0x10, 0x14, 4 + 3)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _lock_display_seq_0b[] = {
	TILE_SEQ_LINE( 0,   0, 0, 0x10, 1, 0x14, 8 + 1)
	TILE_SEQ_LINE( 0, 0xF, 0, 0x10, 1, 0x14, 12 + 1)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _lock_display_seq_1b[] = {
	TILE_SEQ_LINE(   0, 0, 0, 0x1, 0x10, 0x14, 8)
	TILE_SEQ_LINE( 0xF, 0, 0, 0x1, 0x10, 0x14, 12)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _lock_display_seq_2b[] = {
	TILE_SEQ_LINE( 0,   0, 0, 0x10, 1, 0x14, 8 + 2)
	TILE_SEQ_LINE( 0, 0xF, 0, 0x10, 1, 0x14, 12 + 2)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _lock_display_seq_3b[] = {
	TILE_SEQ_LINE(   0, 0, 0, 1, 0x10, 0x14, 8 + 3)
	TILE_SEQ_LINE( 0xF, 0, 0, 1, 0x10, 0x14, 12 + 3)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _lock_display_seq_0t[] = {
	TILE_SEQ_LINE( 0,   0, 0, 0x10, 1, 0x14, 16 + 1)
	TILE_SEQ_LINE( 0, 0xF, 0, 0x10, 1, 0x14, 20 + 1)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _lock_display_seq_1t[] = {
	TILE_SEQ_LINE(   0, 0, 0, 0x1, 0x10, 0x14, 16)
	TILE_SEQ_LINE( 0xF, 0, 0, 0x1, 0x10, 0x14, 20)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _lock_display_seq_2t[] = {
	TILE_SEQ_LINE( 0,   0, 0, 0x10, 1, 0x14, 16 + 2)
	TILE_SEQ_LINE( 0, 0xF, 0, 0x10, 1, 0x14, 20 + 2)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _lock_display_seq_3t[] = {
	TILE_SEQ_LINE(   0, 0, 0, 1, 0x10, 0x14, 16 + 3)
	TILE_SEQ_LINE( 0xF, 0, 0, 1, 0x10, 0x14, 20 + 3)
	TILE_SEQ_END()
};

static const DrawTileSprites _lock_display_data[][DIAGDIR_END] = {
	{ // LOCK_PART_MIDDLE
		TILE_SPRITE_LINE(1, _lock_display_seq_0) // NE
		TILE_SPRITE_LINE(0, _lock_display_seq_1) // SE
		TILE_SPRITE_LINE(2, _lock_display_seq_2) // SW
		TILE_SPRITE_LINE(3, _lock_display_seq_3) // NW
	},

	{ // LOCK_PART_LOWER
		TILE_SPRITE_LINE(0xFDD, _lock_display_seq_0b) // NE
		TILE_SPRITE_LINE(0xFDD, _lock_display_seq_1b) // SE
		TILE_SPRITE_LINE(0xFDD, _lock_display_seq_2b) // SW
		TILE_SPRITE_LINE(0xFDD, _lock_display_seq_3b) // NW
	},

	{ // LOCK_PART_UPPER
		TILE_SPRITE_LINE(0xFDD, _lock_display_seq_0t) // NE
		TILE_SPRITE_LINE(0xFDD, _lock_display_seq_1t) // SE
		TILE_SPRITE_LINE(0xFDD, _lock_display_seq_2t) // SW
		TILE_SPRITE_LINE(0xFDD, _lock_display_seq_3t) // NW
	},
};

#undef TILE_SEQ_LINE
#undef TILE_SEQ_END
#undef TILE_SPRITE_LINE
