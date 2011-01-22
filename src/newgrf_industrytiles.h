/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_industrytiles.h NewGRF handling of industry tiles. */

#ifndef NEWGRF_INDUSTRYTILES_H
#define NEWGRF_INDUSTRYTILES_H

#include "tile_cmd.h"
#include "newgrf_animation_type.h"
#include "newgrf_industries.h"
#include "core/random_func.hpp"

bool DrawNewIndustryTile(TileInfo *ti, Industry *i, IndustryGfx gfx, const IndustryTileSpec *inds);
uint16 GetIndustryTileCallback(CallbackID callback, uint32 param1, uint32 param2, IndustryGfx gfx_id, Industry *industry, TileIndex tile);
CommandCost PerformIndustryTileSlopeCheck(TileIndex ind_base_tile, TileIndex ind_tile, const IndustryTileSpec *its, IndustryType type, IndustryGfx gfx, uint itspec_index, uint16 initial_random_bits, Owner founder, IndustryAvailabilityCallType creation_type);

void AnimateNewIndustryTile(TileIndex tile);
bool StartStopIndustryTileAnimation(TileIndex tile, IndustryAnimationTrigger iat, uint32 random = Random());
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
