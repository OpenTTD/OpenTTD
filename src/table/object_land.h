/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file object_land.h Sprites to use and how to display them for object tiles. */

#include "table/strings.h"

#define TILE_SEQ_LINE(sz, img) { 0, 0, 0, 16, 16, sz, {img, PAL_NONE} },

static const DrawTileSeqStruct _object_transmitter_seq[] = {
	{   7,  7,  0,  2,  2, 70, {SPR_TRANSMITTER, PAL_NONE}},
};

static const DrawTileSeqStruct _object_lighthouse_seq[] = {
	{   4,  4,  0,  7,  7, 61, {SPR_LIGHTHOUSE, PAL_NONE}},
};

static const DrawTileSeqStruct _object_statue_seq[] = {
	{   0,  0,  0, 16, 16, 25, {SPR_STATUE_COMPANY | (1 << PALETTE_MODIFIER_COLOUR), PAL_NONE}},
};

static const DrawTileSeqStruct _object_owned_land_seq[] = {
	{   8,  8,  0,  1,  1,  6, {SPR_BOUGHT_LAND    | (1 << PALETTE_MODIFIER_COLOUR), PAL_NONE}},
};

extern const DrawTileSpriteSpan _objects[] = {
	{ { SPR_FLAT_2_THIRD_GRASS_TILE, PAL_NONE }, _object_transmitter_seq },
	{ { SPR_FLAT_2_THIRD_GRASS_TILE, PAL_NONE }, _object_lighthouse_seq  },
	{ { SPR_CONCRETE_GROUND,         PAL_NONE }, _object_statue_seq      },
	{ { SPR_FLAT_BARE_LAND,          PAL_NONE }, _object_owned_land_seq  },
};


static const DrawTileSeqStruct _object_hq_medium_north[] = {
	TILE_SEQ_LINE(20, SPR_MEDIUMHQ_NORTH_WALL | (1 << PALETTE_MODIFIER_COLOUR))
};

static const DrawTileSeqStruct _object_hq_medium_east[] = {
	TILE_SEQ_LINE(20, SPR_MEDIUMHQ_EAST_WALL  | (1 << PALETTE_MODIFIER_COLOUR))
};

static const DrawTileSeqStruct _object_hq_medium_west[] = {
	TILE_SEQ_LINE(20, SPR_MEDIUMHQ_WEST_WALL  | (1 << PALETTE_MODIFIER_COLOUR))
};

static const DrawTileSeqStruct _object_hq_large_north[] = {
	TILE_SEQ_LINE(50, SPR_LARGEHQ_NORTH_BUILD | (1 << PALETTE_MODIFIER_COLOUR))
};

static const DrawTileSeqStruct _object_hq_large_east[] = {
	TILE_SEQ_LINE(50, SPR_LARGEHQ_EAST_BUILD  | (1 << PALETTE_MODIFIER_COLOUR))
};

static const DrawTileSeqStruct _object_hq_large_west[] = {
	TILE_SEQ_LINE(50, SPR_LARGEHQ_WEST_BUILD  | (1 << PALETTE_MODIFIER_COLOUR))
};

static const DrawTileSeqStruct _object_hq_huge_north[] = {
	TILE_SEQ_LINE(60, SPR_HUGEHQ_NORTH_BUILD  | (1 << PALETTE_MODIFIER_COLOUR))
};

static const DrawTileSeqStruct _object_hq_huge_east[] = {
	TILE_SEQ_LINE(60, SPR_HUGEHQ_EAST_BUILD   | (1 << PALETTE_MODIFIER_COLOUR))
};

static const DrawTileSeqStruct _object_hq_huge_west[] = {
	TILE_SEQ_LINE(60, SPR_HUGEHQ_WEST_BUILD   | (1 << PALETTE_MODIFIER_COLOUR))
};

#undef TILE_SEQ_LINE

#define TILE_SPRITE_LINE(img, dtss) { {img | (1 << PALETTE_MODIFIER_COLOUR), PAL_NONE}, dtss },
#define TILE_SPRITE_LINE_NOTHING(img) { {img | (1 << PALETTE_MODIFIER_COLOUR), PAL_NONE} },

