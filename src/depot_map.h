/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file depot_map.h Map related accessors for depots. */

#ifndef DEPOT_MAP_H
#define DEPOT_MAP_H

#include "station_map.h"

static const uint8_t DEPOT_TYPE = 0x02;

/**
 * Check if a tile is a depot and it is a depot of the given type.
 */
inline bool IsDepotTypeTile(Tile tile, TransportType type)
{
	if (type == TRANSPORT_AIR) return IsHangarTile(tile);

	if (GB(tile.m5(), 6, 2) != DEPOT_TYPE) return false;

	switch (type) {
		default: NOT_REACHED();
		case TRANSPORT_RAIL:
			return IsTileType(tile, MP_RAILWAY);
		case TRANSPORT_ROAD:
			return IsTileType(tile, MP_ROAD);
		case TRANSPORT_WATER:
			return IsTileType(tile, MP_WATER);
	}
}

/**
 * Is the given tile a tile with a depot on it?
 * @param tile the tile to check
 * @return true if and only if there is a depot on the tile.
 */
inline bool IsDepotTile(Tile tile)
{
	TileType type = GetTileType(tile);
	if (type == MP_STATION) return IsHangar(tile);
	if (GB(tile.m5(), 6, 2) != DEPOT_TYPE) return false;

	return type == MP_RAILWAY || type == MP_ROAD || type == MP_WATER;
}

extern DepotID GetHangarIndex(TileIndex t);

/**
 * Get the index of which depot is attached to the tile.
 * @param t the tile
 * @pre IsDepotTile(t)
 * @return DepotID
 */
inline DepotID GetDepotIndex(Tile t)
{
	assert(IsDepotTile(t));

	/* Hangars don't store depot id on m2. */
	if (IsTileType(t, MP_STATION)) return GetHangarIndex(t);

	return t.m2();
}

/**
 * Get the type of vehicles that can use a depot
 * @param t The tile
 * @pre IsDepotTile(t)
 * @return the type of vehicles that can use the depot
 */
inline VehicleType GetDepotVehicleType(Tile t)
{
	switch (GetTileType(t)) {
		default: NOT_REACHED();
		case MP_RAILWAY: return VEH_TRAIN;
		case MP_ROAD:    return VEH_ROAD;
		case MP_WATER:   return VEH_SHIP;
		case MP_STATION: return VEH_AIRCRAFT;
	}
}

/** Return true if a tile belongs to an extended depot. */
static inline bool IsExtendedDepot(Tile tile) {
	assert(IsValidTile(tile));
	assert(IsDepotTile(tile));
	if (IsAirportTile(tile)) return false;
	return HasBit(tile.m5(), 5);
}

/** Return true if a tile belongs to an extended depot. */
static inline bool IsExtendedDepotTile(TileIndex tile) {
	if (!IsValidTile(tile)) return false;
	if (!IsDepotTile(tile)) return false;
	return IsExtendedDepot(tile);
}

/**
 * Has this depot some vehicle servicing or stopped inside?
 * @param tile tile of the depot.
 * @param south_dir In case of road transport, return reservation facing south if true.
 * @return The type of reservation on this tile (empty, servicing or occupied).
 * @pre is a depot tile
 */
static inline DepotReservation GetDepotReservation(Tile t, bool south_dir = false)
{
	assert(IsDepotTile(t));
	if (!IsExtendedDepot(t)) return DEPOT_RESERVATION_EMPTY;
	if (south_dir) {
		assert(GetDepotVehicleType(t) == VEH_ROAD);
		return (DepotReservation)GB(t.m6(), 4, 2);
	}
	return (DepotReservation)GB(t.m4(), 6, 2);
}

/**
 * Is this a platform/depot tile full with stopped vehicles?
 * @param tile tile of the depot.
 * @param south_dir In case of road transport, check reservation facing south if true.
 * @return the type of reservation of the depot.
 * @pre is a depot tile
 */
static inline bool IsDepotFullWithStoppedVehicles(TileIndex t, bool south_dir = false)
{
	assert(IsDepotTile(t));
	if (!IsExtendedDepot(t)) return false;
	return GetDepotReservation(t, south_dir) == DEPOT_RESERVATION_FULL_STOPPED_VEH;
}


/**
 * Has this depot tile/platform some vehicle inside?
 * @param tile tile of the depot.
 * @param south_dir In case of road transport, check reservation facing south if true.
 * @return true iff depot tile/platform has no vehicle.
 * @pre IsExtendedDepotTile
 */
static inline bool IsExtendedDepotEmpty(TileIndex t, bool south_dir = false)
{
	assert(IsExtendedDepotTile(t));
	return GetDepotReservation(t, south_dir) == DEPOT_RESERVATION_EMPTY;
}

/**
 * Mark whether this depot has a ship inside.
 * @param tile of the depot.
 * @param reservation type of reservation
 * @param south_dir Whether to set south direction reservation.
 * @pre tile is an extended ship depot.
 */
static inline void SetDepotReservation(Tile t, DepotReservation reservation, bool south_dir = false)
{
	assert(IsDepotTile(t));
	if (!IsExtendedDepot(t)) return;
	switch (GetTileType(t)) {
		default: NOT_REACHED();
		case MP_RAILWAY:
			break;
		case MP_ROAD:
			if (south_dir) {
				SB(t.m6(), 4, 2, reservation);
				return;
			}
			break;
		case MP_WATER:
			assert(GetDepotReservation(t) == GetDepotReservation(GetOtherShipDepotTile(t)));
			SB(Tile(GetOtherShipDepotTile(t)).m4(), 6, 2, reservation);
			break;
		case MP_STATION: return;
	}

	SB(t.m4(), 6, 2, reservation);
}

void UpdateExtendedDepotReservation(Vehicle *v, bool state);

#endif /* DEPOT_MAP_H */
