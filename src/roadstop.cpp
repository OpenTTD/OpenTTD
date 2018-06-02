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
#include "vehicle_func.h"

#include "safeguards.h"

/** The pool of roadstops. */
RoadStopPool _roadstop_pool("RoadStop");
INSTANTIATE_POOL_METHODS(RoadStop)

/**
 * De-Initializes RoadStops.
 */
RoadStop::~RoadStop()
{
	/* When we are the head we need to free the entries */
	if (HasBit(this->status, RSSFB_BASE_ENTRY)) {
		delete this->east;
		delete this->west;
	}

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
		if (!HasTileAnyRoadSubType(rs->xy, v->rtid.basetype, v->compatible_subtypes)) continue;
		/* The vehicle is articulated and can therefore not go to a standard road stop. */
		if (IsStandardRoadStopTile(rs->xy) && v->HasArticulatedPart()) continue;

		/* The vehicle can actually go to this road stop. So, return it! */
		return rs;
	}

	return NULL;
}

/**
 * Join this road stop to another 'base' road stop if possible;
 * fill all necessary data to become an actual drive through road stop.
 * Also update the length etc.
 */
void RoadStop::MakeDriveThrough()
{
	assert(this->east == NULL && this->west == NULL);

	RoadStopType rst = GetRoadStopType(this->xy);
	DiagDirection dir = GetRoadStopDir(this->xy);
	/* Use absolute so we always go towards the northern tile */
	TileIndexDiff offset = abs(TileOffsByDiagDir(dir));

	/* Information about the tile north of us */
	TileIndex north_tile = this->xy - offset;
	bool north = IsDriveThroughRoadStopContinuation(this->xy, north_tile);
	RoadStop *rs_north = north ? RoadStop::GetByTile(north_tile, rst) : NULL;

	/* Information about the tile south of us */
	TileIndex south_tile = this->xy + offset;
	bool south = IsDriveThroughRoadStopContinuation(this->xy, south_tile);
	RoadStop *rs_south = south ? RoadStop::GetByTile(south_tile, rst) : NULL;

	/* Amount of road stops that will be added to the 'northern' head */
	int added = 1;
	if (north && rs_north->east != NULL) { // (east != NULL) == (west != NULL)
		/* There is a more northern one, so this can join them */
		this->east = rs_north->east;
		this->west = rs_north->west;

		if (south && rs_south->east != NULL) { // (east != NULL) == (west != NULL)
			/* There more southern tiles too, they must 'join' us too */
			ClrBit(rs_south->status, RSSFB_BASE_ENTRY);
			this->east->occupied += rs_south->east->occupied;
			this->west->occupied += rs_south->west->occupied;

			/* Free the now unneeded entry structs */
			delete rs_south->east;
			delete rs_south->west;

			/* Make all 'children' of the southern tile take the new master */
			for (; IsDriveThroughRoadStopContinuation(this->xy, south_tile); south_tile += offset) {
				rs_south = RoadStop::GetByTile(south_tile, rst);
				if (rs_south->east == NULL) break;
				rs_south->east = rs_north->east;
				rs_south->west = rs_north->west;
				added++;
			}
		}
	} else if (south && rs_south->east != NULL) { // (east != NULL) == (west != NULL)
		/* There is one to the south, but not to the north... so we become 'parent' */
		this->east = rs_south->east;
		this->west = rs_south->west;
		SetBit(this->status, RSSFB_BASE_ENTRY);
		ClrBit(rs_south->status, RSSFB_BASE_ENTRY);
	} else {
		/* We are the only... so we are automatically the master */
		this->east = new Entry();
		this->west = new Entry();
		SetBit(this->status, RSSFB_BASE_ENTRY);
	}

	/* Now update the lengths */
	added *= TILE_SIZE;
	this->east->length += added;
	this->west->length += added;
}

/**
 * Prepare for removal of this stop; update other neighbouring stops
 * if needed. Also update the length etc.
 */
