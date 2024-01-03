/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file station_map.h Maps accessors for stations. */

#ifndef STATION_MAP_H
#define STATION_MAP_H

#include "rail_map.h"
#include "road_map.h"
#include "water_map.h"
#include "station_func.h"
#include "rail.h"
#include "road.h"

typedef byte StationGfx; ///< Index of station graphics. @see _station_display_datas

/**
 * Get StationID from a tile
 * @param t Tile to query station ID from
 * @pre IsTileType(t, MP_STATION)
 * @return Station ID of the station at \a t
 */
static inline StationID GetStationIndex(Tile t)
{
	assert(IsTileType(t, MP_STATION));
	return (StationID)t.m2();
}


static const int GFX_DOCK_BASE_WATER_PART          =  4; ///< The offset for the water parts.
static const int GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET =  4; ///< The offset for the drive through parts.

/**
 * Get the station type of this tile
 * @param t the tile to query
 * @pre IsTileType(t, MP_STATION)
 * @return the station type
 */
static inline StationType GetStationType(Tile t)
{
	assert(IsTileType(t, MP_STATION));
	return (StationType)GB(t.m6(), 3, 3);
}

/**
 * Get the road stop type of this tile
 * @param t the tile to query
 * @pre GetStationType(t) == STATION_TRUCK || GetStationType(t) == STATION_BUS
 * @return the road stop type
 */
static inline RoadStopType GetRoadStopType(Tile t)
{
	assert(GetStationType(t) == STATION_TRUCK || GetStationType(t) == STATION_BUS);
	return GetStationType(t) == STATION_TRUCK ? ROADSTOP_TRUCK : ROADSTOP_BUS;
}

/**
 * Get the station graphics of this tile
 * @param t the tile to query
 * @pre IsTileType(t, MP_STATION)
 * @return the station graphics
 */
static inline StationGfx GetStationGfx(Tile t)
{
	assert(IsTileType(t, MP_STATION));
	return t.m5();
}

/**
 * Set the station graphics of this tile
 * @param t the tile to update
 * @param gfx the new graphics
 * @pre IsTileType(t, MP_STATION)
 */
static inline void SetStationGfx(Tile t, StationGfx gfx)
{
	assert(IsTileType(t, MP_STATION));
	t.m5() = gfx;
}

/**
 * Is this station tile a rail station?
 * @param t the tile to get the information from
 * @pre IsTileType(t, MP_STATION)
 * @return true if and only if the tile is a rail station
 */
static inline bool IsRailStation(Tile t)
{
	return GetStationType(t) == STATION_RAIL;
}

/**
 * Is this tile a station tile and a rail station?
 * @param t the tile to get the information from
 * @return true if and only if the tile is a rail station
 */
static inline bool IsRailStationTile(Tile t)
{
	return IsTileType(t, MP_STATION) && IsRailStation(t);
}

/**
 * Is this station tile a rail waypoint?
 * @param t the tile to get the information from
 * @pre IsTileType(t, MP_STATION)
 * @return true if and only if the tile is a rail waypoint
 */
static inline bool IsRailWaypoint(Tile t)
{
	return GetStationType(t) == STATION_WAYPOINT;
}

/**
 * Is this tile a station tile and a rail waypoint?
 * @param t the tile to get the information from
 * @return true if and only if the tile is a rail waypoint
 */
static inline bool IsRailWaypointTile(Tile t)
{
	return IsTileType(t, MP_STATION) && IsRailWaypoint(t);
}

/**
 * Has this station tile a rail? In other words, is this station
 * tile a rail station or rail waypoint?
 * @param t the tile to check
 * @pre IsTileType(t, MP_STATION)
 * @return true if and only if the tile has rail
 */
static inline bool HasStationRail(Tile t)
{
	return IsRailStation(t) || IsRailWaypoint(t);
}

