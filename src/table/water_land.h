/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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

/**
 * Constructor macro of a DrawTileSpriteSpan structure
 * @param img   Ground sprite without palette of the tile
 * @param dtss  Sequence child sprites of the tile
 */
#define TILE_SPRITE_LINE(img, dtss) { {img, PAL_NONE}, dtss },

static const DrawTileSeqStruct _shipdepot_display_ne_seq[] = {
	TILE_SEQ_LINE( 0, 15, 0, 16, 1, 0x14, 0xFE8 | (1 << PALETTE_MODIFIER_COLOUR))
};

static const DrawTileSeqStruct _shipdepot_display_sw_seq[] = {
	TILE_SEQ_LINE( 0,  0, 0, 16, 1, 0x14, 0xFEA)
	TILE_SEQ_LINE( 0, 15, 0, 16, 1, 0x14, 0xFE6 | (1 << PALETTE_MODIFIER_COLOUR))
};

static const DrawTileSeqStruct _shipdepot_display_nw_seq[] = {
	TILE_SEQ_LINE( 15, 0, 0, 1, 0x10, 0x14, 0xFE9 | (1 << PALETTE_MODIFIER_COLOUR))
};

static const DrawTileSeqStruct _shipdepot_display_se_seq[] = {
	TILE_SEQ_LINE(  0, 0, 0, 1, 16, 0x14, 0xFEB)
	TILE_SEQ_LINE( 15, 0, 0, 1, 16, 0x14, 0xFE7 | (1 << PALETTE_MODIFIER_COLOUR))
};

static const DrawTileSpriteSpan _shipdepot_display_data[][to_underlying(DepotPart::End)] = {
	{ // AXIS_X
		TILE_SPRITE_LINE(0xFDD, _shipdepot_display_ne_seq) // DepotPart::North
		TILE_SPRITE_LINE(0xFDD, _shipdepot_display_sw_seq) // DepotPart::South
	},
	{ // AXIS_Y
		TILE_SPRITE_LINE(0xFDD, _shipdepot_display_nw_seq) // DepotPart::North
		TILE_SPRITE_LINE(0xFDD, _shipdepot_display_se_seq) // DepotPart::South
	},
};

static constexpr uint8_t LOCK_HEIGHT_LOWER_REAR = 6; ///< Sub-tile height of rear wall of lower part.
static constexpr uint8_t LOCK_HEIGHT_LOWER_FRONT = 10; ///< Sub-tile height of front wall of lower part.
static constexpr uint8_t LOCK_HEIGHT_MIDDLE_REAR = 6; ///< Sub-tile height of rear wall of middle part.
static constexpr uint8_t LOCK_HEIGHT_MIDDLE_FRONT = 10; ///< Sub-tile height of front wall of middle part.
static constexpr uint8_t LOCK_HEIGHT_UPPER_REAR = 6; ///< Sub-tile height of rear wall of upper part.
static constexpr uint8_t LOCK_HEIGHT_UPPER_FRONT = 6; ///< Sub-tile height of front wall of upper part.

static const DrawTileSeqStruct _lock_display_middle_ne_seq[] = {
	TILE_SEQ_LINE(0,  0, 0, TILE_SIZE, 1, LOCK_HEIGHT_MIDDLE_REAR,  0 + 1)
	TILE_SEQ_LINE(0, 15, 0, TILE_SIZE, 1, LOCK_HEIGHT_MIDDLE_FRONT, 4 + 1)
};

static const DrawTileSeqStruct _lock_display_middle_se_seq[] = {
	TILE_SEQ_LINE( 0, 0, 0, 1, TILE_SIZE, LOCK_HEIGHT_MIDDLE_REAR,  0)
	TILE_SEQ_LINE(15, 0, 0, 1, TILE_SIZE, LOCK_HEIGHT_MIDDLE_FRONT, 4)
};

static const DrawTileSeqStruct _lock_display_middle_sw_seq[] = {
	TILE_SEQ_LINE(0,  0, 0, TILE_SIZE, 1, LOCK_HEIGHT_MIDDLE_REAR,  0 + 2)
	TILE_SEQ_LINE(0, 15, 0, TILE_SIZE, 1, LOCK_HEIGHT_MIDDLE_FRONT, 4 + 2)
};

static const DrawTileSeqStruct _lock_display_middle_nw_seq[] = {
	TILE_SEQ_LINE( 0, 0, 0, 1, TILE_SIZE, LOCK_HEIGHT_MIDDLE_REAR,  0 + 3)
	TILE_SEQ_LINE(15, 0, 0, 1, TILE_SIZE, LOCK_HEIGHT_MIDDLE_FRONT, 4 + 3)
};

