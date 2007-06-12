/* $Id$ */

/** @file newgrf_industries.h */

#ifndef NEWGRF_INDUSTRIES_H
#define NEWGRF_INDUSTRIES_H

#include "industry.h"
#include "newgrf_spritegroup.h"

uint32 IndustryGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available);
uint16 GetIndustryCallback(uint16 callback, uint32 param1, uint32 param2, Industry *industry, TileIndex tile);
uint32 GetIndustryIDAtOffset(TileIndex new_tile, TileIndex old_tile, const Industry *i);

#endif /* NEWGRF_INDUSTRIES_H */
