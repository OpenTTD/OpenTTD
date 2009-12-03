/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file roadstop.cpp Implementation of the roadstop base class. */

#include "stdafx.h"
#include "roadveh.h"
#include "core/pool_func.hpp"
#include "roadstop_base.h"
#include "station_base.h"

RoadStopPool _roadstop_pool("RoadStop");
INSTANTIATE_POOL_METHODS(RoadStop)

/**
 * De-Initializes RoadStops.
 */
RoadStop::~RoadStop()
{
	if (CleaningPool()) return;
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
		if (IsStandardRoadStopTile(rs->xy) && v->HasArticulatedPart()) continue;

		/* The vehicle can actually go to this road stop. So, return it! */
		return rs;
	}

	return NULL;
}

/**
 * Leave the road stop
 * @param rv the vehicle that leaves the stop
 */
void RoadStop::Leave(RoadVehicle *rv)
{
	/* Vehicle is leaving a road stop tile, mark bay as free
	 * For drive-through stops, only do it if the vehicle stopped here */
	if (IsStandardRoadStopTile(rv->tile) || HasBit(rv->state, RVS_IS_STOPPING)) {
		this->FreeBay(HasBit(rv->state, RVS_USING_SECOND_BAY));
		ClrBit(rv->state, RVS_IS_STOPPING);
	}
	if (IsStandardRoadStopTile(rv->tile)) this->SetEntranceBusy(false);
}

/**
 * Enter the road stop
 * @param rv   the vehicle that enters the stop
 * @return whether the road stop could actually be entered
 */
bool RoadStop::Enter(RoadVehicle *rv)
{
	if (IsStandardRoadStopTile(this->xy)) {
		/* For normal (non drive-through) road stops
		 * Check if station is busy or if there are no free bays or whether it is a articulated vehicle. */
		if (this->IsEntranceBusy() || !this->HasFreeBay() || rv->HasArticulatedPart()) return false;

		SetBit(rv->state, RVS_IN_ROAD_STOP);

		/* Allocate a bay and update the road state */
		uint bay_nr = this->AllocateBay();
		SB(rv->state, RVS_USING_SECOND_BAY, 1, bay_nr);

		/* Mark the station entrace as busy */
		this->SetEntranceBusy(true);
		return true;
	}

	/* Vehicles entering a drive-through stop from the 'normal' side use first bay (bay 0). */
	byte side = ((DirToDiagDir(rv->direction) == ReverseDiagDir(GetRoadStopDir(this->xy))) == (rv->overtaking == 0)) ? 0 : 1;

	if (!this->IsFreeBay(side)) return false;

	/* Check if the vehicle is stopping at this road stop */
	if (GetRoadStopType(this->xy) == (rv->IsBus() ? ROADSTOP_BUS : ROADSTOP_TRUCK) &&
			rv->current_order.ShouldStopAtStation(rv, GetStationIndex(this->xy))) {
		SetBit(rv->state, RVS_IS_STOPPING);
		this->AllocateDriveThroughBay(side);
	}

	/* Indicate if vehicle is using second bay. */
	if (side == 1) SetBit(rv->state, RVS_USING_SECOND_BAY);
	/* Indicate a drive-through stop */
	SetBit(rv->state, RVS_IN_DT_ROAD_STOP);
	return true;
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
