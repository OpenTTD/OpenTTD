/* $Id$ */

/** @file newgrf_industries.h */

#ifndef NEWGRF_INDUSTRIES_H
#define NEWGRF_INDUSTRIES_H

#include "industry.h"
#include "newgrf_spritegroup.h"

/* in newgrf_industry.cpp */
uint32 IndustryGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available);
uint16 GetIndustryCallback(CallbackID callback, uint32 param1, uint32 param2, Industry *industry, IndustryType type, TileIndex tile);
uint32 GetIndustryIDAtOffset(TileIndex new_tile, TileIndex old_tile, const Industry *i);
void IndustryProductionCallback(Industry *ind, int reason);
bool CheckIfCallBackAllowsCreation(TileIndex tile, IndustryType type, uint itspec_index);
bool CheckIfCallBackAllowsAvailability(IndustryType type, IndustryAvailabilityCallType creation_type);

IndustryType MapNewGRFIndustryType(IndustryType grf_type, uint32 grf_id);

/* in newgrf_industrytiles.cpp*/
uint32 IndustryTileGetRandomBits(const ResolverObject *object);
uint32 IndustryTileGetTriggers(const ResolverObject *object);
void IndustryTileSetTriggers(const ResolverObject *object, int triggers);

uint32 GetNearbyIndustryTileInformation(byte parameter, TileIndex tile, IndustryID index);

#endif /* NEWGRF_INDUSTRIES_H */
