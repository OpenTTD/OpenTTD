/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_industrytiles.h NewGRF handling of industry tiles. */

#ifndef NEWGRF_INDUSTRYTILES_H
#define NEWGRF_INDUSTRYTILES_H

#include "newgrf_animation_type.h"
#include "newgrf_industries.h"
#include "core/random_func.hpp"

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
	uint32_t GetVariable(byte variable, [[maybe_unused]] uint32_t parameter, bool *available) const override;
	uint32_t GetTriggers() const override;
};

/** Resolver for industry tiles. */
struct IndustryTileResolverObject : public ResolverObject {
	IndustryTileScopeResolver indtile_scope; ///< Scope resolver for the industry tile.
	IndustriesScopeResolver ind_scope;       ///< Scope resolver for the industry owning the tile.
	IndustryGfx gfx;

	IndustryTileResolverObject(IndustryGfx gfx, TileIndex tile, Industry *indus,
			CallbackID callback = CBID_NO_CALLBACK, uint32_t callback_param1 = 0, uint32_t callback_param2 = 0);

	ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0) override
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
uint16_t GetIndustryTileCallback(CallbackID callback, uint32_t param1, uint32_t param2, IndustryGfx gfx_id, Industry *industry, TileIndex tile);
CommandCost PerformIndustryTileSlopeCheck(TileIndex ind_base_tile, TileIndex ind_tile, const IndustryTileSpec *its, IndustryType type, IndustryGfx gfx, size_t layout_index, uint16_t initial_random_bits, Owner founder, IndustryAvailabilityCallType creation_type);

void AnimateNewIndustryTile(TileIndex tile);
bool StartStopIndustryTileAnimation(TileIndex tile, IndustryAnimationTrigger iat, uint32_t random = Random());
bool StartStopIndustryTileAnimation(const Industry *ind, IndustryAnimationTrigger iat);


/** Available industry tile triggers. */
enum IndustryTileTrigger {
	INDTILE_TRIGGER_TILE_LOOP       = 0x01, ///< The tile of the industry has been triggered during the tileloop.
	INDUSTRY_TRIGGER_INDUSTRY_TICK  = 0x02, ///< The industry has been triggered via its tick.
	INDUSTRY_TRIGGER_RECEIVED_CARGO = 0x04, ///< Cargo has been delivered.
};
void TriggerIndustryTile(TileIndex t, IndustryTileTrigger trigger);
void TriggerIndustry(Industry *ind, IndustryTileTrigger trigger);

#endif /* NEWGRF_INDUSTRYTILES_H */
