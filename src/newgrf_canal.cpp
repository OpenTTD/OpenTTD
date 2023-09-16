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
#include "water.h"
#include "water_map.h"
#include "spritecache.h"

#include "safeguards.h"

/** Table of canal 'feature' sprite groups */
WaterFeature _water_feature[CF_END];

/** Scope resolver of a canal tile. */
struct CanalScopeResolver : public ScopeResolver {
	TileIndex tile; ///< Tile containing the canal.

	CanalScopeResolver(ResolverObject &ro, TileIndex tile)
		: ScopeResolver(ro), tile(tile)
	{
	}

	uint32_t GetRandomBits() const override;
	uint32_t GetVariable(byte variable, [[maybe_unused]] uint32_t parameter, bool *available) const override;
};

/** Resolver object for canals. */
struct CanalResolverObject : public ResolverObject {
	CanalScopeResolver canal_scope;
	CanalFeature feature;

	CanalResolverObject(CanalFeature feature, TileIndex tile,
			CallbackID callback = CBID_NO_CALLBACK, uint32_t callback_param1 = 0, uint32_t callback_param2 = 0);

	ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0) override
	{
		switch (scope) {
			case VSG_SCOPE_SELF: return &this->canal_scope;
			default: return ResolverObject::GetScope(scope, relative);
		}
	}

	GrfSpecFeature GetFeature() const override;
	uint32_t GetDebugID() const override;
};

/* virtual */ uint32_t CanalScopeResolver::GetRandomBits() const
{
	/* Return random bits only for water tiles, not station tiles */
	return IsTileType(this->tile, MP_WATER) ? GetWaterTileRandomBits(this->tile) : 0;
}

/* virtual */ uint32_t CanalScopeResolver::GetVariable(byte variable, [[maybe_unused]] uint32_t parameter, bool *available) const
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

		/* Dike map: Connectivity info for river and canal tiles
		 *
		 * Assignment of bits to directions defined in agreement with
		 * http://projects.tt-forums.net/projects/ttdpatch/repository/revisions/2367/entry/trunk/patches/water.asm#L879
		 *         7
		 *      3     0
		 *   6     *     4
		 *      2     1
		 *         5
		 */
		case 0x82: {
			uint32_t connectivity =
				  (!IsWateredTile(TILE_ADDXY(tile, -1,  0), DIR_SW) << 0)  // NE
				+ (!IsWateredTile(TILE_ADDXY(tile,  0,  1), DIR_NW) << 1)  // SE
				+ (!IsWateredTile(TILE_ADDXY(tile,  1,  0), DIR_NE) << 2)  // SW
				+ (!IsWateredTile(TILE_ADDXY(tile,  0, -1), DIR_SE) << 3)  // NW
				+ (!IsWateredTile(TILE_ADDXY(tile, -1,  1), DIR_W)  << 4)  // E
				+ (!IsWateredTile(TILE_ADDXY(tile,  1,  1), DIR_N)  << 5)  // S
				+ (!IsWateredTile(TILE_ADDXY(tile,  1, -1), DIR_E)  << 6)  // W
				+ (!IsWateredTile(TILE_ADDXY(tile, -1, -1), DIR_S)  << 7); // N
			return connectivity;
		}

		/* Random data for river or canal tiles, otherwise zero */
		case 0x83: return IsTileType(this->tile, MP_WATER) ? GetWaterTileRandomBits(this->tile) : 0;
	}

	Debug(grf, 1, "Unhandled canal variable 0x{:02X}", variable);

	*available = false;
	return UINT_MAX;
}

GrfSpecFeature CanalResolverObject::GetFeature() const
{
	return GSF_CANALS;
}

uint32_t CanalResolverObject::GetDebugID() const
{
	return this->feature;
}

/**
 * Canal resolver constructor.
 * @param feature Which canal feature we want.
 * @param tile Tile index of canal.
 * @param callback Callback ID.
 * @param callback_param1 First parameter (var 10) of the callback.
 * @param callback_param2 Second parameter (var 18) of the callback.
 */
CanalResolverObject::CanalResolverObject(CanalFeature feature, TileIndex tile,
		CallbackID callback, uint32_t callback_param1, uint32_t callback_param2)
		: ResolverObject(_water_feature[feature].grffile, callback, callback_param1, callback_param2), canal_scope(*this, tile), feature(feature)
{
	this->root_spritegroup = _water_feature[feature].group;
}

/**
 * Lookup the base sprite to use for a canal.
 * @param feature Which canal feature we want.
 * @param tile Tile index of canal, if appropriate.
 * @return Base sprite returned by GRF, or \c 0 if none.
 */
SpriteID GetCanalSprite(CanalFeature feature, TileIndex tile)
{
	CanalResolverObject object(feature, tile);
	const SpriteGroup *group = object.Resolve();
	if (group == nullptr) return 0;

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
static uint16_t GetCanalCallback(CallbackID callback, uint32_t param1, uint32_t param2, CanalFeature feature, TileIndex tile)
{
	CanalResolverObject object(feature, tile, callback, param1, param2);
	return object.ResolveCallback();
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
		uint16_t cb = GetCanalCallback(CBID_CANALS_SPRITE_OFFSET, cur_offset, 0, feature, tile);
		if (cb != CALLBACK_FAILED) return cur_offset + cb;
	}
	return cur_offset;
}
