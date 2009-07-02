/* $Id$ */

/** @file roadstop.cpp Implementation of the roadstop base class. */

#include "stdafx.h"
#include "roadveh.h"
#include "station_map.h"
#include "core/pool_func.hpp"
#include "roadstop_base.h"
#include "station_base.h"

RoadStopPool _roadstop_pool("RoadStop");
INSTANTIATE_POOL_METHODS(RoadStop)

/** De-Initializes a RoadStops. This includes clearing all slots that vehicles might
  * have and unlinks it from the linked list of road stops at the given station
  */
RoadStop::~RoadStop()
{
	if (CleaningPool()) return;

	/* Clear the slot assignment of all vehicles heading for this road stop */
	if (this->num_vehicles != 0) {
		RoadVehicle *rv;
		FOR_ALL_ROADVEHICLES(rv) {
			if (rv->slot == this) ClearSlot(rv);
		}
	}
	assert(this->num_vehicles == 0);
}

/**
 * Get the next road stop accessible by this vehicle.
 * @param v the vehicle to get the next road stop for.
 * @return the next road stop accessible.
 */
RoadStop *RoadStop::GetNextRoadStop(const RoadVehicle *v) const
{
	for (RoadStop *rs = this->next; rs != NULL; rs = rs->next) {
		/* The vehicle cannot go to this roadstop (different roadtype) */
		if ((GetRoadTypes(rs->xy) & v->compatible_roadtypes) == ROADTYPES_NONE) continue;
		/* The vehicle is articulated and can therefor not go the a standard road stop */
		if (IsStandardRoadStopTile(rs->xy) && v->RoadVehHasArticPart()) continue;

		/* The vehicle can actually go to this road stop. So, return it! */
		return rs;
	}

	return NULL;
}

/**
 * Find a roadstop at given tile
 * @param tile tile with roadstop
 * @param type roadstop type
 * @return pointer to RoadStop
 * @pre there has to be roadstop of given type there!
 */
/* static */ RoadStop *RoadStop::GetByTile(TileIndex tile, RoadStopType type)
{
	const Station *st = Station::GetByTile(tile);

	for (RoadStop *rs = st->GetPrimaryRoadStop(type);; rs = rs->next) {
		if (rs->xy == tile) return rs;
		assert(rs->next != NULL);
	}
}

void InitializeRoadStops()
{
	_roadstop_pool.CleanPool();
}
