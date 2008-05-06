/* $Id$ */

/** @file newgrf_industries.h Functions for NewGRF industries. */

#ifndef NEWGRF_INDUSTRIES_H
#define NEWGRF_INDUSTRIES_H

#include "industry_type.h"
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

/** From where is callback CBID_INDUSTRY_AVAILABLE been called */
enum IndustryAvailabilityCallType {
	IACT_MAPGENERATION,   ///< during random map generation
	IACT_RANDOMCREATION,  ///< during creation of random ingame industry
	IACT_USERCREATION,    ///< from the Fund/build window
};

/* in newgrf_industry.cpp */
uint32 IndustryGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available);
uint16 GetIndustryCallback(CallbackID callback, uint32 param1, uint32 param2, Industry *industry, IndustryType type, TileIndex tile);
uint32 GetIndustryIDAtOffset(TileIndex new_tile, const Industry *i);
void IndustryProductionCallback(Industry *ind, int reason);
bool CheckIfCallBackAllowsCreation(TileIndex tile, IndustryType type, uint itspec_index, uint32 seed);
bool CheckIfCallBackAllowsAvailability(IndustryType type, IndustryAvailabilityCallType creation_type);

IndustryType MapNewGRFIndustryType(IndustryType grf_type, uint32 grf_id);

/* in newgrf_industrytiles.cpp*/
uint32 GetNearbyIndustryTileInformation(byte parameter, TileIndex tile, IndustryID index);

#endif /* NEWGRF_INDUSTRIES_H */
