/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_canal.cpp Implementation of NewGRF canals. */

#include "stdafx.h"
#include "debug.h"
#include "newgrf_spritegroup.h"
#include "newgrf_canal.h"
#include "water_map.h"


/** Table of canal 'feature' sprite groups */
WaterFeature _water_feature[CF_END];


/* Random bits and triggers are not supported for canals, so the following
 * three functions are stubs. */
static uint32 CanalGetRandomBits(const ResolverObject *object)
{
	/* Return random bits only for water tiles, not station tiles */
	return IsTileType(object->u.canal.tile, MP_WATER) ? GetWaterTileRandomBits(object->u.canal.tile) : 0;
}


static uint32 CanalGetTriggers(const ResolverObject *object)
{
	return 0;
}


static void CanalSetTriggers(const ResolverObject *object, int triggers)
{
	return;
}


static uint32 CanalGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	TileIndex tile = object->u.canal.tile;

	switch (variable) {
		/* Height of tile */
		case 0x80: {
			uint z = GetTileZ(tile) / TILE_HEIGHT;
			/* Return consistent height within locks */
			if (IsTileType(tile, MP_WATER) && IsLock(tile) && GetSection(tile) == LOCK_UPPER) z--;
			return z;
		}

		/* Terrain type */
		case 0x81: return GetTerrainType(tile);

		/* Random data for river or canal tiles, otherwise zero */
		case 0x83: return IsTileType(tile, MP_WATER) ? GetWaterTileRandomBits(tile) : 0;
	}

	DEBUG(grf, 1, "Unhandled canal variable 0x%02X", variable);

	*available = false;
	return UINT_MAX;
}


static const SpriteGroup *CanalResolveReal(const ResolverObject *object, const RealSpriteGroup *group)
{
	if (group->num_loaded == 0) return NULL;

	return group->loaded[0];
}


static void NewCanalResolver(ResolverObject *res, TileIndex tile, const GRFFile *grffile)
{
	res->GetRandomBits = &CanalGetRandomBits;
	res->GetTriggers   = &CanalGetTriggers;
	res->SetTriggers   = &CanalSetTriggers;
	res->GetVariable   = &CanalGetVariable;
	res->ResolveReal   = &CanalResolveReal;

	res->u.canal.tile = tile;

	res->callback        = CBID_NO_CALLBACK;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
	res->count           = 0;
	res->grffile         = grffile;
}


SpriteID GetCanalSprite(CanalFeature feature, TileIndex tile)
{
	ResolverObject object;
	const SpriteGroup *group;

	NewCanalResolver(&object, tile, _water_feature[feature].grffile);

	group = SpriteGroup::Resolve(_water_feature[feature].group, &object);
	if (group == NULL) return 0;

	return group->GetResult();
}

/**
 * Run a specific callback for canals.
 * @param callback Callback ID.
 * @param param1   Callback parameter 1.
 * @param param2   Callback parameter 2.
 * @param feature  For which feature to run the callback.
 * @param tile     Tile index of canal.
 * @return Callback result or CALLBACK_FAILED if the callback failed.
 */
static uint16 GetCanalCallback(CallbackID callback, uint32 param1, uint32 param2, CanalFeature feature, TileIndex tile)
{
	ResolverObject object;
	const SpriteGroup *group;

	NewCanalResolver(&object, tile, _water_feature[feature].grffile);

	object.callback = callback;
	object.callback_param1 = param1;
	object.callback_param2 = param2;

	group = SpriteGroup::Resolve(_water_feature[feature].group, &object);
	if (group == NULL) return CALLBACK_FAILED;

	return group->GetCallbackResult();
}

/**
 * Get the new sprite offset for a water tile.
 * @param tile       Tile index of the canal/water tile.
 * @param feature    For which feature to get the new sprite offset.
 * @param cur_offset Current sprite offset.
 * @return New sprite offset.
 */
uint GetCanalSpriteOffset(CanalFeature feature, TileIndex tile, uint cur_offset)
{
	if (HasBit(_water_feature[feature].callback_mask, CBM_CANAL_SPRITE_OFFSET)) {
		uint16 cb = GetCanalCallback(CBID_CANALS_SPRITE_OFFSET, cur_offset, 0, feature, tile);
		if (cb != CALLBACK_FAILED) return cur_offset + cb;
	}
	return cur_offset;
}
