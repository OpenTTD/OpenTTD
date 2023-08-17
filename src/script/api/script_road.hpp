/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_road.hpp Everything to query and build roads. */

#ifndef SCRIPT_ROAD_HPP
#define SCRIPT_ROAD_HPP

#include "script_tile.hpp"
#include "../squirrel_helper_type.hpp"
#include "../../../road.h"

/**
 * Class that handles all road related functions.
 * @api ai game
 */
class ScriptRoad : public ScriptObject {
public:
	/**
	 * All road related error messages.
	 */
	enum ErrorMessages {
		/** Base for road building / maintaining errors */
		ERR_ROAD_BASE = ScriptError::ERR_CAT_ROAD << ScriptError::ERR_CAT_BIT_SIZE,

		/** Road works are in progress */
		ERR_ROAD_WORKS_IN_PROGRESS,                   // [STR_ERROR_ROAD_WORKS_IN_PROGRESS]

		/** Drive through is in the wrong direction */
		ERR_ROAD_DRIVE_THROUGH_WRONG_DIRECTION,       // [STR_ERROR_DRIVE_THROUGH_DIRECTION]

		/** Drive through roads can't be build on town owned roads */
		ERR_ROAD_CANNOT_BUILD_ON_TOWN_ROAD,           // [STR_ERROR_DRIVE_THROUGH_ON_TOWN_ROAD]

		/** One way roads can't have junctions */
		ERR_ROAD_ONE_WAY_ROADS_CANNOT_HAVE_JUNCTIONS, // [STR_ERROR_ONEWAY_ROADS_CAN_T_HAVE_JUNCTION]

		/** This roadtype cannot have crossings */
		ERR_ROADTYPE_DISALLOWS_CROSSING,              // [STR_ERROR_CROSSING_DISALLOWED_ROAD]

		/** No suitable road could be found */
		ERR_UNSUITABLE_ROAD,                          // [STR_ERROR_NO_SUITABLE_ROAD, STR_ERROR_NO_SUITABLE_TRAMWAY]
	};

	/**
	 * Types of road known to the game.
	 */
	enum RoadType {
		/* Note: these values represent part of the in-game static values */
		ROADTYPE_ROAD    = ::ROADTYPE_ROAD,    ///< Build road objects.
		ROADTYPE_TRAM    = ::ROADTYPE_TRAM,    ///< Build tram objects.

		/* Custom added value, only valid for this API */
		ROADTYPE_INVALID = -1,                 ///< Invalid RoadType.
	};

	/**
	 * Road/tram types
	 */
	enum RoadTramTypes : uint8_t {
		ROADTRAMTYPES_ROAD = ::RTTB_ROAD, ///< Road road types.
		ROADTRAMTYPES_TRAM = ::RTTB_TRAM, ///< Tram road types.
	};

	/**
	 * Type of road station.
	 */
	enum RoadVehicleType {
		ROADVEHTYPE_BUS,   ///< Build objects useable for busses and passenger trams
		ROADVEHTYPE_TRUCK, ///< Build objects useable for trucks and cargo trams
	};

	/**
	 * Types of road-related objects in the game.
	 */
	enum BuildType {
		BT_ROAD,       ///< Build a piece of road
		BT_DEPOT,      ///< Build a road depot
		BT_BUS_STOP,   ///< Build a bus stop
		BT_TRUCK_STOP, ///< Build a truck stop
	};

	/**
	 * Get the name of a road type.
	 * @param road_type The road type to get the name of.
	 * @pre IsRoadTypeAvailable(road_type).
	 * @return The name the road type has.
	 */
	static std::optional<std::string> GetName(RoadType road_type);

	/**
	 * Determines whether a busstop or a truckstop is needed to transport a certain cargo.
	 * @param cargo_type The cargo to test.
	 * @pre ScriptCargo::IsValidCargo(cargo_type).
	 * @return The road vehicle type needed to transport the cargo.
	 */
	static RoadVehicleType GetRoadVehicleTypeForCargo(CargoID cargo_type);

