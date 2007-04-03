/* $Id$ */

/** @file src/roadveh.h Road vehicle states */

#ifndef ROADVEH_H
#define ROADVEH_H

#include "vehicle.h"


static inline bool IsRoadVehInDepot(const Vehicle* v)
{
	assert(v->type == VEH_ROAD);
	return v->u.road.state == 254;
}

static inline bool IsRoadVehInDepotStopped(const Vehicle* v)
{
	return IsRoadVehInDepot(v) && v->vehstatus & VS_STOPPED;
}

void CcBuildRoadVeh(bool success, TileIndex tile, uint32 p1, uint32 p2);
void CcCloneRoadVeh(bool success, TileIndex tile, uint32 p1, uint32 p2);

#endif /* ROADVEH_H */
