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

struct CanalScopeResolver : public ScopeResolver {
	TileIndex tile;

	CanalScopeResolver(ResolverObject *ro, TileIndex tile);

	/* virtual */ uint32 GetRandomBits() const;
	/* virtual */ uint32 GetVariable(byte variable, uint32 parameter, bool *available) const;
};

/** Resolver object for canals. */
struct CanalResolverObject : public ResolverObject {
	CanalScopeResolver canal_scope;

	CanalResolverObject(const GRFFile *grffile, TileIndex tile,
			CallbackID callback = CBID_NO_CALLBACK, uint32 callback_param1 = 0, uint32 callback_param2 = 0);

	/* virtual */ ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0)
	{
		switch (scope) {
			case VSG_SCOPE_SELF: return &this->canal_scope;
			default: return &this->default_scope; // XXX ResolverObject::GetScope(scope, relative);
		}
	}

	/* virtual */ const SpriteGroup *ResolveReal(const RealSpriteGroup *group) const;
};

/* virtual */ uint32 CanalScopeResolver::GetRandomBits() const
{
	/* Return random bits only for water tiles, not station tiles */
	return IsTileType(this->tile, MP_WATER) ? GetWaterTileRandomBits(this->tile) : 0;
}

/* virtual */ uint32 CanalScopeResolver::GetVariable(byte variable, uint32 parameter, bool *available) const
{
	switch (variable) {
		/* Height of tile */
		case 0x80: {
			int z = GetTileZ(this->tile);
			/* Return consistent height within locks */
			if (IsTileType(this->tile, MP_WATER) && IsLock(this->tile) && GetLockPart(this->tile) == LOCK_PART_UPPER) z--;
			return z;
		}

		/* Terrain type */
		case 0x81: return GetTerrainType(this->tile);

		/* Random data for river or canal tiles, otherwise zero */
		case 0x83: return IsTileType(this->tile, MP_WATER) ? GetWaterTileRandomBits(this->tile) : 0;
	}

	DEBUG(grf, 1, "Unhandled canal variable 0x%02X", variable);

	*available = false;
	return UINT_MAX;
}


/* virtual */ const SpriteGroup *CanalResolverObject::ResolveReal(const RealSpriteGroup *group) const
{
	if (group->num_loaded == 0) return NULL;

	return group->loaded[0];
}

CanalScopeResolver::CanalScopeResolver(ResolverObject *ro, TileIndex tile) : ScopeResolver(ro)
{
	this->tile = tile;
}

/**
 * @param tile Tile index of canal.
 * @param grffile Grf file.
 */
CanalResolverObject::CanalResolverObject(const GRFFile *grffile, TileIndex tile,
		CallbackID callback, uint32 callback_param1, uint32 callback_param2)
		: ResolverObject(grffile, callback, callback_param1, callback_param2), canal_scope(this, tile)
{
}

/**
 * Lookup the base sprite to use for a canal.
 * @param feature Which canal feature we want.
 * @param tile Tile index of canal, if appropriate.
 * @return Base sprite returned by GRF, or \c 0 if none.
 */
SpriteID GetCanalSprite(CanalFeature feature, TileIndex tile)
{
	CanalResolverObject object(_water_feature[feature].grffile, tile);
	const SpriteGroup *group = SpriteGroup::Resolve(_water_feature[feature].group, &object);
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
 * @return Callback result or #CALLBACK_FAILED if the callback failed.
 */
static uint16 GetCanalCallback(CallbackID callback, uint32 param1, uint32 param2, CanalFeature feature, TileIndex tile)
{
	CanalResolverObject object(_water_feature[feature].grffile, tile, callback, param1, param2);
	const SpriteGroup *group = SpriteGroup::Resolve(_water_feature[feature].group, &object);
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