	/**
	 * Checks whether the given tile is actually a tile with road that can be
	 *  used to traverse a tile. This excludes road depots and 'normal' road
	 *  stations, but includes drive through stations.
	 * @param tile The tile to check.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @return True if and only if the tile has road.
	 */
	static bool IsRoadTile(TileIndex tile);

	/**
	 * Checks whether the given tile is actually a tile with a road depot.
	 * @param tile The tile to check.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @pre IsRoadTypeAvailable(GetCurrentRoadType()).
	 * @return True if and only if the tile has a road depot.
	 */
	static bool IsRoadDepotTile(TileIndex tile);

	/**
	 * Checks whether the given tile is actually a tile with a road station.
	 * @param tile The tile to check.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @pre IsRoadTypeAvailable(GetCurrentRoadType()).
	 * @return True if and only if the tile has a road station.
	 */
	static bool IsRoadStationTile(TileIndex tile);

	/**
	 * Checks whether the given tile is actually a tile with a drive through
	 *  road station.
	 * @param tile The tile to check.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @pre IsRoadTypeAvailable(GetCurrentRoadType()).
	 * @return True if and only if the tile has a drive through road station.
	 */
	static bool IsDriveThroughRoadStationTile(TileIndex tile);

	/**
	 * Check if a given RoadType is available.
	 * @param road_type The RoadType to check for.
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return True if this RoadType can be used.
	 */
	static bool IsRoadTypeAvailable(RoadType road_type);

	/**
	 * Get the current RoadType set for all ScriptRoad functions.
	 * @return The RoadType currently set.
	 */
	static RoadType GetCurrentRoadType();

	/**
	 * Set the RoadType for all further ScriptRoad functions.
	 * @param road_type The RoadType to set.
	 */
	static void SetCurrentRoadType(RoadType road_type);

	/**
	 * Check if a road vehicle built for a road type can run on another road type.
	 * @param engine_road_type The road type the road vehicle is built for.
	 * @param road_road_type The road type you want to check.
	 * @pre ScriptRoad::IsRoadTypeAvailable(engine_road_type).
	 * @pre ScriptRoad::IsRoadTypeAvailable(road_road_type).
	 * @return Whether a road vehicle built for 'engine_road_type' can run on 'road_road_type'.
	 */
	static bool RoadVehCanRunOnRoad(ScriptRoad::RoadType engine_road_type, ScriptRoad::RoadType road_road_type);

	/**
	 * Check if a road vehicle built for a road type has power on another road type.
	 * @param engine_road_type The road type the road vehicle is built for.
	 * @param road_road_type The road type you want to check.
	 * @pre ScriptRoad::IsRoadTypeAvailable(engine_road_type).
	 * @pre ScriptRoad::IsRoadTypeAvailable(road_road_type).
	 * @return Whether a road vehicle built for 'engine_road_type' has power on 'road_road_type'.
	 */
	static bool RoadVehHasPowerOnRoad(ScriptRoad::RoadType engine_road_type, ScriptRoad::RoadType road_road_type);


	/**
	 * Convert the road on all tiles within a rectangle to another RoadType.
	 * @param start_tile One corner of the rectangle.
	 * @param end_tile The opposite corner of the rectangle.
	 * @param road_type The RoadType you want to convert.
	 * @pre ScriptMap::IsValidTile(start_tile).
	 * @pre ScriptMap::IsValidTile(end_tile).
	 * @pre IsRoadTypeAvailable(road_type).
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @exception ScriptRoad::ERR_UNSUITABLE_ROAD
	 * @return Whether at least some road has been converted successfully.
	 */
	static bool ConvertRoadType(TileIndex start_tile, TileIndex end_tile, RoadType road_type);

	/**
	 * Check if a given tile has RoadType.
	 * @param tile The tile to check.
	 * @param road_type The RoadType to check for.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @pre IsRoadTypeAvailable(road_type).
	 * @return True if the tile contains a RoadType object.
	 */
	static bool HasRoadType(TileIndex tile, RoadType road_type);

