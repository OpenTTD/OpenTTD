/* $Id$ */

#include "industry.h"
#include "macros.h"
#include "tile.h"


static inline uint GetIndustryIndex(TileIndex t)
{
	return _m[t].m2;
}

static inline Industry* GetIndustryByTile(TileIndex t)
{
	return GetIndustry(GetIndustryIndex(t));
}
