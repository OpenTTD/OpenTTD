/* $Id$ */

#include "station_map.h"
#include "vehicle.h"


static inline bool IsAircraftInHangar(const Vehicle* v)
{
	assert(v->type == VEH_Aircraft);
	return v->vehstatus & VS_HIDDEN && IsHangarTile(v->tile);
}

static inline bool IsAircraftInHangarStopped(const Vehicle* v)
{
	return IsAircraftInHangar(v) && v->vehstatus & VS_STOPPED;
}
