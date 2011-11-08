/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_industries.h Functions for NewGRF industries. */

#ifndef NEWGRF_INDUSTRIES_H
#define NEWGRF_INDUSTRIES_H

#include "command_type.h"
#include "company_type.h"
#include "newgrf_spritegroup.h"

/** When should the industry(tile) be triggered for random bits? */
enum IndustryTrigger {
	/** Triggered each tile loop */
	INDUSTRY_TRIGGER_TILELOOP_PROCESS = 1,
	/** Triggered (whole industry) each 256 ticks */
	INDUSTRY_TRIGGER_256_TICKS        = 2,
	/** Triggered on cargo delivery */
	INDUSTRY_TRIGGER_CARGO_DELIVERY   = 4,
};

/** From where is callback CBID_INDUSTRY_PROBABILITY been called */
enum IndustryAvailabilityCallType {
	IACT_MAPGENERATION,    ///< during random map generation
	IACT_RANDOMCREATION,   ///< during creation of random ingame industry
	IACT_USERCREATION,     ///< from the Fund/build window
	IACT_PROSPECTCREATION, ///< from the Fund/build using prospecting
};

/* in newgrf_industry.cpp */
uint32 IndustryGetVariable(const ResolverObject *object, byte variable, uint32 parameter, bool *available);
uint16 GetIndustryCallback(CallbackID callback, uint32 param1, uint32 param2, Industry *industry, IndustryType type, TileIndex tile);
uint32 GetIndustryIDAtOffset(TileIndex new_tile, const Industry *i, uint32 cur_grfid);
void IndustryProductionCallback(Industry *ind, int reason);
CommandCost CheckIfCallBackAllowsCreation(TileIndex tile, IndustryType type, uint layout, uint32 seed, uint16 initial_random_bits, Owner founder, IndustryAvailabilityCallType creation_type);
uint32 GetIndustryProbabilityCallback(IndustryType type, IndustryAvailabilityCallType creation_type, uint32 default_prob);
bool IndustryTemporarilyRefusesCargo(Industry *ind, CargoID cargo_type);

IndustryType MapNewGRFIndustryType(IndustryType grf_type, uint32 grf_id);

/* in newgrf_industrytiles.cpp*/
uint32 GetNearbyIndustryTileInformation(byte parameter, TileIndex tile, IndustryID index, bool signed_offsets = true);

#endif /* NEWGRF_INDUSTRIES_H */
