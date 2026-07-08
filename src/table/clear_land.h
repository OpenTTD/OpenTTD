/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file clear_land.h Tables with sprites for clear land and fences. */

#include "sprites.h"

/** Sprites of the random variants of rough grass sprites, for flat tiles only. */
static const SpriteID _landscape_clear_sprites_rough[8] = {
	SPR_FLAT_ROUGH_LAND,
	SPR_FLAT_ROUGH_LAND_1,
	SPR_FLAT_ROUGH_LAND_2,
	SPR_FLAT_ROUGH_LAND_3,
	SPR_FLAT_ROUGH_LAND_4,
	SPR_FLAT_ROUGH_LAND,
	SPR_FLAT_ROUGH_LAND_1,
	SPR_FLAT_ROUGH_LAND_2,
};

/** Sprite offset for sloped fences on the southwest edge of the tile. */
static const uint8_t _fence_mod_by_tileh_sw[32] = {
	0, 2, 4, 0, 0, 2, 4, 0,
	0, 2, 4, 0, 0, 2, 4, 0,
	0, 2, 4, 0, 0, 2, 4, 4,
	0, 2, 4, 2, 0, 2, 4, 0,
};

/** Sprite offset for sloped fences on the southeast edge of the tile. */
static const uint8_t _fence_mod_by_tileh_se[32] = {
	1, 1, 5, 5, 3, 3, 1, 1,
	1, 1, 5, 5, 3, 3, 1, 1,
	1, 1, 5, 5, 3, 3, 1, 5,
	1, 1, 5, 5, 3, 3, 3, 1,
};

/** Sprite offset for sloped fences on the northeast edge of the tile. */
static const uint8_t _fence_mod_by_tileh_ne[32] = {
	0, 0, 0, 0, 4, 4, 4, 4,
	2, 2, 2, 2, 0, 0, 0, 0,
	0, 0, 0, 0, 4, 4, 4, 4,
	2, 2, 2, 2, 0, 2, 4, 0,
};

/** Sprite offset for sloped fences on the northwest edge of the tile. */
static const uint8_t _fence_mod_by_tileh_nw[32] = {
	1, 5, 1, 5, 1, 5, 1, 5,
	3, 1, 3, 1, 3, 1, 3, 1,
	1, 5, 1, 5, 1, 5, 1, 5,
	3, 1, 3, 5, 3, 3, 3, 1,
};

/** Sprites of the flat tile base for each type of farm field fence. */
static const SpriteID _clear_land_fence_sprites[7] = {
	SPR_HEDGE_BUSHES,
	SPR_HEDGE_BUSHES_WITH_GATE,
	SPR_HEDGE_FENCE,
	SPR_HEDGE_BLOOMBUSH_YELLOW,
	SPR_HEDGE_BLOOMBUSH_RED,
	SPR_HEDGE_STONE,
};

/** Sprites of the flat tile base for each state of farm fields. */
static const SpriteID _clear_land_sprites_farmland[16] = {
	SPR_FARMLAND_BARE,
	SPR_FARMLAND_STATE_1,
	SPR_FARMLAND_STATE_2,
	SPR_FARMLAND_STATE_3,
	SPR_FARMLAND_STATE_4,
	SPR_FARMLAND_STATE_5,
	SPR_FARMLAND_STATE_6,
	SPR_FARMLAND_STATE_7,
	SPR_FARMLAND_HAYPACKS,
};

/** Sprites of the flat tile base for each density of clear -> grass transition. */
static const SpriteID _clear_land_sprites_grass[4] = {
	SPR_FLAT_BARE_LAND,
	SPR_FLAT_1_THIRD_GRASS_TILE,
	SPR_FLAT_2_THIRD_GRASS_TILE,
	SPR_FLAT_GRASS_TILE,
};

/** Sprites of the flat tile base for each density of grass -> snow/desert transition. */
static const SpriteID _clear_land_sprites_snow_desert[4] = {
	SPR_FLAT_1_QUART_SNOW_DESERT_TILE,
	SPR_FLAT_2_QUART_SNOW_DESERT_TILE,
	SPR_FLAT_3_QUART_SNOW_DESERT_TILE,
	SPR_FLAT_SNOW_DESERT_TILE,
};