void RoadStop::ClearDriveThrough()
{
	assert(this->east != NULL && this->west != NULL);

	RoadStopType rst = GetRoadStopType(this->xy);
	DiagDirection dir = GetRoadStopDir(this->xy);
	/* Use absolute so we always go towards the northern tile */
	TileIndexDiff offset = abs(TileOffsByDiagDir(dir));

	/* Information about the tile north of us */
	TileIndex north_tile = this->xy - offset;
	bool north = IsDriveThroughRoadStopContinuation(this->xy, north_tile);
	RoadStop *rs_north = north ? RoadStop::GetByTile(north_tile, rst) : NULL;

	/* Information about the tile south of us */
	TileIndex south_tile = this->xy + offset;
	bool south = IsDriveThroughRoadStopContinuation(this->xy, south_tile);
	RoadStop *rs_south = south ? RoadStop::GetByTile(south_tile, rst) : NULL;

	/* Must only be cleared after we determined which neighbours are
	 * part of our little entry 'queue' */
	DoClearSquare(this->xy);

	if (north) {
		/* There is a tile to the north, so we can't clear ourselves. */
		if (south) {
			/* There are more southern tiles too, they must be split;
			 * first make the new southern 'base' */
			SetBit(rs_south->status, RSSFB_BASE_ENTRY);
			rs_south->east = new Entry();
			rs_south->west = new Entry();

			/* Keep track of the base because we need it later on */
			RoadStop *rs_south_base = rs_south;
			TileIndex base_tile = south_tile;

			/* Make all (even more) southern stops part of the new entry queue */
			for (south_tile += offset; IsDriveThroughRoadStopContinuation(base_tile, south_tile); south_tile += offset) {
				rs_south = RoadStop::GetByTile(south_tile, rst);
				rs_south->east = rs_south_base->east;
				rs_south->west = rs_south_base->west;
			}

			/* Find the other end; the northern most tile */
			for (; IsDriveThroughRoadStopContinuation(base_tile, north_tile); north_tile -= offset) {
				rs_north = RoadStop::GetByTile(north_tile, rst);
			}

			/* We have to rebuild the entries because we cannot easily determine
			 * how full each part is. So instead of keeping and maintaining a list
			 * of vehicles and using that to 'rebuild' the occupied state we just
			 * rebuild it from scratch as that removes lots of maintenance code
			 * for the vehicle list and it's faster in real games as long as you
			 * do not keep split and merge road stop every tick by the millions. */
			rs_south_base->east->Rebuild(rs_south_base);
			rs_south_base->west->Rebuild(rs_south_base);

			assert(HasBit(rs_north->status, RSSFB_BASE_ENTRY));
			rs_north->east->Rebuild(rs_north);
			rs_north->west->Rebuild(rs_north);
		} else {
			/* Only we left, so simple update the length. */
			rs_north->east->length -= TILE_SIZE;
			rs_north->west->length -= TILE_SIZE;
		}
	} else if (south) {
		/* There is only something to the south. Hand over the base entry */
		SetBit(rs_south->status, RSSFB_BASE_ENTRY);
		rs_south->east->length -= TILE_SIZE;
		rs_south->west->length -= TILE_SIZE;
	} else {
		/* We were the last */
		delete this->east;
		delete this->west;
	}

	/* Make sure we don't get used for something 'incorrect' */
	ClrBit(this->status, RSSFB_BASE_ENTRY);
	this->east = NULL;
	this->west = NULL;
}

/**
 * Leave the road stop
 * @param rv the vehicle that leaves the stop
 */
void RoadStop::Leave(RoadVehicle *rv)
{
	if (IsStandardRoadStopTile(rv->tile)) {
		/* Vehicle is leaving a road stop tile, mark bay as free */
		this->FreeBay(HasBit(rv->state, RVS_USING_SECOND_BAY));
		this->SetEntranceBusy(false);
	} else {
		/* Otherwise just leave the drive through's entry cache. */
		this->GetEntry(DirToDiagDir(rv->direction))->Leave(rv);
	}
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

		/* Mark the station entrance as busy */
		this->SetEntranceBusy(true);
		return true;
	}

	/* Vehicles entering a drive-through stop from the 'normal' side use first bay (bay 0). */
	this->GetEntry(DirToDiagDir(rv->direction))->Enter(rv);

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

/**
 * Leave the road stop
 * @param rv the vehicle that leaves the stop
 */
void RoadStop::Entry::Leave(const RoadVehicle *rv)
{
	this->occupied -= rv->gcache.cached_total_length;
	assert(this->occupied >= 0);
}

