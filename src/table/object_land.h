/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file object_land.h Sprites to use and how to display them for object tiles. */

#define TILE_SEQ_LINE(sz, img) { 0, 0, 0, 16, 16, sz, {img, PAL_NONE} },
#define TILE_SEQ_END() { (int8_t)0x80, 0, 0, 0, 0, 0, {0, 0} }

static const DrawTileSeqStruct _object_nothing[] = {
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _object_transmitter_seq[] = {
	{   7,  7,  0,  2,  2, 70, {SPR_TRANSMITTER, PAL_NONE}},
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _object_lighthouse_seq[] = {
	{   4,  4,  0,  7,  7, 61, {SPR_LIGHTHOUSE, PAL_NONE}},
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _object_statue_seq[] = {
	{   0,  0,  0, 16, 16, 25, {SPR_STATUE_COMPANY | (1 << PALETTE_MODIFIER_COLOUR), PAL_NONE}},
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _object_owned_land_seq[] = {
	{   8,  8,  0,  1,  1,  6, {SPR_BOUGHT_LAND    | (1 << PALETTE_MODIFIER_COLOUR), PAL_NONE}},
	TILE_SEQ_END()
};

extern const DrawTileSprites _objects[] = {
	{ { SPR_FLAT_2_THIRD_GRASS_TILE, PAL_NONE }, _object_transmitter_seq },
	{ { SPR_FLAT_2_THIRD_GRASS_TILE, PAL_NONE }, _object_lighthouse_seq  },
	{ { SPR_CONCRETE_GROUND,         PAL_NONE }, _object_statue_seq      },
	{ { SPR_FLAT_BARE_LAND,          PAL_NONE }, _object_owned_land_seq  },
};


static const DrawTileSeqStruct _object_hq_medium_north[] = {
	TILE_SEQ_LINE(20, SPR_MEDIUMHQ_NORTH_WALL | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _object_hq_medium_east[] = {
	TILE_SEQ_LINE(20, SPR_MEDIUMHQ_EAST_WALL  | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _object_hq_medium_west[] = {
	TILE_SEQ_LINE(20, SPR_MEDIUMHQ_WEST_WALL  | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _object_hq_large_north[] = {
	TILE_SEQ_LINE(50, SPR_LARGEHQ_NORTH_BUILD | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _object_hq_large_east[] = {
	TILE_SEQ_LINE(50, SPR_LARGEHQ_EAST_BUILD  | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _object_hq_large_west[] = {
	TILE_SEQ_LINE(50, SPR_LARGEHQ_WEST_BUILD  | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _object_hq_huge_north[] = {
	TILE_SEQ_LINE(60, SPR_HUGEHQ_NORTH_BUILD  | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _object_hq_huge_east[] = {
	TILE_SEQ_LINE(60, SPR_HUGEHQ_EAST_BUILD   | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _object_hq_huge_west[] = {
	TILE_SEQ_LINE(60, SPR_HUGEHQ_WEST_BUILD   | (1 << PALETTE_MODIFIER_COLOUR))
	TILE_SEQ_END()
};

#undef TILE_SEQ_LINE
#undef TILE_SEQ_END

#define TILE_SPRITE_LINE(img, dtss) { {img | (1 << PALETTE_MODIFIER_COLOUR), PAL_NONE}, dtss },

static const DrawTileSprites _object_hq[] = {
	TILE_SPRITE_LINE(SPR_TINYHQ_NORTH,         _object_nothing)
	TILE_SPRITE_LINE(SPR_TINYHQ_WEST,          _object_nothing)
	TILE_SPRITE_LINE(SPR_TINYHQ_EAST,          _object_nothing)
	TILE_SPRITE_LINE(SPR_TINYHQ_SOUTH,         _object_nothing)

	TILE_SPRITE_LINE(SPR_SMALLHQ_NORTH,        _object_nothing)
	TILE_SPRITE_LINE(SPR_SMALLHQ_WEST,         _object_nothing)
	TILE_SPRITE_LINE(SPR_SMALLHQ_EAST,         _object_nothing)
	TILE_SPRITE_LINE(SPR_SMALLHQ_SOUTH,        _object_nothing)

	TILE_SPRITE_LINE(SPR_MEDIUMHQ_NORTH,       _object_hq_medium_north)
	TILE_SPRITE_LINE(SPR_MEDIUMHQ_WEST,        _object_hq_medium_west)
	TILE_SPRITE_LINE(SPR_MEDIUMHQ_EAST,        _object_hq_medium_east)
	TILE_SPRITE_LINE(SPR_MEDIUMHQ_SOUTH,       _object_nothing)

	TILE_SPRITE_LINE(SPR_LARGEHQ_NORTH_GROUND, _object_hq_large_north)
	TILE_SPRITE_LINE(SPR_LARGEHQ_WEST_GROUND,  _object_hq_large_west)
	TILE_SPRITE_LINE(SPR_LARGEHQ_EAST_GROUND,  _object_hq_large_east)
	TILE_SPRITE_LINE(SPR_LARGEHQ_SOUTH,        _object_nothing)

	TILE_SPRITE_LINE(SPR_HUGEHQ_NORTH_GROUND,  _object_hq_huge_north)
	TILE_SPRITE_LINE(SPR_HUGEHQ_WEST_GROUND,   _object_hq_huge_west)
	TILE_SPRITE_LINE(SPR_HUGEHQ_EAST_GROUND,   _object_hq_huge_east)
	TILE_SPRITE_LINE(SPR_HUGEHQ_SOUTH,         _object_nothing)
};

#undef TILE_SPRITE_LINE

#define M(name, size, build_cost_multiplier, clear_cost_multiplier, height, climate, gen_amount, flags) { GRFFilePropsBase<2>(), {0, 0, 0, 0}, INVALID_OBJECT_CLASS, name, climate, size, build_cost_multiplier, clear_cost_multiplier, 0, CalendarTime::MAX_DATE + 1, flags, 0, height, 1, gen_amount }

/* Climates
 * T = Temperate
 * A = Sub-Arctic
 * S = Sub-Tropic
 * Y = Toyland */
#define T 1
#define A 2
#define S 4
#define Y 8
/** Specification of the original object structures. */
extern const ObjectSpec _original_objects[] = {
	M(STR_LAI_OBJECT_DESCRIPTION_TRANSMITTER,          0x11,   0,   0, 10, T|A|S  , 15, OBJECT_FLAG_CANNOT_REMOVE | OBJECT_FLAG_ONLY_IN_SCENEDIT),
	M(STR_LAI_OBJECT_DESCRIPTION_LIGHTHOUSE,           0x11,   0,   0,  8, T|A    ,  8, OBJECT_FLAG_CANNOT_REMOVE | OBJECT_FLAG_ONLY_IN_SCENEDIT | OBJECT_FLAG_SCALE_BY_WATER),
	M(STR_TOWN_BUILDING_NAME_STATUE_1,                 0x11,   0,   0,  5, T|S|A|Y,  0, OBJECT_FLAG_CANNOT_REMOVE | OBJECT_FLAG_ONLY_IN_GAME | OBJECT_FLAG_ONLY_IN_SCENEDIT), // Yes, we disallow building this everywhere. Happens in "special" case!
	M(STR_LAI_OBJECT_DESCRIPTION_COMPANY_OWNED_LAND,   0x11,  10,  10,  0, T|S|A|Y,  0, OBJECT_FLAG_AUTOREMOVE | OBJECT_FLAG_ONLY_IN_GAME | OBJECT_FLAG_CLEAR_INCOME | OBJECT_FLAG_HAS_NO_FOUNDATION ), // Only non-silly use case is to use it when you cannot build a station, so disallow bridges
	M(STR_LAI_OBJECT_DESCRIPTION_COMPANY_HEADQUARTERS, 0x22,   0,   0,  7, T|S|A|Y,  0, OBJECT_FLAG_CANNOT_REMOVE | OBJECT_FLAG_ONLY_IN_GAME),
};

#undef M
#undef Y
#undef S
#undef A
#undef T