/**
 * Has this station tile a rail? In other words, is this station
 * tile a rail station or rail waypoint?
 * @param t the tile to check
 * @return true if and only if the tile is a station tile and has rail
 */
static inline bool HasStationTileRail(Tile t)
{
	return IsTileType(t, MP_STATION) && HasStationRail(t);
}

/**
 * Is this station tile an airport?
 * @param t the tile to get the information from
 * @pre IsTileType(t, MP_STATION)
 * @return true if and only if the tile is an airport
 */
static inline bool IsAirport(Tile t)
{
	return GetStationType(t) == STATION_AIRPORT;
}

/**
 * Is this tile a station tile and an airport tile?
 * @param t the tile to get the information from
 * @return true if and only if the tile is an airport
 */
static inline bool IsAirportTile(Tile t)
{
	return IsTileType(t, MP_STATION) && IsAirport(t);
}

bool IsHangar(Tile t);

/**
 * Is the station at \a t a truck stop?
 * @param t Tile to check
 * @pre IsTileType(t, MP_STATION)
 * @return \c true if station is a truck stop, \c false otherwise
 */
static inline bool IsTruckStop(Tile t)
{
	return GetStationType(t) == STATION_TRUCK;
}

/**
 * Is the station at \a t a bus stop?
 * @param t Tile to check
 * @pre IsTileType(t, MP_STATION)
 * @return \c true if station is a bus stop, \c false otherwise
 */
static inline bool IsBusStop(Tile t)
{
	return GetStationType(t) == STATION_BUS;
}

/**
 * Is the station at \a t a road station?
 * @param t Tile to check
 * @pre IsTileType(t, MP_STATION)
 * @return \c true if station at the tile is a bus top or a truck stop, \c false otherwise
 */
static inline bool IsRoadStop(Tile t)
{
	assert(IsTileType(t, MP_STATION));
	return IsTruckStop(t) || IsBusStop(t);
}

/**
 * Is tile \a t a road stop station?
 * @param t Tile to check
 * @return \c true if the tile is a station tile and a road stop
 */
static inline bool IsRoadStopTile(Tile t)
{
	return IsTileType(t, MP_STATION) && IsRoadStop(t);
}

/**
 * Is tile \a t a bay (non-drive through) road stop station?
 * @param t Tile to check
 * @return \c true if the tile is a station tile and a bay road stop
 */
static inline bool IsBayRoadStopTile(Tile t)
{
	return IsRoadStopTile(t) && GetStationGfx(t) < GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET;
}

/**
 * Is tile \a t a drive through road stop station?
 * @param t Tile to check
 * @return \c true if the tile is a station tile and a drive through road stop
 */
static inline bool IsDriveThroughStopTile(Tile t)
{
	return IsRoadStopTile(t) && GetStationGfx(t) >= GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET;
}

StationGfx GetTranslatedAirportTileID(StationGfx gfx);

/**
 * Get the station graphics of this airport tile
 * @param t the tile to query
 * @pre IsAirport(t)
 * @return the station graphics
 */
static inline StationGfx GetAirportGfx(Tile t)
{
	assert(IsAirport(t));
	return GetTranslatedAirportTileID(GetStationGfx(t));
}

/**
 * Gets the direction the road stop entrance points towards.
 * @param t the tile of the road stop
 * @pre IsRoadStopTile(t)
 * @return the direction of the entrance
 */
static inline DiagDirection GetRoadStopDir(Tile t)
{
	StationGfx gfx = GetStationGfx(t);
	assert(IsRoadStopTile(t));
	if (gfx < GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET) {
		return (DiagDirection)(gfx);
	} else {
		return (DiagDirection)(gfx - GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET);
	}
}

/**
 * Is tile \a t part of an oilrig?
 * @param t Tile to check
 * @pre IsTileType(t, MP_STATION)
 * @return \c true if the tile is an oilrig tile
 */
static inline bool IsOilRig(Tile t)
{
	return GetStationType(t) == STATION_OILRIG;
}

