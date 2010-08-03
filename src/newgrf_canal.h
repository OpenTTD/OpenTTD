/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_canal.h Handling of NewGRF canals. */

#ifndef NEWGRF_CANAL_H
#define NEWGRF_CANAL_H

#include "gfx_type.h"
#include "tile_type.h"
#include "newgrf_generic.h"
#include "newgrf_callbacks.h"

/**
 * List of different canal 'features'.
 * Each feature gets an entry in the canal spritegroup table
 */
enum CanalFeature {
	CF_WATERSLOPE,
	CF_LOCKS,
	CF_DIKES,
	CF_ICON,
	CF_DOCKS,
	CF_RIVER_SLOPE,
	CF_RIVER_EDGE,
	CF_END,
};

/** Flags controlling the display of canals. */
enum CanalFeatureFlag {
	CFF_HAS_FLAT_SPRITE = 0, ///< Additional flat ground sprite in the beginning.
};

/** Information about a water feature. */
struct WaterFeature {
	const SpriteGroup *group; ///< Sprite group to start resolving.
	const GRFFile *grffile;   ///< NewGRF where 'group' belongs to.
	uint8 callback_mask;      ///< Bitmask of canal callbacks that have to be called.
	uint8 flags;              ///< Flags controlling display.
};


/** Table of canal 'feature' sprite groups */
extern WaterFeature _water_feature[CF_END];


/**
 * Lookup the base sprite to use for a canal.
 * @param feature Which canal feature we want.
 * @param tile Tile index of canal, if appropriate.
 * @return Base sprite returned by GRF, or 0 if none.
 */
SpriteID GetCanalSprite(CanalFeature feature, TileIndex tile);

uint16 GetCanalCallback(CallbackID callback, uint32 param1, uint32 param2, CanalFeature feature, TileIndex tile);
uint GetCanalSpriteOffset(CanalFeature feature, TileIndex tile, uint cur_offset);

#endif /* NEWGRF_CANAL_H */
