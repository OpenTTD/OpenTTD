/* $Id$ */

#ifndef AIRCRAFT_H
#define AIRCRAFT_H

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

uint16 AircraftDefaultCargoCapacity(CargoID cid, EngineID engine_type);

void CcCloneAircraft(bool success, TileIndex tile, uint32 p1, uint32 p2);
void HandleAircraftEnterHangar(Vehicle *v);

#endif /* AIRCRAFT_H */