static const DrawTileSpriteSpan _object_hq[] = {
	TILE_SPRITE_LINE_NOTHING(SPR_TINYHQ_NORTH)
	TILE_SPRITE_LINE_NOTHING(SPR_TINYHQ_WEST)
	TILE_SPRITE_LINE_NOTHING(SPR_TINYHQ_EAST)
	TILE_SPRITE_LINE_NOTHING(SPR_TINYHQ_SOUTH)

	TILE_SPRITE_LINE_NOTHING(SPR_SMALLHQ_NORTH)
	TILE_SPRITE_LINE_NOTHING(SPR_SMALLHQ_WEST)
	TILE_SPRITE_LINE_NOTHING(SPR_SMALLHQ_EAST)
	TILE_SPRITE_LINE_NOTHING(SPR_SMALLHQ_SOUTH)

	TILE_SPRITE_LINE(SPR_MEDIUMHQ_NORTH,       _object_hq_medium_north)
	TILE_SPRITE_LINE(SPR_MEDIUMHQ_WEST,        _object_hq_medium_west)
	TILE_SPRITE_LINE(SPR_MEDIUMHQ_EAST,        _object_hq_medium_east)
	TILE_SPRITE_LINE_NOTHING(SPR_MEDIUMHQ_SOUTH)

	TILE_SPRITE_LINE(SPR_LARGEHQ_NORTH_GROUND, _object_hq_large_north)
	TILE_SPRITE_LINE(SPR_LARGEHQ_WEST_GROUND,  _object_hq_large_west)
	TILE_SPRITE_LINE(SPR_LARGEHQ_EAST_GROUND,  _object_hq_large_east)
	TILE_SPRITE_LINE_NOTHING(SPR_LARGEHQ_SOUTH)

	TILE_SPRITE_LINE(SPR_HUGEHQ_NORTH_GROUND,  _object_hq_huge_north)
	TILE_SPRITE_LINE(SPR_HUGEHQ_WEST_GROUND,   _object_hq_huge_west)
	TILE_SPRITE_LINE(SPR_HUGEHQ_EAST_GROUND,   _object_hq_huge_east)
	TILE_SPRITE_LINE_NOTHING(SPR_HUGEHQ_SOUTH)
};

#undef TILE_SPRITE_LINE
#undef TILE_SPRITE_LINE_NOTHING

#define M(name, size, build_cost_multiplier, clear_cost_multiplier, height, climate, gen_amount, flags) {{INVALID_OBJECT_CLASS, 0}, StandardGRFFileProps{}, AnimationInfo<ObjectAnimationTriggers>{}, name, climate, size, build_cost_multiplier, clear_cost_multiplier, TimerGameCalendar::Date{}, CalendarTime::MAX_DATE + 1, flags, ObjectCallbackMasks{}, height, 1, gen_amount, {}}

/* Climates
 * T = Temperate
 * A = Sub-Arctic
 * S = Sub-Tropic
 * Y = Toyland */
#define T LandscapeType::Temperate
#define A LandscapeType::Arctic
#define S LandscapeType::Tropic
#define Y LandscapeType::Toyland
/** Specification of the original object structures. */
extern const ObjectSpec _original_objects[] = {
	M(STR_LAI_OBJECT_DESCRIPTION_TRANSMITTER,          0x11,   0,   0, 10, LandscapeTypes({T,A,S  }), 15, ObjectFlags({ObjectFlag::CannotRemove, ObjectFlag::OnlyInScenedit})),
	M(STR_LAI_OBJECT_DESCRIPTION_LIGHTHOUSE,           0x11,   0,   0,  8, LandscapeTypes({T,A    }),  8, ObjectFlags({ObjectFlag::CannotRemove, ObjectFlag::OnlyInScenedit, ObjectFlag::ScaleByWater})),
	M(STR_TOWN_BUILDING_NAME_STATUE_1,                 0x11,   0,   0,  5, LandscapeTypes({T,S,A,Y}),  0, ObjectFlags({ObjectFlag::CannotRemove, ObjectFlag::OnlyInGame, ObjectFlag::OnlyInScenedit})), // Yes, we disallow building this everywhere. Happens in "special" case!
	M(STR_LAI_OBJECT_DESCRIPTION_COMPANY_OWNED_LAND,   0x11,  10,  10,  0, LandscapeTypes({T,S,A,Y}),  0, ObjectFlags({ObjectFlag::Autoremove, ObjectFlag::OnlyInGame, ObjectFlag::ClearIncome, ObjectFlag::HasNoFoundation})), // Only non-silly use case is to use it when you cannot build a station, so disallow bridges
	M(STR_LAI_OBJECT_DESCRIPTION_COMPANY_HEADQUARTERS, 0x22,   0,   0,  7, LandscapeTypes({T,S,A,Y}),  0, ObjectFlags({ObjectFlag::CannotRemove, ObjectFlag::OnlyInGame})),
};

#undef M
#undef Y
#undef S
#undef A
#undef T