static const DrawTileSeqStruct _lock_display_lower_ne_seq[] = {
	TILE_SEQ_LINE(0,  0, 0, TILE_SIZE, 1, LOCK_HEIGHT_LOWER_REAR,   8 + 1)
	TILE_SEQ_LINE(0, 15, 0, TILE_SIZE, 1, LOCK_HEIGHT_LOWER_FRONT, 12 + 1)
};

static const DrawTileSeqStruct _lock_display_lower_se_seq[] = {
	TILE_SEQ_LINE( 0, 0, 0, 1, TILE_SIZE, LOCK_HEIGHT_LOWER_REAR,   8)
	TILE_SEQ_LINE(15, 0, 0, 1, TILE_SIZE, LOCK_HEIGHT_LOWER_FRONT, 12)
};

static const DrawTileSeqStruct _lock_display_lower_sw_seq[] = {
	TILE_SEQ_LINE(0,  0, 0, TILE_SIZE, 1, LOCK_HEIGHT_LOWER_REAR,   8 + 2)
	TILE_SEQ_LINE(0, 15, 0, TILE_SIZE, 1, LOCK_HEIGHT_LOWER_FRONT, 12 + 2)
};

static const DrawTileSeqStruct _lock_display_lower_nw_seq[] = {
	TILE_SEQ_LINE( 0, 0, 0, 1, TILE_SIZE, LOCK_HEIGHT_LOWER_REAR,   8 + 3)
	TILE_SEQ_LINE(15, 0, 0, 1, TILE_SIZE, LOCK_HEIGHT_LOWER_FRONT, 12 + 3)
};

static const DrawTileSeqStruct _lock_display_upper_ne_seq[] = {
	TILE_SEQ_LINE(0,  0, 0, TILE_SIZE, 1, LOCK_HEIGHT_UPPER_REAR,  16 + 1)
	TILE_SEQ_LINE(0, 15, 0, TILE_SIZE, 1, LOCK_HEIGHT_UPPER_FRONT, 20 + 1)
};

static const DrawTileSeqStruct _lock_display_upper_se_seq[] = {
	TILE_SEQ_LINE( 0, 0, 0, 1, TILE_SIZE, LOCK_HEIGHT_UPPER_REAR,  16)
	TILE_SEQ_LINE(15, 0, 0, 1, TILE_SIZE, LOCK_HEIGHT_UPPER_FRONT, 20)
};

static const DrawTileSeqStruct _lock_display_upper_sw_seq[] = {
	TILE_SEQ_LINE(0,  0, 0, TILE_SIZE, 1, LOCK_HEIGHT_UPPER_REAR,  16 + 2)
	TILE_SEQ_LINE(0, 15, 0, TILE_SIZE, 1, LOCK_HEIGHT_UPPER_FRONT, 20 + 2)
};

static const DrawTileSeqStruct _lock_display_upper_nw_seq[] = {
	TILE_SEQ_LINE( 0, 0, 0, 1, TILE_SIZE, LOCK_HEIGHT_UPPER_REAR,  16 + 3)
	TILE_SEQ_LINE(15, 0, 0, 1, TILE_SIZE, LOCK_HEIGHT_UPPER_FRONT, 20 + 3)
};

static const DrawTileSpriteSpan _lock_display_data[][DIAGDIR_END] = {
	{ // LockPart::Middle
		TILE_SPRITE_LINE(1, _lock_display_middle_ne_seq) // NE
		TILE_SPRITE_LINE(0, _lock_display_middle_se_seq) // SE
		TILE_SPRITE_LINE(2, _lock_display_middle_sw_seq) // SW
		TILE_SPRITE_LINE(3, _lock_display_middle_nw_seq) // NW
	},

	{ // LockPart::Lower
		TILE_SPRITE_LINE(0xFDD, _lock_display_lower_ne_seq) // NE
		TILE_SPRITE_LINE(0xFDD, _lock_display_lower_se_seq) // SE
		TILE_SPRITE_LINE(0xFDD, _lock_display_lower_sw_seq) // SW
		TILE_SPRITE_LINE(0xFDD, _lock_display_lower_nw_seq) // NW
	},

	{ // LockPart::Upper
		TILE_SPRITE_LINE(0xFDD, _lock_display_upper_ne_seq) // NE
		TILE_SPRITE_LINE(0xFDD, _lock_display_upper_se_seq) // SE
		TILE_SPRITE_LINE(0xFDD, _lock_display_upper_sw_seq) // SW
		TILE_SPRITE_LINE(0xFDD, _lock_display_upper_nw_seq) // NW
	},
};

#undef TILE_SEQ_LINE
#undef TILE_SPRITE_LINE