/**
 * Is tile \a t a dock tile?
 * @param t Tile to check
 * @pre IsTileType(t, MP_STATION)
 * @return \c true if the tile is a dock
 */
static inline bool IsDock(Tile t)
{
	return GetStationType(t) == STATION_DOCK;
}

/**
 * Is tile \a t a dock tile?
 * @param t Tile to check
 * @return \c true if the tile is a dock
 */
static inline bool IsDockTile(Tile t)
{
	return IsTileType(t, MP_STATION) && GetStationType(t) == STATION_DOCK;
}

/**
 * Is tile \a t a buoy tile?
 * @param t Tile to check
 * @pre IsTileType(t, MP_STATION)
 * @return \c true if the tile is a buoy
 */
static inline bool IsBuoy(Tile t)
{
	return GetStationType(t) == STATION_BUOY;
}

/**
 * Is tile \a t a buoy tile?
 * @param t Tile to check
 * @return \c true if the tile is a buoy
 */
static inline bool IsBuoyTile(Tile t)
{
	return IsTileType(t, MP_STATION) && IsBuoy(t);
}

/**
 * Is tile \a t an hangar tile?
 * @param t Tile to check
 * @return \c true if the tile is an hangar
 */
static inline bool IsHangarTile(Tile t)
{
	return IsTileType(t, MP_STATION) && IsHangar(t);
}

/**
 * Is tile \a t a blocked tile?
 * @pre HasStationRail(t)
 * @param t Tile to check
 * @return \c true if the tile is blocked
 */
static inline bool IsStationTileBlocked(Tile t)
{
	assert(HasStationRail(t));
	return HasBit(t.m6(), 0);
}

/**
 * Set the blocked state of the rail station
 * @pre HasStationRail(t)
 * @param t the station tile
 * @param b the blocked state
 */
static inline void SetStationTileBlocked(Tile t, bool b)
{
	assert(HasStationRail(t));
	SB(t.m6(), 0, 1, b ? 1 : 0);
}

/**
 * Can tile \a t have catenary wires?
 * @pre HasStationRail(t)
 * @param t Tile to check
 * @return \c true if the tile can have catenary wires
 */
static inline bool CanStationTileHaveWires(Tile t)
{
	assert(HasStationRail(t));
	return HasBit(t.m6(), 6);
}

/**
 * Set the catenary wires state of the rail station
 * @pre HasStationRail(t)
 * @param t the station tile
 * @param b the catenary wires state
 */
static inline void SetStationTileHaveWires(Tile t, bool b)
{
	assert(HasStationRail(t));
	SB(t.m6(), 6, 1, b ? 1 : 0);
}

/**
 * Can tile \a t have catenary pylons?
 * @pre HasStationRail(t)
 * @param t Tile to check
 * @return \c true if the tile can have catenary pylons
 */
static inline bool CanStationTileHavePylons(Tile t)
{
	assert(HasStationRail(t));
	return HasBit(t.m6(), 7);
}

/**
 * Set the catenary pylon state of the rail station
 * @pre HasStationRail(t)
 * @param t the station tile
 * @param b the catenary pylons state
 */
static inline void SetStationTileHavePylons(Tile t, bool b)
{
	assert(HasStationRail(t));
	SB(t.m6(), 7, 1, b ? 1 : 0);
}

/**
 * Get the rail direction of a rail station.
 * @param t Tile to query
 * @pre HasStationRail(t)
 * @return The direction of the rails on tile \a t.
 */
static inline Axis GetRailStationAxis(Tile t)
{
	assert(HasStationRail(t));
	return HasBit(GetStationGfx(t), 0) ? AXIS_Y : AXIS_X;
}

/**
 * Get the rail track of a rail station tile.
 * @param t Tile to query
 * @pre HasStationRail(t)
 * @return The rail track of the rails on tile \a t.
 */
static inline Track GetRailStationTrack(Tile t)
{
	return AxisToTrack(GetRailStationAxis(t));
}