	/**
	 * Checks whether the given tiles are directly connected, i.e. whether
	 *  a road vehicle can travel from the center of the first tile to the
	 * center of the second tile.
	 * @param tile_from The source tile.
	 * @param tile_to The destination tile.
	 * @pre IsRoadTypeAvailable(GetCurrentRoadType()).
	 * @pre ScriptMap::IsValidTile(tile_from).
	 * @pre ScriptMap::IsValidTile(tile_to).
	 * @pre 'tile_from' and 'tile_to' are directly neighbouring tiles.
	 * @return True if and only if a road vehicle can go from tile_from to tile_to.
	 */
	static bool AreRoadTilesConnected(TileIndex tile_from, TileIndex tile_to);

	/**
	 * Lookup function for building road parts independent of whether the
	 *  "building on slopes" setting is enabled or not.
	 *  This implementation can be used for abstract reasoning about a tile as
	 *  it needs the slope and existing road parts of the tile as information.
	 * @param slope The slope of the tile to examine.
	 * @param existing An array with the existing neighbours in the same format
	 *                 as "start" and "end", e.g. ScriptMap.GetTileIndex(0, 1).
	 *                 As a result of this all values of the existing array
	 *                 must be of type integer.
	 * @param start The tile from where the 'tile to be considered' will be
	 *              entered. This is a relative tile, so valid parameters are:
	 *              ScriptMap.GetTileIndex(0, 1), ScriptMap.GetTileIndex(0, -1),
	 *              ScriptMap.GetTileIndex(1, 0) and ScriptMap.GetTileIndex(-1, 0).
	 * @param end The tile from where the 'tile to be considered' will be
	 *            exited. This is a relative tile, sovalid parameters are:
	 *              ScriptMap.GetTileIndex(0, 1), ScriptMap.GetTileIndex(0, -1),
	 *              ScriptMap.GetTileIndex(1, 0) and ScriptMap.GetTileIndex(-1, 0).
	 * @pre start != end.
	 * @pre slope must be a valid slope, i.e. one specified in ScriptTile::Slope.
	 * @note Passing data that would be invalid in-game, e.g. existing containing
	 *       road parts that can not be build on a tile with the given slope,
	 *       does not necessarily means that -1 is returned, i.e. not all
	 *       preconditions written here or assumed by the game are extensively
	 *       checked to make sure the data entered is valid.
	 * @return 0 when the build parts do not connect, 1 when they do connect once
	 *         they are build or 2 when building the first part automatically
	 *         builds the second part. -1 means the preconditions are not met.
	 */
	static SQInteger CanBuildConnectedRoadParts(ScriptTile::Slope slope, Array<> &&existing, TileIndex start, TileIndex end);

	/**
	 * Lookup function for building road parts independent of whether the
	 *  "building on slopes" setting is enabled or not.
	 *  This implementation can be used for reasoning about an existing tile.
	 * @param tile The tile to examine.
	 * @param start The tile from where "tile" will be entered.
	 * @param end The tile from where "tile" will be exited.
	 * @pre start != end.
	 * @pre tile != start.
	 * @pre tile != end.
	 * @pre ScriptMap.IsValidTile(tile).
	 * @pre ScriptMap.IsValidTile(start).
	 * @pre ScriptMap.IsValidTile(end).
	 * @pre ScriptMap.GetDistanceManhattanToTile(tile, start) == 1.
	 * @pre ScriptMap.GetDistanceManhattanToTile(tile, end) == 1.
	 * @return 0 when the build parts do not connect, 1 when they do connect once
	 *         they are build or 2 when building the first part automatically
	 *         builds the second part. -1 means the preconditions are not met.
	 */
	static SQInteger CanBuildConnectedRoadPartsHere(TileIndex tile, TileIndex start, TileIndex end);

	/**
	 * Count how many neighbours are road.
	 * @param tile The tile to check on.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @pre IsRoadTypeAvailable(GetCurrentRoadType()).
	 * @return 0 means no neighbour road; max value is 4.
	 */
	static SQInteger GetNeighbourRoadCount(TileIndex tile);

