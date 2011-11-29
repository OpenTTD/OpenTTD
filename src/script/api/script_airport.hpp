/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_airport.hpp Everything to query and build airports. */

#ifndef SCRIPT_AIRPORT_HPP
#define SCRIPT_AIRPORT_HPP

#include "script_object.hpp"

/**
 * Class that handles all airport related functions.
 * @api ai
 */
class ScriptAirport : public ScriptObject {
public:
	/**
	 * The types of airports available in the game.
	 */
	enum AirportType {
		/* Note: the values _are_ important as they represent an in-game value */
		AT_SMALL         =   0, ///< The small airport.
		AT_LARGE         =   1, ///< The large airport.
		AT_METROPOLITAN  =   3, ///< The metropolitan airport.
		AT_INTERNATIONAL =   4, ///< The international airport.
		AT_COMMUTER      =   5, ///< The commuter airport.
		AT_INTERCON      =   7, ///< The intercontinental airport.

		/* Next are the airports which only have helicopter platforms */
		AT_HELIPORT      =   2, ///< The heliport.
		AT_HELISTATION   =   8, ///< The helistation.
		AT_HELIDEPOT     =   6, ///< The helidepot.

		AT_INVALID       = 255, ///< Invalid airport.
	};

	/**
	 * All plane types available.
	 */
	enum PlaneType {
		/* Note: the values _are_ important as they represent an in-game value */
		PT_HELICOPTER    =   0, ///< A helicopter.
		PT_SMALL_PLANE   =   1, ///< A small plane.
		PT_BIG_PLANE     =   3, ///< A big plane.

		PT_INVALID       =  -1, ///< An invalid PlaneType
	};

	/**
	 * Checks whether the given AirportType is valid and available.
	 * @param type The AirportType to check.
	 * @return True if and only if the AirportType is valid and available.
	 * @post return value == true -> IsAirportInformationAvailable returns true.
	 */
	static bool IsValidAirportType(AirportType type);

	/**
	 * Can you get information on this airport type? As opposed to
	 * IsValidAirportType this will return also return true when
	 * an airport type is no longer buildable.
	 * @param type The AirportType to check.
	 * @return True if and only if the AirportType is valid.
	 * @post return value == false -> IsValidAirportType returns false.
	 */
	static bool IsAirportInformationAvailable(AirportType type);

	/**
	 * Get the cost to build this AirportType.
	 * @param type The AirportType to check.
	 * @pre AirportAvailable(type).
	 * @return The cost of building this AirportType.
	 */
	static Money GetPrice(AirportType type);

	/**
	 * Checks whether the given tile is actually a tile with a hangar.
	 * @param tile The tile to check.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @return True if and only if the tile has a hangar.
	 */
	static bool IsHangarTile(TileIndex tile);

	/**
	 * Checks whether the given tile is actually a tile with an airport.
	 * @param tile The tile to check.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @return True if and only if the tile has an airport.
	 */
	static bool IsAirportTile(TileIndex tile);

	/**
	 * Get the width of this type of airport.
	 * @param type The type of airport.
	 * @pre IsAirportInformationAvailable(type).
	 * @return The width in tiles.
	 */
	static int32 GetAirportWidth(AirportType type);

	/**
	 * Get the height of this type of airport.
	 * @param type The type of airport.
	 * @pre IsAirportInformationAvailable(type).
	 * @return The height in tiles.
	 */
	static int32 GetAirportHeight(AirportType type);

	/**
	 * Get the coverage radius of this type of airport.
	 * @param type The type of airport.
	 * @pre IsAirportInformationAvailable(type).
	 * @return The radius in tiles.
	 */
	static int32 GetAirportCoverageRadius(AirportType type);

	/**
	 * Get the number of hangars of the airport.
	 * @param tile Any tile of the airport.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @return The number of hangars of the airport.
	 */
	static int32 GetNumHangars(TileIndex tile);

	/**
	 * Get the first hanger tile of the airport.
	 * @param tile Any tile of the airport.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @pre GetNumHangars(tile) > 0.
	 * @return The first hanger tile of the airport.
	 * @note Possible there are more hangars, but you won't be able to find them
	 *  without walking over all the tiles of the airport and using
	 *  IsHangarTile() on them.
	 */
	static TileIndex GetHangarOfAirport(TileIndex tile);

	/**
	 * Builds a airport with tile at the topleft corner.
	 * @param tile The topleft corner of the airport.
	 * @param type The type of airport to build.
	 * @param station_id The station to join, ScriptStation::STATION_NEW or ScriptStation::STATION_JOIN_ADJACENT.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @pre AirportAvailable(type).
	 * @pre station_id == ScriptStation::STATION_NEW || station_id == ScriptStation::STATION_JOIN_ADJACENT || ScriptStation::IsValidStation(station_id).
	 * @exception ScriptError::ERR_AREA_NOT_CLEAR
	 * @exception ScriptError::ERR_FLAT_LAND_REQUIRED
	 * @exception ScriptError::ERR_LOCAL_AUTHORITY_REFUSES
	 * @exception ScriptStation::ERR_STATION_TOO_LARGE
	 * @exception ScriptStation::ERR_STATION_TOO_CLOSE_TO_ANOTHER_STATION
	 * @return Whether the airport has been/can be build or not.
	 */
	static bool BuildAirport(TileIndex tile, AirportType type, StationID station_id);

	/**
	 * Removes an airport.
	 * @param tile Any tile of the airport.
	 * @pre ScriptMap::IsValidTile(tile).
	 * @exception ScriptError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @return Whether the airport has been/can be removed or not.
	 */
	static bool RemoveAirport(TileIndex tile);

	/**
	 * Get the AirportType of an existing airport.
	 * @param tile Any tile of the airport.
	 * @pre ScriptTile::IsStationTile(tile).
	 * @pre ScriptStation::HasStationType(ScriptStation.GetStationID(tile), ScriptStation::STATION_AIRPORT).
	 * @return The AirportType of the airport.
	 */
	static AirportType GetAirportType(TileIndex tile);

	/**
	 * Get the noise that will be added to the nearest town if an airport was
	 *  built at this tile.
	 * @param tile The tile to check.
	 * @param type The AirportType to check.
	 * @pre IsAirportInformationAvailable(type).
	 * @return The amount of noise added to the nearest town.
	 * @note The noise will be added to the town with TownID GetNearestTown(tile, type).
	 */
	static int GetNoiseLevelIncrease(TileIndex tile, AirportType type);

	/**
	 * Get the TownID of the town whose local authority will influence
	 *  an airport at some tile.
	 * @param tile The tile to check.
	 * @param type The AirportType to check.
	 * @pre IsAirportInformationAvailable(type).
	 * @return The TownID of the town closest to the tile.
	 */
	static TownID GetNearestTown(TileIndex tile, AirportType type);
};

#endif /* SCRIPT_AIRPORT_HPP */
