/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_tree.cpp NewGRF handling of trees. */

#include "stdafx.h"
#include "debug.h"
#include "newgrf_animation_base.h"
#include "newgrf_tree.h"
#include "newgrf_spritegroup.h"
#include "town.h"
#include "tree_func.h"
#include "tree_type.h"
#include "tree_map.h"

#include "safeguards.h"

/**
 * Get nearby tile information for trees.
 * @param parameter The NewGRF "encoded" offset.
 * @param tile Tile to have the offset from.
 * @param treetype The treetype of the tree being queried.
 * @param grf_version8 True, if we are dealing with a new NewGRF which uses GRF version >= 8.
 * @return a construction of bits obeying the newgrf format
 */
static uint32_t GetNearbyTreeTileInformation(uint8_t parameter, TileIndex tile, TreeType treetype, bool grf_version8)
{
	if (parameter != 0) tile = GetNearbyTile(parameter, tile);
	bool is_same_tree = (IsTileType(tile, TileType::Trees) && GetTreeType(tile) == treetype);

	return GetNearbyTileInformation(tile, grf_version8) | (is_same_tree ? 1 : 0) << 8;
}

/** Resolver for the tree scope. */
struct TreeScopeResolver : public ScopeResolver {
	TileIndex tile; ///< Tile being resolved.
	uint8_t slot; ///< Tree slot of tile.

	/**
	 * Scope resolver of a tree tile.
	 * @param ro Surrounding resolver.
	 * @param tile \c Tile of the tree.
	 * @param slot Tree slot of tile being resolved.
	 */
	TreeScopeResolver(ResolverObject &ro, TileIndex tile, uint8_t slot)
		: ScopeResolver(ro), tile(tile), slot(slot)
	{
	}

	uint32_t GetRandomBits() const override
	{
		uint tmp = CountBits(this->tile.base() + (TileX(this->tile) + TileY(this->tile)) * TILE_SIZE);
		return GB(tmp, 0, 2);
	}

	uint32_t GetRandomTriggers() const override
	{
		return 0;
	}

	uint32_t GetVariable(uint8_t variable, [[maybe_unused]] uint32_t parameter, bool &available) const override
	{
		if (this->tile == INVALID_TILE) {
			switch (variable) {
				case 0x40: return 0;
				case 0x41: return 1 << 8;
				case 0x42: return to_underlying(HouseZone::TownEdge);
				case 0x44: return TimerGameCalendar::month;
				case 0x45: return 0;
				case 0x60: return 0;
			}

			available = false;
			return UINT_MAX;
		}

		switch (variable) {
			/* Terrain type */
			case 0x40: return GetTerrainType(this->tile);

			/* Current tree number and tree count */
			case 0x41: return (GetTreeCount(this->tile) << 8) | this->slot;

			/* Layout (bits 0..1) and position (bits 2..3) variant. */
			case 0x42: return GB(CountBits(this->tile.base() + (TileX(this->tile) + TileY(this->tile)) * TILE_SIZE), 0, 4);

			/* Current town zone of the tile in the nearest town */
			case 0x43: return to_underlying(GetTownRadiusGroup(ClosestTownFromTile(this->tile, UINT_MAX), this->tile));

			/* Tree month cycle */
			case 0x44: return GetTreeMonth(this->tile);

			/* Land info of nearby tiles */
			case 0x60: return GetNearbyTreeTileInformation(parameter, this->tile, GetTreeType(this->tile), this->ro.grffile->grf_version >= 8);
		}

		Debug(grf, 1, "Unhandled tree variable 0x{:X}", variable);

		available = false;
		return UINT_MAX;
	}
};

/** Resolver for tree tiles. */
struct TreeResolverObject : public ResolverObject {
	TreeScopeResolver tree_scope; ///< Scope resolver for the tree.
	uint16_t tree; ///< The tree.

	/**
	 * Scope resolver of a tree.
	 * @param tree The tree.
	 * @param slot Tree slot of tile.
	 * @param tile The tree tile.
	 * @param callback Callback ID.
	 * @param callback_param1 First parameter (var 10) of the callback.
	 * @param callback_param2 Second parameter (var 18) of the callback.
	 */
	TreeResolverObject(uint16_t tree, uint8_t slot, TileIndex tile, CallbackID callback = CBID_NO_CALLBACK, uint32_t callback_param1 = 0, uint32_t callback_param2 = 0) :
			ResolverObject(GetTreeSpec(tree).grf_prop.grffile, callback, callback_param1, callback_param2),
			tree_scope(*this, tile, slot), tree(tree)
	{
		this->root_spritegroup = GetTreeSpec(tree).grf_prop.GetSpriteGroup(StandardSpriteGroup::Default);
	}

	ScopeResolver *GetScope(VarSpriteGroupScope scope = VarSpriteGroupScope::Self, uint8_t relative = 0) override
	{
		switch (scope) {
			case VarSpriteGroupScope::Self: return &tree_scope;
			default: return ResolverObject::GetScope(scope, relative);
		}
	}

	GrfSpecFeature GetFeature() const override
	{
		return GrfSpecFeature::Trees;
	}

	uint32_t GetDebugID() const override
	{
		return GetTreeSpec(tree).grf_prop.local_id;
	}
};

/**
 * Evaluate a NewGRF callback for trees.
 * @param callback The callback to evaluate.
 * @param param1 First parameter of the callback.
 * @param param2 Second parameter of the callback.
 * @param tree The tree.
 * @param tile The tile.
 * @param[out] regs100 Additional result values from registers 100+
 * @return The value the callback returned, or CALLBACK_FAILED if it failed
 */
uint16_t GetTreeTileCallback(CallbackID callback, uint32_t param1, uint32_t param2, uint16_t tree, TileIndex tile, std::span<int32_t> regs100)
{
	TreeResolverObject object(tree, 0, tile, callback, param1, param2);
	return object.ResolveCallback(regs100);
}

/**
 * Get a sprite representing a trees.
 * @param tile The tile.
 * @param tree The tree.
 * @param slot Tree slot of tile.
 * @return Sprite to draw.
 */
SpriteID GetCustomTreeSprite(TileIndex tile, uint16_t tree, uint8_t slot)
{
	TreeResolverObject object(tree, slot, tile);
	const auto *group = object.Resolve<ResultSpriteGroup>();
	if (group == nullptr || group->num_sprites == 0) return {};

	return group->sprite;
}