	/**
	 * Gets the tile in front of a road depot.
	 * @param depot The road depot tile.
	 * @pre IsRoadDepotTile(depot).
	 * @return The tile in front of the depot.
	 */
	static TileIndex GetRoadDepotFrontTile(TileIndex depot);

	/**
	 * Gets the tile in front of a road station.
	 * @param station The road station tile.
	 * @pre IsRoadStationTile(station).
	 * @return The tile in front of the road station.
	 */
	static TileIndex GetRoadStationFrontTile(TileIndex station);

	/**
	 * Gets the tile at the back of a drive through road station.
	 *  So, one side of the drive through station is retrieved with
	 *  GetTileInFrontOfStation, the other with this function.
	 * @param station The road station tile.
	 * @pre IsDriveThroughRoadStationTile(station).
	 * @return The tile at the back of the drive through road station.
	 */
	static TileIndex GetDriveThroughBackTile(TileIndex station);

	/**
	 * Builds a road from the center of tile start to the center of tile end.
	 * @param start The start tile of the road.
	 * @param end The end tile of the road.
	 * @pre 'start' is not equal to 'end'.
	 * @pre ScriptMap::IsValidTile(start).
	 * @pre ScriptMap::IsValidTile(end).
	 * @pre 'start' and 'end' are in a straight line, i.e.
	 *  ScriptMap::GetTileX(start) == ScriptMap::GetTileX(end) or
	 *  ScriptMap::GetTileY(start) == ScriptMap::GetTileY(end).
	 * @pre IsRoadTypeAvailable(GetCurrentRoadType()).
	 * @exception ScriptError::ERR_ALREADY_BUILT
	 * @exception ScriptError::ERR_LAND_SLOPED_WRONG
	 * @exception ScriptError::ERR_AREA_NOT_CLEAR
	 * @exception ScriptRoad::ERR_ROAD_ONE_WAY_ROADS_CANNOT_HAVE_JUNCTIONS
	 * @exception ScriptRoad::ERR_ROAD_WORKS_IN_PROGRESS
	 * @exception ScriptError::ERR_VEHICLE_IN_THE_WAY
	 * @note Construction will fail if an obstacle is found between the start and end tiles.
	 * @game @note Building a piece of road as deity (ScriptCompanyMode::IsDeity()) results in a piece of road owned by towns.
	 * @return Whether the road has been/can be build or not.
	 */
	static bool BuildRoad(TileIndex start, TileIndex end);

	/**
	 * Builds a one-way road from the center of tile start to the center
	 *  of tile end. If the road already exists, it is made one-way road.
	 *  If the road already exists and is already one-way in this direction,
	 *  the road is made two-way again. If the road already exists but is
	 *  one-way in the other direction, it's made a 'no'-way road (it's
	 *  forbidden to enter the tile from any direction).
	 * @param start The start tile of the road.
	 * @param end The end tile of the road.
	 * @pre 'start' is not equal to 'end'.
	 * @pre ScriptMap::IsValidTile(start).
	 * @pre ScriptMap::IsValidTile(end).
	 * @pre 'start' and 'end' are in a straight line, i.e.
	 *  ScriptMap::GetTileX(start) == ScriptMap::GetTileX(end) or
	 *  ScriptMap::GetTileY(start) == ScriptMap::GetTileY(end).
	 * @pre GetCurrentRoadType() == ROADTYPE_ROAD.
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @exception ScriptError::ERR_ALREADY_BUILT
	 * @exception ScriptError::ERR_LAND_SLOPED_WRONG
	 * @exception ScriptError::ERR_AREA_NOT_CLEAR
	 * @exception ScriptRoad::ERR_ROAD_ONE_WAY_ROADS_CANNOT_HAVE_JUNCTIONS
	 * @exception ScriptRoad::ERR_ROAD_WORKS_IN_PROGRESS
	 * @exception ScriptError::ERR_VEHICLE_IN_THE_WAY
	 * @note Construction will fail if an obstacle is found between the start and end tiles.
	 * @return Whether the road has been/can be build or not.
	 */
	static bool BuildOneWayRoad(TileIndex start, TileIndex end);

