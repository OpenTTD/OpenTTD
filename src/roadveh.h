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


/**
 * This class 'wraps' Vehicle; you do not actually instantiate this class.
 * You create a Vehicle using AllocateVehicle, so it is added to the pool
 * and you reinitialize that to a Train using:
 *   v = new (v) RoadVehicle();
 *
 * As side-effect the vehicle type is set correctly.
 */
struct RoadVehicle : public Vehicle {
	/** Initializes the Vehicle to a road vehicle */
	RoadVehicle() { this->type = VEH_ROAD; }

	/** We want to 'destruct' the right class. */
	virtual ~RoadVehicle() {}

	const char *GetTypeString() { return "road vehicle"; }
	void MarkDirty();
};

#endif /* ROADVEH_H */