/**
 * Get the trackbits of a rail station tile.
 * @param t Tile to query
 * @pre HasStationRail(t)
 * @return The trackbits of the rails on tile \a t.
 */
static inline TrackBits GetRailStationTrackBits(Tile t)
{
	return AxisToTrackBits(GetRailStationAxis(t));
}

/**
 * Check if a tile is a valid continuation to a railstation tile.
 * The tile \a test_tile is a valid continuation to \a station_tile, if all of the following are true:
 * \li \a test_tile is a rail station tile
 * \li the railtype of \a test_tile is compatible with the railtype of \a station_tile
 * \li the tracks on \a test_tile and \a station_tile are in the same direction
 * \li both tiles belong to the same station
 * \li \a test_tile is not blocked (@see IsStationTileBlocked)
 * @param test_tile Tile to test
 * @param station_tile Station tile to compare with
 * @pre IsRailStationTile(station_tile)
 * @return true if the two tiles are compatible
 */
static inline bool IsCompatibleTrainStationTile(Tile test_tile, Tile station_tile)
{
	assert(IsRailStationTile(station_tile));
	return IsRailStationTile(test_tile) && !IsStationTileBlocked(test_tile) &&
			IsCompatibleRail(GetRailType(test_tile), GetRailType(station_tile)) &&
			GetRailStationAxis(test_tile) == GetRailStationAxis(station_tile) &&
			GetStationIndex(test_tile) == GetStationIndex(station_tile);
}

/**
 * Get the reservation state of the rail station
 * @pre HasStationRail(t)
 * @param t the station tile
 * @return reservation state
 */
static inline bool HasStationReservation(Tile t)
{
	assert(HasStationRail(t));
	return HasBit(t.m6(), 2);
}

/**
 * Set the reservation state of the rail station
 * @pre HasStationRail(t)
 * @param t the station tile
 * @param b the reservation state
 */
static inline void SetRailStationReservation(Tile t, bool b)
{
	assert(HasStationRail(t));
	SB(t.m6(), 2, 1, b ? 1 : 0);
}

/**
 * Get the reserved track bits for a waypoint
 * @pre HasStationRail(t)
 * @param t the tile
 * @return reserved track bits
 */
static inline TrackBits GetStationReservationTrackBits(Tile t)
{
	return HasStationReservation(t) ? GetRailStationTrackBits(t) : TRACK_BIT_NONE;
}

/**
 * Get the direction of a dock.
 * @param t Tile to query
 * @pre IsDock(t)
 * @pre \a t is the land part of the dock
 * @return The direction of the dock on tile \a t.
 */
static inline DiagDirection GetDockDirection(Tile t)
{
	StationGfx gfx = GetStationGfx(t);
	assert(IsDock(t) && gfx < GFX_DOCK_BASE_WATER_PART);
	return (DiagDirection)(gfx);
}

/**
 * Check whether a dock tile is the tile on water.
 */
static inline bool IsDockWaterPart(Tile t)
{
	assert(IsDockTile(t));
	StationGfx gfx = GetStationGfx(t);
	return gfx >= GFX_DOCK_BASE_WATER_PART;
}

/**
 * Is there a custom rail station spec on this tile?
 * @param t Tile to query
 * @pre HasStationTileRail(t)
 * @return True if this station is part of a newgrf station.
 */
static inline bool IsCustomStationSpecIndex(Tile t)
{
	assert(HasStationTileRail(t));
	return t.m4() != 0;
}

/**
 * Set the custom station spec for this tile.
 * @param t Tile to set the stationspec of.
 * @param specindex The new spec.
 * @pre HasStationTileRail(t)
 */
static inline void SetCustomStationSpecIndex(Tile t, byte specindex)
{
	assert(HasStationTileRail(t));
	t.m4() = specindex;
}

