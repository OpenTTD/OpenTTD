/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_trees.cpp NewGRF handling of trees tiles. */

#include "stdafx.h"
#include "debug.h"
#include "newgrf_animation_base.h"
#include "newgrf_trees.h"
#include "newgrf_spritegroup.h"
#include "tile_cmd.h"
#include "town.h"
#include "tree_func.h"
#include "tree_type.h"
#include "tree_map.h"

#include "safeguards.h"

/** Resolver for the tree tiles scope. */
struct TreeTileScopeResolver : public ScopeResolver {
	TileIndex tile; ///< %Tile being resolved.

	/**
	 * Constructor of the scope resolver for the tree tile.
	 * @param ro Surrounding resolver.
	 * @param tile %Tile of the tree.
	 */
	TreeTileScopeResolver(ResolverObject &ro, TileIndex tile)
		: ScopeResolver(ro), tile(tile)
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
		switch (variable) {
			/* Terrain type */
			case 0x40: return GetTerrainType(this->tile);

			/* Tree count */
			case 0x41: return GetTreeCount(this->tile);

			/* Layout (0..1) and position (2..3) variant. */
			case 0x42: return GB(CountBits(this->tile.base() + (TileX(this->tile) + TileY(this->tile)) * TILE_SIZE), 0, 4);

			/* Current town zone of the tile in the nearest town */
			case 0x43: return to_underlying(GetTownRadiusGroup(ClosestTownFromTile(this->tile, UINT_MAX), this->tile));

			// /* Terrain height */
			// case 0x44: return TileHeight(this->tile);

			/* Land info of nearby tiles */
			case 0x60: return GetNearbyTileInformation(this->tile, this->ro.grffile->grf_version >= 8);
		}

		Debug(grf, 1, "Unhandled tree tile variable 0x{:X}", variable);

		available = false;
		return UINT_MAX;
	}
};

/** Resolver for tree tiles. */
struct TreeTileResolverObject : public ResolverObject {
	TreeTileScopeResolver tile_scope; ///< Scope resolver for the tree tile.
	TreeType treetype; ///< The tree tile type.

	TreeTileResolverObject(TreeType treetype, TileIndex tile, CallbackID callback = CBID_NO_CALLBACK, uint32_t callback_param1 = 0, uint32_t callback_param2 = 0) :
			ResolverObject(GetTreeSpec(treetype).grf_prop.grffile, callback, callback_param1, callback_param2),
			tile_scope(*this, tile), treetype(treetype)
	{
		this->root_spritegroup = GetTreeSpec(treetype).grf_prop.GetSpriteGroup(StandardSpriteGroup::Default);
	}

	ScopeResolver *GetScope(VarSpriteGroupScope scope = VarSpriteGroupScope::Self, uint8_t relative = 0) override
	{
		switch (scope) {
			case VarSpriteGroupScope::Self: return &tile_scope;
			default: return ResolverObject::GetScope(scope, relative);
		}
	}

	GrfSpecFeature GetFeature() const override
	{
		return GrfSpecFeature::Trees;
	}

	uint32_t GetDebugID() const override
	{
		return GetTreeSpec(treetype).grf_prop.local_id;
	}
};

uint16_t GetTreeTileCallback(CallbackID callback, uint32_t param1, uint32_t param2, TreeType treetype, TileIndex tile, std::span<int32_t> regs100)
{
	// assert(industry != nullptr && IsValidTile(tile));
	// assert(industry->index == IndustryID::Invalid() || IsTileType(tile, TileType::Industry));

	TreeTileResolverObject object(treetype, tile, callback, param1, param2);
	return object.ResolveCallback(regs100);
}

/**
 * Get a sprite representing a tree tile type.
 * @param tile
 * @param treetype
 * @return PalSpriteID
 */
PalSpriteID GetCustomTreeSprite(TileIndex tile, TreeType treetype)
{
	TreeTileResolverObject object(treetype, tile);
	const auto *group = object.Resolve<TileLayoutSpriteGroup>();
	if (group == nullptr || group->dts.seq.empty()) return {};

	const PalSpriteID &ps = group->dts.seq.front().image;
	return {ps.sprite + to_underlying(TreeGrowthStage::Growing3), ps.pal};
}

bool GetNewTreeList(const TileInfo *ti, TreeType treetype, uint trees, std::array<TreeListEnt, 4> &te)
{
	TreeTileResolverObject object(treetype, ti->tile);
	const auto *group = object.Resolve<TileLayoutSpriteGroup>();
	if (group == nullptr) return false;

	auto processor = group->ProcessRegisters(object, nullptr);
	auto dts = processor.GetLayout();

	uint8_t variant = CountBits(ti->tile.base() + ti->x + ti->y);
	const Coord2D<uint8_t> *d = GetTreePositions(GB(variant, 2, 2));

	/* put the trees to draw in a list */
	TreeGrowthStage growth = GetTreeGrowth(ti->tile);

	for (uint i = 0; i < trees; i++) {
		uint index = i % dts.seq.size();

		te[i].sprite = dts.seq[index].image.sprite + (i == trees - 1 ? to_underlying(growth) : 3);
		te[i].pal = dts.seq[index].image.pal;
		te[i].x = d->x;
		te[i].y = d->y;
		d++;
	}

	return true;
}