/**
 * Enter the road stop
 * @param rv the vehicle that enters the stop
 */
void RoadStop::Entry::Enter(const RoadVehicle *rv)
{
	/* we cannot assert on this->occupied < this->length because of the
	 * remote possibility that RVs are running through each other when
	 * trying to prevention an infinite jam. */
	this->occupied += rv->gcache.cached_total_length;
}

/**
 * Checks whether the 'next' tile is still part of the road same drive through
 * stop 'rs' in the same direction for the same vehicle.
 * @param rs   the road stop tile to check against
 * @param next the 'next' tile to check
 * @return true if the 'next' tile is part of the road stop at 'next'.
 */
/* static */ bool RoadStop::IsDriveThroughRoadStopContinuation(TileIndex rs, TileIndex next)
{
	return IsTileType(next, MP_STATION) &&
			GetStationIndex(next) == GetStationIndex(rs) &&
			GetStationType(next) == GetStationType(rs) &&
			GetRoadStopDir(next) == GetRoadStopDir(rs) &&
			IsDriveThroughStopTile(next);
}

typedef std::list<const RoadVehicle *> RVList; ///< A list of road vehicles

/** Helper for finding RVs in a road stop. */
struct RoadStopEntryRebuilderHelper {
	RVList vehicles;   ///< The list of vehicles to possibly add to.
	DiagDirection dir; ///< The direction the vehicle has to face to be added.
};

/**
 * Add road vehicles to the station's list if needed.
 * @param v the found vehicle
 * @param data the extra data used to make our decision
 * @return always NULL
 */
Vehicle *FindVehiclesInRoadStop(Vehicle *v, void *data)
{
	RoadStopEntryRebuilderHelper *rserh = (RoadStopEntryRebuilderHelper*)data;
	/* Not a RV or not in the right direction or crashed :( */
	if (v->type != VEH_ROAD || DirToDiagDir(v->direction) != rserh->dir || !v->IsPrimaryVehicle() || (v->vehstatus & VS_CRASHED) != 0) return NULL;

	RoadVehicle *rv = RoadVehicle::From(v);
	/* Don't add ones not in a road stop */
	if (rv->state < RVSB_IN_ROAD_STOP) return NULL;

	/* Do not add duplicates! */
	for (RVList::iterator it = rserh->vehicles.begin(); it != rserh->vehicles.end(); it++) {
		if (rv == *it) return NULL;
	}

	rserh->vehicles.push_back(rv);
	return NULL;
}

/**
 * Rebuild, from scratch, the vehicles and other metadata on this stop.
 * @param rs   the roadstop this entry is part of
 * @param side the side of the road stop to look at
 */
void RoadStop::Entry::Rebuild(const RoadStop *rs, int side)
{
	assert(HasBit(rs->status, RSSFB_BASE_ENTRY));

	DiagDirection dir = GetRoadStopDir(rs->xy);
	if (side == -1) side = (rs->east == this);

	RoadStopEntryRebuilderHelper rserh;
	rserh.dir = side ? dir : ReverseDiagDir(dir);

	this->length = 0;
	TileIndexDiff offset = abs(TileOffsByDiagDir(dir));
	for (TileIndex tile = rs->xy; IsDriveThroughRoadStopContinuation(rs->xy, tile); tile += offset) {
		this->length += TILE_SIZE;
		FindVehicleOnPos(tile, &rserh, FindVehiclesInRoadStop);
	}

	this->occupied = 0;
	for (RVList::iterator it = rserh.vehicles.begin(); it != rserh.vehicles.end(); it++) {
		this->occupied += (*it)->gcache.cached_total_length;
	}
}


/**
 * Check the integrity of the data in this struct.
 * @param rs the roadstop this entry is part of
 */
void RoadStop::Entry::CheckIntegrity(const RoadStop *rs) const
{
	if (!HasBit(rs->status, RSSFB_BASE_ENTRY)) return;

	/* The tile 'before' the road stop must not be part of this 'line' */
	assert(!IsDriveThroughRoadStopContinuation(rs->xy, rs->xy - abs(TileOffsByDiagDir(GetRoadStopDir(rs->xy)))));

	Entry temp;
	temp.Rebuild(rs, rs->east == this);
	if (temp.length != this->length || temp.occupied != this->occupied) NOT_REACHED();
}