	/**
	 * Builds a road from the edge of tile start to the edge of tile end (both
	 *  included).
	 * @param start The start tile of the road.
	 * @param end The end tile of the road.
	 * @pre 'start' is not equal to 'end'.
	 * @pre ScriptMap::IsValidTile(start).
	 * @pre ScriptMap::IsValidTile(end).
	 * @pre 'start' and 'end' are in a straight line, i.e.
	 *  ScriptMap::GetTileX(start) == ScriptMap::GetTileX(end) or
	 *  ScriptMap::GetTileY(start) == ScriptMap::GetTileY(end).
	 * @pre IsRoadTypeAvailable(GetCurrentRoadType()).
	 * @exception ScriptError::ERR_ALREADY_BUILT
	 * @exception ScriptError::ERR_LAND_SLOPED_WRONG
	 * @exception ScriptError::ERR_AREA_NOT_CLEAR
	 * @exception ScriptRoad::ERR_ROAD_ONE_WAY_ROADS_CANNOT_HAVE_JUNCTIONS
	 * @exception ScriptRoad::ERR_ROAD_WORKS_IN_PROGRESS
	 * @exception ScriptError::ERR_VEHICLE_IN_THE_WAY
	 * @note Construction will fail if an obstacle is found between the start and end tiles.
	 * @game @note Building a piece of road as deity (ScriptCompanyMode::IsDeity()) results in a piece of road owned by towns.
	 * @return Whether the road has been/can be build or not.
	 */
	static bool BuildRoadFull(TileIndex start, TileIndex end);

	/**
	 * Builds a one-way road from the edge of tile start to the edge of tile end
	 *  (both included). If the road already exists, it is made one-way road.
	 *  If the road already exists and is already one-way in this direction,
	 *  the road is made two-way again. If the road already exists but is
	 *  one-way in the other direction, it's made a 'no'-way road (it's
	 *  forbidden to enter the tile from any direction).
	 * @param start The start tile of the road.
	 * @param end The end tile of the road.
	 * @pre 'start' is not equal to 'end'.
	 * @pre ScriptMap::IsValidTile(start).
	 * @pre ScriptMap::IsValidTile(end).
	 * @pre 'start' and 'end' are in a straight line, i.e.
	 *  ScriptMap::GetTileX(start) == ScriptMap::GetTileX(end) or
	 *  ScriptMap::GetTileY(start) == ScriptMap::GetTileY(end).
	 * @pre GetCurrentRoadType() == ROADTYPE_ROAD.
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @exception ScriptError::ERR_ALREADY_BUILT
	 * @exception ScriptError::ERR_LAND_SLOPED_WRONG
	 * @exception ScriptError::ERR_AREA_NOT_CLEAR
	 * @exception ScriptRoad::ERR_ROAD_ONE_WAY_ROADS_CANNOT_HAVE_JUNCTIONS
	 * @exception ScriptRoad::ERR_ROAD_WORKS_IN_PROGRESS
	 * @exception ScriptError::ERR_VEHICLE_IN_THE_WAY
	 * @note Construction will fail if an obstacle is found between the start and end tiles.
	 * @return Whether the road has been/can be build or not.
	 */
	static bool BuildOneWayRoadFull(TileIndex start, TileIndex end);

	/**
	 * Builds a road depot.
	 * @param tile Place to build the depot.
	 * @param front The tile exactly in front of the depot.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @pre ScriptMap::IsValidTile(front).
	 * @pre 'tile' is not equal to 'front', but in a straight line of it.
	 * @pre IsRoadTypeAvailable(GetCurrentRoadType()).
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @exception ScriptError::ERR_FLAT_LAND_REQUIRED
	 * @exception ScriptError::ERR_AREA_NOT_CLEAR
	 * @return Whether the road depot has been/can be build or not.
	 */
	static bool BuildRoadDepot(TileIndex tile, TileIndex front);