/**
 * Get the custom station spec for this tile.
 * @param t Tile to query
 * @pre HasStationTileRail(t)
 * @return The custom station spec of this tile.
 */
static inline uint GetCustomStationSpecIndex(Tile t)
{
	assert(HasStationTileRail(t));
	return t.m4();
}

/**
 * Is there a custom road stop spec on this tile?
 * @param t Tile to query
 * @pre IsRoadStopTile(t)
 * @return True if this station is part of a newgrf station.
 */
static inline bool IsCustomRoadStopSpecIndex(Tile t)
{
	assert(IsRoadStopTile(t));
	return GB(t.m8(), 0, 6) != 0;
}

/**
 * Set the custom road stop spec for this tile.
 * @param t Tile to set the stationspec of.
 * @param specindex The new spec.
 * @pre IsRoadStopTile(t)
 */
static inline void SetCustomRoadStopSpecIndex(Tile t, byte specindex)
{
	assert(IsRoadStopTile(t));
	SB(t.m8(), 0, 6, specindex);
}

/**
 * Get the custom road stop spec for this tile.
 * @param t Tile to query
 * @pre IsRoadStopTile(t)
 * @return The custom station spec of this tile.
 */
static inline uint GetCustomRoadStopSpecIndex(Tile t)
{
	assert(IsRoadStopTile(t));
	return GB(t.m8(), 0, 6);
}

/**
 * Set the random bits for a station tile.
 * @param t Tile to set random bits for.
 * @param random_bits The random bits.
 * @pre IsTileType(t, MP_STATION)
 */
static inline void SetStationTileRandomBits(Tile t, byte random_bits)
{
	assert(IsTileType(t, MP_STATION));
	SB(t.m3(), 4, 4, random_bits);
}

/**
 * Get the random bits of a station tile.
 * @param t Tile to query
 * @pre IsTileType(t, MP_STATION)
 * @return The random bits for this station tile.
 */
static inline byte GetStationTileRandomBits(Tile t)
{
	assert(IsTileType(t, MP_STATION));
	return GB(t.m3(), 4, 4);
}

/**
 * Make the given tile a station tile.
 * @param t the tile to make a station tile
 * @param o the owner of the station
 * @param sid the station to which this tile belongs
 * @param st the type this station tile
 * @param section the StationGfx to be used for this tile
 * @param wc The water class of the station
 */
static inline void MakeStation(Tile t, Owner o, StationID sid, StationType st, byte section, WaterClass wc = WATER_CLASS_INVALID)
{
	SetTileType(t, MP_STATION);
	SetTileOwner(t, o);
	SetWaterClass(t, wc);
	SetDockingTile(t, false);
	t.m2() = sid;
	t.m3() = 0;
	t.m4() = 0;
	t.m5() = section;
	SB(t.m6(), 2, 1, 0);
	SB(t.m6(), 3, 3, st);
	t.m7() = 0;
	t.m8() = 0;
}

/**
 * Make the given tile a rail station tile.
 * @param t the tile to make a rail station tile
 * @param o the owner of the station
 * @param sid the station to which this tile belongs
 * @param a the axis of this tile
 * @param section the StationGfx to be used for this tile
 * @param rt the railtype of this tile
 */
static inline void MakeRailStation(Tile t, Owner o, StationID sid, Axis a, byte section, RailType rt)
{
	MakeStation(t, o, sid, STATION_RAIL, section + a);
	SetRailType(t, rt);
	SetRailStationReservation(t, false);
}

/**
 * Make the given tile a rail waypoint tile.
 * @param t the tile to make a rail waypoint
 * @param o the owner of the waypoint
 * @param sid the waypoint to which this tile belongs
 * @param a the axis of this tile
 * @param section the StationGfx to be used for this tile
 * @param rt the railtype of this tile
 */
static inline void MakeRailWaypoint(Tile t, Owner o, StationID sid, Axis a, byte section, RailType rt)
{
	MakeStation(t, o, sid, STATION_WAYPOINT, section + a);
	SetRailType(t, rt);
	SetRailStationReservation(t, false);
}

