/* $Id$ */

#ifndef SHIP_H
#define SHIP_H

#include "vehicle.h"

void CcCloneShip(bool success, TileIndex tile, uint32 p1, uint32 p2);
void RecalcShipStuff(Vehicle *v);

static inline bool IsShipInDepot(const Vehicle* v)
{
	assert(v->type == VEH_Ship);
	return v->u.ship.state == 0x80;
}

static inline bool IsShipInDepotStopped(const Vehicle* v)
{
	return IsShipInDepot(v) && v->vehstatus & VS_STOPPED;
}

#endif /* SHIP_H */
