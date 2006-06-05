/* $Id$ */

#include "vehicle.h"


static inline bool IsShipInDepot(const Vehicle* v)
{
	assert(v->type == VEH_Ship);
	return v->u.ship.state == 0x80;
}

static inline bool IsShipInDepotStopped(const Vehicle* v)
{
	return IsShipInDepot(v) && v->vehstatus & VS_STOPPED;
}
