/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_industrytiles.h NewGRF handling of industry tiles. */

#ifndef NEWGRF_INDUSTRYTILES_H
#define NEWGRF_INDUSTRYTILES_H

#include "newgrf_animation_type.h"
#include "newgrf_industries.h"

/** Resolver for the industry tiles scope. */
struct IndustryTileScopeResolver : public ScopeResolver {
	Industry *industry; ///< Industry owning the tiles.
	TileIndex tile;     ///< %Tile being resolved.

	/**
	 * Constructor of the scope resolver for the industry tile.
	 * @param ro Surrounding resolver.
	 * @param industry %Industry owning the tile.
	 * @param tile %Tile of the industry.
	 */
	IndustryTileScopeResolver(ResolverObject &ro, Industry *industry, TileIndex tile)
		: ScopeResolver(ro), industry(industry), tile(tile)
	{
	}

	uint32_t GetRandomBits() const override;
	uint32_t GetVariable(uint8_t variable, [[maybe_unused]] uint32_t parameter, bool &available) const override;
	uint32_t GetRandomTriggers() const override;
};

/** Resolver for industry tiles. */
struct IndustryTileResolverObject : public SpecializedResolverObject<IndustryRandomTriggers> {
	IndustryTileScopeResolver indtile_scope; ///< Scope resolver for the industry tile.
	IndustriesScopeResolver ind_scope;       ///< Scope resolver for the industry owning the tile.
	IndustryGfx gfx;

	IndustryTileResolverObject(IndustryGfx gfx, TileIndex tile, Industry *indus,
			CallbackID callback = CBID_NO_CALLBACK, uint32_t callback_param1 = 0, uint32_t callback_param2 = 0);

	ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, uint8_t relative = 0) override
	{
		switch (scope) {
			case VSG_SCOPE_SELF: return &indtile_scope;
			case VSG_SCOPE_PARENT: return &ind_scope;
			default: return ResolverObject::GetScope(scope, relative);
		}
	}

	GrfSpecFeature GetFeature() const override;
	uint32_t GetDebugID() const override;
};

bool DrawNewIndustryTile(TileInfo *ti, Industry *i, IndustryGfx gfx, const IndustryTileSpec *inds);
uint16_t GetIndustryTileCallback(CallbackID callback, uint32_t param1, uint32_t param2, IndustryGfx gfx_id, Industry *industry, TileIndex tile, std::span<int32_t> regs100 = {});
CommandCost PerformIndustryTileSlopeCheck(TileIndex ind_base_tile, TileIndex ind_tile, const IndustryTileSpec *its, IndustryType type, IndustryGfx gfx, size_t layout_index, uint16_t initial_random_bits, Owner founder, IndustryAvailabilityCallType creation_type);

void AnimateNewIndustryTile(TileIndex tile);
bool TriggerIndustryTileAnimation(TileIndex tile, IndustryAnimationTrigger iat);
bool TriggerIndustryTileAnimation_ConstructionStageChanged(TileIndex tile, bool first_call);
bool TriggerIndustryAnimation(const Industry *ind, IndustryAnimationTrigger iat);

void TriggerIndustryTileRandomisation(TileIndex t, IndustryRandomTrigger trigger);
void TriggerIndustryRandomisation(Industry *ind, IndustryRandomTrigger trigger);

#endif /* NEWGRF_INDUSTRYTILES_H */
