/* $Id$ */

#ifndef RAIL_MAP_H
#define RAIL_MAP_H

#include "tile.h"

// TODO remove this by moving to the same bits as GetRailType()
static inline RailType GetRailTypeCrossing(TileIndex t)
{
	return (RailType)GB(_m[t].m4, 0, 4);
}

#endif