	/**
	 * Builds a road bus or truck station.
	 * @param tile Place to build the station.
	 * @param front The tile exactly in front of the station.
	 * @param road_veh_type Whether to build a truck or bus station.
	 * @param station_id The station to join, ScriptStation::STATION_NEW or ScriptStation::STATION_JOIN_ADJACENT.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @pre ScriptMap::IsValidTile(front).
	 * @pre 'tile' is not equal to 'front', but in a straight line of it.
	 * @pre station_id == ScriptStation::STATION_NEW || station_id == ScriptStation::STATION_JOIN_ADJACENT || ScriptStation::IsValidStation(station_id).
	 * @pre GetCurrentRoadType() == ROADTYPE_ROAD.
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception ScriptError::ERR_AREA_NOT_CLEAR
	 * @exception ScriptError::ERR_FLAT_LAND_REQUIRED
	 * @exception ScriptRoad::ERR_ROAD_DRIVE_THROUGH_WRONG_DIRECTION
	 * @exception ScriptRoad::ERR_ROAD_CANNOT_BUILD_ON_TOWN_ROAD
	 * @exception ScriptError::ERR_VEHICLE_IN_THE_WAY
	 * @exception ScriptStation::ERR_STATION_TOO_CLOSE_TO_ANOTHER_STATION
	 * @exception ScriptStation::ERR_STATION_TOO_MANY_STATIONS
	 * @exception ScriptStation::ERR_STATION_TOO_MANY_STATIONS_IN_TOWN
	 * @return Whether the station has been/can be build or not.
	 */
	static bool BuildRoadStation(TileIndex tile, TileIndex front, RoadVehicleType road_veh_type, StationID station_id);

	/**
	 * Builds a drive-through road bus or truck station.
	 * @param tile Place to build the station.
	 * @param front A tile on the same axis with 'tile' as the station shall be oriented.
	 * @param road_veh_type Whether to build a truck or bus station.
	 * @param station_id The station to join, ScriptStation::STATION_NEW or ScriptStation::STATION_JOIN_ADJACENT.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @pre ScriptMap::IsValidTile(front).
	 * @pre 'tile' is not equal to 'front', but in a straight line of it.
	 * @pre station_id == ScriptStation::STATION_NEW || station_id == ScriptStation::STATION_JOIN_ADJACENT || ScriptStation::IsValidStation(station_id).
	 * @pre IsRoadTypeAvailable(GetCurrentRoadType()).
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception ScriptError::ERR_AREA_NOT_CLEAR
	 * @exception ScriptError::ERR_FLAT_LAND_REQUIRED
	 * @exception ScriptRoad::ERR_ROAD_DRIVE_THROUGH_WRONG_DIRECTION
	 * @exception ScriptRoad::ERR_ROAD_CANNOT_BUILD_ON_TOWN_ROAD
	 * @exception ScriptError::ERR_VEHICLE_IN_THE_WAY
	 * @exception ScriptStation::ERR_STATION_TOO_CLOSE_TO_ANOTHER_STATION
	 * @exception ScriptStation::ERR_STATION_TOO_MANY_STATIONS
	 * @exception ScriptStation::ERR_STATION_TOO_MANY_STATIONS_IN_TOWN
	 * @return Whether the station has been/can be build or not.
	 */
	static bool BuildDriveThroughRoadStation(TileIndex tile, TileIndex front, RoadVehicleType road_veh_type, StationID station_id);

	/**
	 * Removes a road from the center of tile start to the center of tile end.
	 * @param start The start tile of the road.
	 * @param end The end tile of the road.
	 * @pre 'start' is not equal to 'end'.
	 * @pre ScriptMap::IsValidTile(start).
	 * @pre ScriptMap::IsValidTile(end).
	 * @pre 'start' and 'end' are in a straight line, i.e.
	 *  ScriptMap::GetTileX(start) == ScriptMap::GetTileX(end) or
	 *  ScriptMap::GetTileY(start) == ScriptMap::GetTileY(end).
	 * @pre IsRoadTypeAvailable(GetCurrentRoadType()).
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception ScriptError::ERR_VEHICLE_IN_THE_WAY
	 * @exception ScriptRoad::ERR_ROAD_WORKS_IN_PROGRESS
	 * @return Whether the road has been/can be removed or not.
	 */
	static bool RemoveRoad(TileIndex start, TileIndex end);

