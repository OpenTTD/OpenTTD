/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file unmovable_land.h Sprites to use and how to display them for unmovable tiles. */

#define TILE_SEQ_LINE(sz, img) { 0, 0, 0, 16, 16, sz, {img, PAL_NONE} },
#define TILE_SEQ_END() { (int8)0x80, 0, 0, 0, 0, 0, {0, 0} }

static const DrawTileSeqStruct _unmovable_nothing[] = {
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_transmitter_seq[] = {
	{   7,  7,  0,  2,  2, 70, {SPR_UNMOVABLE_TRANSMITTER, PAL_NONE}},
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_lighthouse_seq[] = {
	{   4,  4,  0,  7,  7, 61, {SPR_UNMOVABLE_LIGHTHOUSE, PAL_NONE}},
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_statue_seq[] = {
	{   0,  0,  0, 16, 16, 25, {SPR_STATUE_COMPANY | (1 << PALETTE_MODIFIER_COLOUR), PAL_NONE}},
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_owned_land_seq[] = {
	{   8,  8,  0,  1,  1,  6, {SPR_BOUGHT_LAND    | (1 << PALETTE_MODIFIER_COLOUR), PAL_NONE}},
	TILE_SEQ_END()
};

static const DrawTileSprites _unmovables[] = {
	{ { SPR_FLAT_2_THIRD_GRASS_TILE, PAL_NONE }, _unmovable_transmitter_seq },
	{ { SPR_FLAT_2_THIRD_GRASS_TILE, PAL_NONE }, _unmovable_lighthouse_seq  },
	{ { SPR_CONCRETE_GROUND,         PAL_NONE }, _unmovable_statue_seq      },
	{ { SPR_FLAT_BARE_LAND,          PAL_NONE }, _unmovable_owned_land_seq  },
};


static const DrawTileSeqStruct _unmovable_hq_medium_north[] = {
	TILE_SEQ_LINE(20, SPR_MEDIUMHQ_NORTH_WALL | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_hq_medium_east[] = {
	TILE_SEQ_LINE(20, SPR_MEDIUMHQ_EAST_WALL  | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_hq_medium_west[] = {
	TILE_SEQ_LINE(20, SPR_MEDIUMHQ_WEST_WALL  | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_hq_large_north[] = {
	TILE_SEQ_LINE(50, SPR_LARGEHQ_NORTH_BUILD | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_hq_large_east[] = {
	TILE_SEQ_LINE(50, SPR_LARGEHQ_EAST_BUILD  | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_hq_large_west[] = {
	TILE_SEQ_LINE(50, SPR_LARGEHQ_WEST_BUILD  | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_hq_huge_north[] = {
	TILE_SEQ_LINE(60, SPR_HUGEHQ_NORTH_BUILD  | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_hq_huge_east[] = {
	TILE_SEQ_LINE(60, SPR_HUGEHQ_EAST_BUILD   | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _unmovable_hq_huge_west[] = {
	TILE_SEQ_LINE(60, SPR_HUGEHQ_WEST_BUILD   | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

#undef TILE_SEQ_LINE
#undef TILE_SEQ_END

#define TILE_SPRITE_LINE(img, dtss) { {img | (1 << PALETTE_MODIFIER_COLOUR), PAL_NONE}, dtss },

static const DrawTileSprites _unmovable_hq[] = {
	TILE_SPRITE_LINE(SPR_TINYHQ_NORTH,         _unmovable_nothing)
	TILE_SPRITE_LINE(SPR_TINYHQ_WEST,          _unmovable_nothing)
	TILE_SPRITE_LINE(SPR_TINYHQ_EAST,          _unmovable_nothing)
	TILE_SPRITE_LINE(SPR_TINYHQ_SOUTH,         _unmovable_nothing)

	TILE_SPRITE_LINE(SPR_SMALLHQ_NORTH,        _unmovable_nothing)
	TILE_SPRITE_LINE(SPR_SMALLHQ_WEST,         _unmovable_nothing)
	TILE_SPRITE_LINE(SPR_SMALLHQ_EAST,         _unmovable_nothing)
	TILE_SPRITE_LINE(SPR_SMALLHQ_SOUTH,        _unmovable_nothing)

	TILE_SPRITE_LINE(SPR_MEDIUMHQ_NORTH,       _unmovable_hq_medium_north)
	TILE_SPRITE_LINE(SPR_MEDIUMHQ_WEST,        _unmovable_hq_medium_west)
	TILE_SPRITE_LINE(SPR_MEDIUMHQ_EAST,        _unmovable_hq_medium_east)
	TILE_SPRITE_LINE(SPR_MEDIUMHQ_SOUTH,       _unmovable_nothing)

	TILE_SPRITE_LINE(SPR_LARGEHQ_NORTH_GROUND, _unmovable_hq_large_north)
	TILE_SPRITE_LINE(SPR_LARGEHQ_WEST_GROUND,  _unmovable_hq_large_west)
	TILE_SPRITE_LINE(SPR_LARGEHQ_EAST_GROUND,  _unmovable_hq_large_east)
	TILE_SPRITE_LINE(SPR_LARGEHQ_SOUTH,        _unmovable_nothing)

	TILE_SPRITE_LINE(SPR_HUGEHQ_NORTH_GROUND,  _unmovable_hq_huge_north)
	TILE_SPRITE_LINE(SPR_HUGEHQ_WEST_GROUND,   _unmovable_hq_huge_west)
	TILE_SPRITE_LINE(SPR_HUGEHQ_EAST_GROUND,   _unmovable_hq_huge_east)
	TILE_SPRITE_LINE(SPR_HUGEHQ_SOUTH,         _unmovable_nothing)
};

#undef TILE_SPRITE_LINE

/** Specification of the original unmovable structures. */
static const UnmovableSpec _original_unmovable[] = {
	{ STR_LAI_UNMOVABLE_DESCRIPTION_TRANSMITTER,          0x11,   0,   0, OBJECT_FLAG_CANNOT_REMOVE | OBJECT_FLAG_REQUIRE_FLAT | OBJECT_FLAG_ONLY_IN_SCENEDIT },
	{ STR_LAI_UNMOVABLE_DESCRIPTION_LIGHTHOUSE,           0x11,   0,   0, OBJECT_FLAG_CANNOT_REMOVE | OBJECT_FLAG_REQUIRE_FLAT | OBJECT_FLAG_ONLY_IN_SCENEDIT },
	{ STR_TOWN_BUILDING_NAME_STATUE_1,                    0x11,   0,   0, OBJECT_FLAG_CANNOT_REMOVE | OBJECT_FLAG_ONLY_IN_GAME | OBJECT_FLAG_ONLY_IN_SCENEDIT }, // Yes, we disallow building this everywhere. Happens in "special" case!
	{ STR_LAI_UNMOVABLE_DESCRIPTION_COMPANY_OWNED_LAND,   0x11,  10,   2, OBJECT_FLAG_AUTOREMOVE | OBJECT_FLAG_ONLY_IN_GAME | OBJECT_FLAG_CLEAR_INCOME | OBJECT_FLAG_HAS_NO_FOUNDATION | OBJECT_FLAG_ALLOW_UNDER_BRIDGE },
	{ STR_LAI_UNMOVABLE_DESCRIPTION_COMPANY_HEADQUARTERS, 0x22,   0,   0, OBJECT_FLAG_CANNOT_REMOVE | OBJECT_FLAG_ONLY_IN_GAME },
};
