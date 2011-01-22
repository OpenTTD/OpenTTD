/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_station.hpp Everything to query and build stations. */

#ifndef AI_STATION_HPP
#define AI_STATION_HPP

#include "ai_road.hpp"
#include "ai_basestation.hpp"

/**
 * Class that handles all station related functions.
 */
class AIStation : public AIBaseStation {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIStation"; }

	/**
	 * All station related error messages.
	 */
	enum ErrorMessages {
		/** Base for station related errors */
		ERR_STATION_BASE = AIError::ERR_CAT_STATION << AIError::ERR_CAT_BIT_SIZE,

		/** The station is build too close to another station, airport or dock */
		ERR_STATION_TOO_CLOSE_TO_ANOTHER_STATION, // [STR_ERROR_TOO_CLOSE_TO_ANOTHER_AIRPORT, STR_ERROR_TOO_CLOSE_TO_ANOTHER_STATION, STR_ERROR_TOO_CLOSE_TO_ANOTHER_DOCK]

		/** There are too many stations, airports and docks in the game */
		ERR_STATION_TOO_MANY_STATIONS,            // [STR_ERROR_TOO_MANY_STATIONS_LOADING, STR_ERROR_TOO_MANY_TRUCK_STOPS, STR_ERROR_TOO_MANY_BUS_STOPS]

		/** There are too many stations, airports of docks in a town */
		ERR_STATION_TOO_MANY_STATIONS_IN_TOWN,    // [STR_ERROR_LOCAL_AUTHORITY_REFUSES_AIRPORT]
	};

	/**
	 * Type of stations known in the game.
	 */
	enum StationType {
		/* Values are important, as they represent the internal state of the game. */
		STATION_TRAIN      = 0x01, ///< Train station
		STATION_TRUCK_STOP = 0x02, ///< Truck station
		STATION_BUS_STOP   = 0x04, ///< Bus station
		STATION_AIRPORT    = 0x08, ///< Airport
		STATION_DOCK       = 0x10, ///< Dock
		STATION_ANY        = 0x1F, ///< All station types
	};

	/**
	 * Checks whether the given station is valid and owned by you.
	 * @param station_id The station to check.
	 * @return True if and only if the station is valid.
	 */
	static bool IsValidStation(StationID station_id);

	/**
	 * Get the StationID of a tile, if there is a station.
	 * @param tile The tile to find the stationID of
	 * @return StationID of the station.
	 * @post Use IsValidStation() to see if the station is valid.
	 */
	static StationID GetStationID(TileIndex tile);

	/**
	 * See how much cargo there is waiting on a station.
	 * @param station_id The station to get the cargo-waiting of.
	 * @param cargo_id The cargo to get the cargo-waiting of.
	 * @pre IsValidStation(station_id).
	 * @pre IsValidCargo(cargo_id).
	 * @return The amount of units waiting at the station.
	 */
	static int32 GetCargoWaiting(StationID station_id, CargoID cargo_id);

	/**
	 * See how high the rating is of a cargo on a station.
	 * @param station_id The station to get the cargo-rating of.
	 * @param cargo_id The cargo to get the cargo-rating of.
	 * @pre IsValidStation(station_id).
	 * @pre IsValidCargo(cargo_id).
	 * @return The rating in percent of the cargo on the station.
	 */
	static int32 GetCargoRating(StationID station_id, CargoID cargo_id);

	/**
	 * Get the coverage radius of this type of station.
	 * @param station_type The type of station.
	 * @return The radius in tiles.
	 */
	static int32 GetCoverageRadius(AIStation::StationType station_type);

	/**
	 * Get the manhattan distance from the tile to the AIStation::GetLocation()
	 *  of the station.
	 * @param station_id The station to get the distance to.
	 * @param tile The tile to get the distance to.
	 * @pre IsValidStation(station_id).
	 * @return The distance between station and tile.
	 */
	static int32 GetDistanceManhattanToTile(StationID station_id, TileIndex tile);

	/**
	 * Get the square distance from the tile to the AIStation::GetLocation()
	 *  of the station.
	 * @param station_id The station to get the distance to.
	 * @param tile The tile to get the distance to.
	 * @pre IsValidStation(station_id).
	 * @return The distance between station and tile.
	 */
	static int32 GetDistanceSquareToTile(StationID station_id, TileIndex tile);

	/**
	 * Find out if this station is within the rating influence of a town.
	 *  Stations within the radius influence the rating of the town.
	 * @param station_id The station to check.
	 * @param town_id The town to check.
	 * @return True if the tile is within the rating influence of the town.
	 */
	static bool IsWithinTownInfluence(StationID station_id, TownID town_id);

	/**
	 * Check if any part of the station contains a station of the type
	 *  StationType
	 * @param station_id The station to look at.
	 * @param station_type The StationType to look for.
	 * @return True if the station has a station part of the type StationType.
	 */
	static bool HasStationType(StationID station_id, StationType station_type);

	/**
	 * Check if any part of the station contains a station of the type
	 *  RoadType.
	 * @param station_id The station to look at.
	 * @param road_type The RoadType to look for.
	 * @return True if the station has a station part of the type RoadType.
	 */
	static bool HasRoadType(StationID station_id, AIRoad::RoadType road_type);

	/**
	 * Get the town that was nearest to the given station when the station was built.
	 * @param station_id The station to look at.
	 * @return The TownID of the town whose center tile was closest to the station
	 *  at the time the station was built.
	 * @note There is no guarantee that the station is even near the returned town
	 *  nor that the returns town is closest to the station now. A station that was
	 *  'walked' to the other end of the map will still return the same town. Also,
	 *  towns grow, towns change. So don't depend on this value too much.
	 */
	static TownID GetNearestTown(StationID station_id);
};

DECLARE_ENUM_AS_BIT_SET(AIStation::StationType)

#endif /* AI_STATION_HPP */
