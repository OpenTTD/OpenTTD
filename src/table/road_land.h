/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file road_land.h Sprite constructs for road depots. */

#define TILE_SEQ_LINE(img, pal, dx, dy, sx, sy) { dx, dy, 0, sx, sy, 20, {img, pal} },
#define TILE_SEQ_END() { (int8)0x80, 0, 0, 0, 0, 0, {0, 0} }

static const DrawTileSeqStruct _road_depot_NE[] = {
	TILE_SEQ_LINE(0x584 | (1 << PALETTE_MODIFIER_COLOUR), PAL_NONE, 0, 15, 16, 1)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _road_depot_SE[] = {
	TILE_SEQ_LINE(0x580 | (1 << PALETTE_MODIFIER_COLOUR), PAL_NONE, 0, 0, 1, 16)
	TILE_SEQ_LINE(0x581 | (1 << PALETTE_MODIFIER_COLOUR), PAL_NONE, 15, 0, 1, 16)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _road_depot_SW[] = {
	TILE_SEQ_LINE(0x582 | (1 << PALETTE_MODIFIER_COLOUR), PAL_NONE, 0, 0, 16, 1)
	TILE_SEQ_LINE(0x583 | (1 << PALETTE_MODIFIER_COLOUR), PAL_NONE, 0, 15, 16, 1)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _road_depot_NW[] = {
	TILE_SEQ_LINE(0x585 | (1 << PALETTE_MODIFIER_COLOUR), PAL_NONE, 15, 0, 1, 16)
	TILE_SEQ_END()
};

static const DrawTileSprites _road_depot[] = {
	{ {0xA4A, PAL_NONE}, _road_depot_NE },
	{ {0xA4A, PAL_NONE}, _road_depot_SE },
	{ {0xA4A, PAL_NONE}, _road_depot_SW },
	{ {0xA4A, PAL_NONE}, _road_depot_NW }
};

/* Sprite layout for level crossings. The SpriteIDs are actually offsets
 * from the base SpriteID returned from the NewGRF sprite resolver. */
static const DrawTileSeqStruct _crossing_layout_ALL[] = {
	TILE_SEQ_LINE(2, PAL_NONE,  0,  0, 3, 3)
	TILE_SEQ_LINE(4, PAL_NONE,  0, 13, 3, 3)
	TILE_SEQ_LINE(6, PAL_NONE, 13,  0, 3, 3)
	TILE_SEQ_LINE(8, PAL_NONE, 13, 13, 3, 3)
	TILE_SEQ_END()
};

static const DrawTileSprites _crossing_layout = {
	{0, PAL_NONE}, _crossing_layout_ALL
};

#undef TILE_SEQ_LINE
#undef TILE_SEQ_END


static const SpriteID _road_tile_sprites_1[16] = {
	    0, 0x546, 0x545, 0x53B, 0x544, 0x534, 0x53E, 0x539,
	0x543, 0x53C, 0x535, 0x538, 0x53D, 0x537, 0x53A, 0x536
};

static const SpriteID _road_frontwire_sprites_1[16] = {
	0, 0x54, 0x55, 0x5B, 0x54, 0x54, 0x5E, 0x5A, 0x55, 0x5C, 0x55, 0x58, 0x5D, 0x57, 0x59, 0x56
};

static const SpriteID _road_backpole_sprites_1[16] = {
	0, 0x38, 0x39, 0x40, 0x38, 0x38, 0x43, 0x3E, 0x39, 0x41, 0x39, 0x3C, 0x42, 0x3B, 0x3D, 0x3A
};

#define MAKELINE(a, b, c) { a, b, c },
#define ENDLINE { 0, 0, 0 }

static const DrawRoadTileStruct _roadside_nothing[] = {
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_3[] = {
	MAKELINE(0x57f,  1,  8)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_5[] = {
	MAKELINE(0x57f,  1,  8)
	MAKELINE(0x57e, 14,  8)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_6[] = {
	MAKELINE(0x57e,  8,  1)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_7[] = {
	MAKELINE(0x57f,  1,  8)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_9[] = {
	MAKELINE(0x57f,  8, 14)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_10[] = {
	MAKELINE(0x57f,  8, 14)
	MAKELINE(0x57e,  8,  1)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_11[] = {
	MAKELINE(0x57f,  8, 14)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_12[] = {
	MAKELINE(0x57e,  8,  1)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_13[] = {
	MAKELINE(0x57e, 14,  8)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_14[] = {
	MAKELINE(0x57e,  8,  1)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_19[] = {
	MAKELINE(0x1212,  0,  2)
	MAKELINE(0x1212,  3,  9)
	MAKELINE(0x1212, 10, 12)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_21[] = {
	MAKELINE(0x1212,  0,  2)
	MAKELINE(0x1212,  0, 10)
	MAKELINE(0x1212, 12,  2)
	MAKELINE(0x1212, 12, 10)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_22[] = {
	MAKELINE(0x1212, 10,  0)
	MAKELINE(0x1212,  3,  3)
	MAKELINE(0x1212,  0, 10)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_23[] = {
	MAKELINE(0x1212,  0,  2)
	MAKELINE(0x1212,  0, 10)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_25[] = {
	MAKELINE(0x1212, 12,  2)
	MAKELINE(0x1212,  9,  9)
	MAKELINE(0x1212,  2, 12)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_26[] = {
	MAKELINE(0x1212,  2,  0)
	MAKELINE(0x1212, 10,  0)
	MAKELINE(0x1212,  2, 12)
	MAKELINE(0x1212, 10, 12)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_27[] = {
	MAKELINE(0x1212,  2, 12)
	MAKELINE(0x1212, 10, 12)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_28[] = {
	MAKELINE(0x1212,  2,  0)
	MAKELINE(0x1212,  9,  3)
	MAKELINE(0x1212, 12, 10)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_29[] = {
	MAKELINE(0x1212, 12,  2)
	MAKELINE(0x1212, 12, 10)
	ENDLINE
};

static const DrawRoadTileStruct _road_display_datas2_30[] = {
	MAKELINE(0x1212,  2, 0)
	MAKELINE(0x1212, 10, 0)
	ENDLINE
};

#undef MAKELINE
#undef ENDLINE

static const DrawRoadTileStruct * const _roadside_none[] = {
	_roadside_nothing, _roadside_nothing,
	_roadside_nothing, _roadside_nothing,
	_roadside_nothing, _roadside_nothing,
	_roadside_nothing, _roadside_nothing,
	_roadside_nothing, _roadside_nothing,
	_roadside_nothing, _roadside_nothing,
	_roadside_nothing, _roadside_nothing,
	_roadside_nothing, _roadside_nothing
};

static const DrawRoadTileStruct * const _roadside_lamps[] = {
	_roadside_nothing,
	_roadside_nothing,
	_roadside_nothing,
	_road_display_datas2_3,
	_roadside_nothing,
	_road_display_datas2_5,
	_road_display_datas2_6,
	_road_display_datas2_7,
	_roadside_nothing,
	_road_display_datas2_9,
	_road_display_datas2_10,
	_road_display_datas2_11,
	_road_display_datas2_12,
	_road_display_datas2_13,
	_road_display_datas2_14,
	_roadside_nothing,
};

static const DrawRoadTileStruct * const _roadside_trees[] = {
	_roadside_nothing,
	_roadside_nothing,
	_roadside_nothing,
	_road_display_datas2_19,
	_roadside_nothing,
	_road_display_datas2_21,
	_road_display_datas2_22,
	_road_display_datas2_23,

	_roadside_nothing,
	_road_display_datas2_25,
	_road_display_datas2_26,
	_road_display_datas2_27,
	_road_display_datas2_28,
	_road_display_datas2_29,
	_road_display_datas2_30,
	_roadside_nothing,
};

static const DrawRoadTileStruct * const * const _road_display_table[] = {
	_roadside_none,
	_roadside_none,
	_roadside_none,
	_roadside_lamps,
	_roadside_none,
	_roadside_trees,
};