/**
 * Make the given tile a roadstop tile.
 * @param t the tile to make a roadstop
 * @param o the owner of the roadstop
 * @param sid the station to which this tile belongs
 * @param rst the type of roadstop to make this tile
 * @param road_rt the road roadtype on this tile
 * @param tram_rt the tram roadtype on this tile
 * @param d the direction of the roadstop
 */
static inline void MakeRoadStop(Tile t, Owner o, StationID sid, RoadStopType rst, RoadType road_rt, RoadType tram_rt, DiagDirection d)
{
	MakeStation(t, o, sid, (rst == ROADSTOP_BUS ? STATION_BUS : STATION_TRUCK), d);
	SetRoadTypes(t, road_rt, tram_rt);
	SetRoadOwner(t, RTT_ROAD, o);
	SetRoadOwner(t, RTT_TRAM, o);
}

/**
 * Make the given tile a drivethrough roadstop tile.
 * @param t the tile to make a roadstop
 * @param station the owner of the roadstop
 * @param road the owner of the road
 * @param tram the owner of the tram
 * @param sid the station to which this tile belongs
 * @param rst the type of roadstop to make this tile
 * @param road_rt the road roadtype on this tile
 * @param tram_rt the tram roadtype on this tile
 * @param a the direction of the roadstop
 */
static inline void MakeDriveThroughRoadStop(Tile t, Owner station, Owner road, Owner tram, StationID sid, RoadStopType rst, RoadType road_rt, RoadType tram_rt, Axis a)
{
	MakeStation(t, station, sid, (rst == ROADSTOP_BUS ? STATION_BUS : STATION_TRUCK), GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET + a);
	SetRoadTypes(t, road_rt, tram_rt);
	SetRoadOwner(t, RTT_ROAD, road);
	SetRoadOwner(t, RTT_TRAM, tram);
}

/**
 * Make the given tile an airport tile.
 * @param t the tile to make a airport
 * @param o the owner of the airport
 * @param sid the station to which this tile belongs
 * @param section the StationGfx to be used for this tile
 * @param wc the type of water on this tile
 */
static inline void MakeAirport(Tile t, Owner o, StationID sid, byte section, WaterClass wc)
{
	MakeStation(t, o, sid, STATION_AIRPORT, section, wc);
}

/**
 * Make the given tile a buoy tile.
 * @param t the tile to make a buoy
 * @param sid the station to which this tile belongs
 * @param wc the type of water on this tile
 */
static inline void MakeBuoy(Tile t, StationID sid, WaterClass wc)
{
	/* Make the owner of the buoy tile the same as the current owner of the
	 * water tile. In this way, we can reset the owner of the water to its
	 * original state when the buoy gets removed. */
	MakeStation(t, GetTileOwner(t), sid, STATION_BUOY, 0, wc);
}

/**
 * Make the given tile a dock tile.
 * @param t the tile to make a dock
 * @param o the owner of the dock
 * @param sid the station to which this tile belongs
 * @param d the direction of the dock
 * @param wc the type of water on this tile
 */
static inline void MakeDock(Tile t, Owner o, StationID sid, DiagDirection d, WaterClass wc)
{
	MakeStation(t, o, sid, STATION_DOCK, d);
	MakeStation(TileIndex(t) + TileOffsByDiagDir(d), o, sid, STATION_DOCK, GFX_DOCK_BASE_WATER_PART + DiagDirToAxis(d), wc);
}

/**
 * Make the given tile an oilrig tile.
 * @param t the tile to make an oilrig
 * @param sid the station to which this tile belongs
 * @param wc the type of water on this tile
 */
static inline void MakeOilrig(Tile t, StationID sid, WaterClass wc)
{
	MakeStation(t, OWNER_NONE, sid, STATION_OILRIG, 0, wc);
}

#endif /* STATION_MAP_H */