	/**
	 * Removes a road from the edge of tile start to the edge of tile end (both
	 *  included).
	 * @param start The start tile of the road.
	 * @param end The end tile of the road.
	 * @pre 'start' is not equal to 'end'.
	 * @pre ScriptMap::IsValidTile(start).
	 * @pre ScriptMap::IsValidTile(end).
	 * @pre 'start' and 'end' are in a straight line, i.e.
	 *  ScriptMap::GetTileX(start) == ScriptMap::GetTileX(end) or
	 *  ScriptMap::GetTileY(start) == ScriptMap::GetTileY(end).
	 * @pre IsRoadTypeAvailable(GetCurrentRoadType()).
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception ScriptError::ERR_VEHICLE_IN_THE_WAY
	 * @exception ScriptRoad::ERR_ROAD_WORKS_IN_PROGRESS
	 * @return Whether the road has been/can be removed or not.
	 */
	static bool RemoveRoadFull(TileIndex start, TileIndex end);

	/**
	 * Removes a road depot.
	 * @param tile Place to remove the depot from.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @pre Tile is a road depot.
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception ScriptError::ERR_VEHICLE_IN_THE_WAY
	 * @return Whether the road depot has been/can be removed or not.
	 */
	static bool RemoveRoadDepot(TileIndex tile);

	/**
	 * Removes a road bus or truck station.
	 * @param tile Place to remove the station from.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @pre Tile is a road station.
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception ScriptError::ERR_VEHICLE_IN_THE_WAY
	 * @return Whether the station has been/can be removed or not.
	 */
	static bool RemoveRoadStation(TileIndex tile);

	/**
	 * Get the baseprice of building a road-related object.
	 * @param roadtype the roadtype of the object to build
	 * @param build_type the type of object to build
	 * @pre IsRoadTypeAvailable(roadtype)
	 * @return The baseprice of building the given object.
	 */
	static Money GetBuildCost(RoadType roadtype, BuildType build_type);

	/**
	 * Test if a road type is for road or trams.
	 * @param roadtype the roadtype to test.
	 * @return RoadTramTypes of the road types.
	 */
	static RoadTramTypes GetRoadTramType(RoadType roadtype);

	/**
	 * Get the maximum speed of road vehicles running on this roadtype.
	 * @param road_type The roadtype to get the maximum speed of.
	 * @pre IsRoadTypeAvailable(road_type)
	 * @return The maximum speed road vehicles can run on this roadtype
	 *   or 0 if there is no limit.
	 * @note The speed is in OpenTTD's internal speed unit.
	 *       This is mph / 0.8, which is roughly 0.5 km/h.
	 *       To get km/h multiply this number by 2.01168.
	 */
	static SQInteger GetMaxSpeed(RoadType road_type);

	/**
	 * Get the maintenance cost factor of a road type.
	 * @param roadtype The road type to get the maintenance factor of.
	 * @pre IsRoadTypeAvailable(roadtype)
	 * @return Maintenance cost factor of the roadtype.
	 */
	static SQInteger GetMaintenanceCostFactor(RoadType roadtype);

private:

	/**
	 * Internal function used by Build(OneWay)Road(Full).
	 */
	static bool _BuildRoadInternal(TileIndex start, TileIndex end, bool one_way, bool full);

	/**
	 * Internal function used by Build(DriveThrough)RoadStation.
	 */
	static bool _BuildRoadStationInternal(TileIndex tile, TileIndex front, RoadVehicleType road_veh_type, bool drive_through, StationID station_id);
};

#endif /* SCRIPT_ROAD_HPP */
