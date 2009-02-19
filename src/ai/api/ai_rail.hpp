/* $Id$ */

/** @file ai_rail.hpp Everything to query and build rails. */

#ifndef AI_RAIL_HPP
#define AI_RAIL_HPP

#include "ai_object.hpp"
#include "ai_error.hpp"
#include "ai_tile.hpp"

/**
 * Class that handles all rail related functions.
 */
class AIRail : public AIObject {
public:
	static const char *GetClassName() { return "AIRail"; }

	/**
	 * All rail related error messages.
	 */
	enum ErrorMessages {
		/** Base for rail building / maintaining errors */
		ERR_RAIL_BASE = AIError::ERR_CAT_RAIL << AIError::ERR_CAT_BIT_SIZE,

		/** One-way roads cannot have crossings */
		ERR_CROSSING_ON_ONEWAY_ROAD,       // [STR_ERR_CROSSING_ON_ONEWAY_ROAD]

		/** Track not suitable for signals */
		ERR_UNSUITABLE_TRACK,              // [STR_1005_NO_SUITABLE_RAILROAD_TRACK]

		/** Non-uniform stations is diabled */
		ERR_NONUNIFORM_STATIONS_DISABLED,  // [STR_NONUNIFORM_STATIONS_DISALLOWED]
	};

	/**
	 * Types of rail known to the game.
	 */
	enum RailType {
		/* Note: the values _are_ important as they represent an in-game value */
		RAILTYPE_INVALID  = 0xFF, //!< Invalid RailType.
	};

	/**
	 * A bitmap with all possible rail tracks on a tile.
	 */
	enum RailTrack {
		/* Note: the values _are_ important as they represent an in-game value */
		RAILTRACK_NE_SW   = 1 << 0, //!< Track along the x-axis (north-east to south-west).
		RAILTRACK_NW_SE   = 1 << 1, //!< Track along the y-axis (north-west to south-east).
		RAILTRACK_NW_NE   = 1 << 2, //!< Track in the upper corner of the tile (north).
		RAILTRACK_SW_SE   = 1 << 3, //!< Track in the lower corner of the tile (south).
		RAILTRACK_NW_SW   = 1 << 4, //!< Track in the left corner of the tile (west).
		RAILTRACK_NE_SE   = 1 << 5, //!< Track in the right corner of the tile (east).
		RAILTRACK_INVALID = 0xFF,   //!< Flag for an invalid track.
	};

	/**
	 * Types of signal known to the game.
	 */
	enum SignalType {
		/* Note: the values _are_ important as they represent an in-game value */
		SIGNALTYPE_NORMAL        = 0, //!< Normal signal.
		SIGNALTYPE_ENTRY         = 1, //!< Entry presignal.
		SIGNALTYPE_EXIT          = 2, //!< Exit signal.
		SIGNALTYPE_COMBO         = 3, //!< Combo signal.
		SIGNALTYPE_PBS           = 4, //!< Normal PBS signal.
		SIGNALTYPE_PBS_ONEWAY    = 5, //!< No-entry PBS signal.
		SIGNALTYPE_TWOWAY        = 8, //!< Bit mask for twoway signal.
		SIGNALTYPE_NORMAL_TWOWAY = SIGNALTYPE_NORMAL | SIGNALTYPE_TWOWAY, //!< Normal twoway signal.
		SIGNALTYPE_ENTRY_TWOWAY  = SIGNALTYPE_ENTRY | SIGNALTYPE_TWOWAY,  //!< Entry twoway signal.
		SIGNALTYPE_EXIT_TWOWAY   = SIGNALTYPE_EXIT | SIGNALTYPE_TWOWAY,   //!< Exit twoway signal.
		SIGNALTYPE_COMBO_TWOWAY  = SIGNALTYPE_COMBO | SIGNALTYPE_TWOWAY,  //!< Combo twoway signal.
		SIGNALTYPE_NONE          = 0xFF, //!< No signal.
	};

	/**
	 * Checks whether the given tile is actually a tile with rail that can be
	 *  used to traverse a tile. This excludes rail depots but includes
	 *  stations and waypoints.
	 * @param tile The tile to check.
	 * @pre AIMap::IsValidTile(tile).
	 * @return True if and only if the tile has rail.
	 */
	static bool IsRailTile(TileIndex tile);

	/**
	 * Checks whether there is a road / rail crossing on a tile.
	 * @param tile The tile to check.
	 * @return True if and only if there is a road / rail crossing.
	 */
	static bool IsLevelCrossingTile(TileIndex tile);

	/**
	 * Checks whether the given tile is actually a tile with a rail depot.
	 * @param tile The tile to check.
	 * @pre AIMap::IsValidTile(tile).
	 * @return True if and only if the tile has a rail depot.
	 */
	static bool IsRailDepotTile(TileIndex tile);

	/**
	 * Checks whether the given tile is actually a tile with a rail station.
	 * @param tile The tile to check.
	 * @pre AIMap::IsValidTile(tile).
	 * @return True if and only if the tile has a rail station.
	 */
	static bool IsRailStationTile(TileIndex tile);

	/**
	 * Checks whether the given tile is actually a tile with a rail waypoint.
	 * @param tile The tile to check.
	 * @pre AIMap::IsValidTile(tile).
	 * @return True if and only if the tile has a rail waypoint.
	 */
	static bool IsRailWaypointTile(TileIndex tile);

	/**
	 * Check if a given RailType is available.
	 * @param rail_type The RailType to check for.
	 * @return True if this RailType can be used.
	 */
	static bool IsRailTypeAvailable(RailType rail_type);

	/**
	 * Get the current RailType set for all AIRail functions.
	 * @return The RailType currently set.
	 */
	static RailType GetCurrentRailType();

	/**
	 * Set the RailType for all further AIRail functions.
	 * @param rail_type The RailType to set.
	 */
	static void SetCurrentRailType(RailType rail_type);

	/**
	 * Check if a train build for a rail type can run on another rail type.
	 * @param engine_rail_type The rail type the train is build for.
	 * @param track_rail_type The type you want to check.
	 * @pre AIRail::IsRailTypeAvailable(engine_rail_type).
	 * @pre AIRail::IsRailTypeAvailable(track_rail_type).
	 * @return Whether a train build for 'engine_rail_type' can run on 'track_rail_type'.
	 * @note Even if a train can run on a RailType that doesn't mean that it'll be
	 *   able to power the train. Use TrainHasPowerOnRail for that.
	 */
	static bool TrainCanRunOnRail(AIRail::RailType engine_rail_type, AIRail::RailType track_rail_type);

	/**
	 * Check if a train build for a rail type has power on another rail type.
	 * @param engine_rail_type The rail type the train is build for.
	 * @param track_rail_type The type you want to check.
	 * @pre AIRail::IsRailTypeAvailable(engine_rail_type).
	 * @pre AIRail::IsRailTypeAvailable(track_rail_type).
	 * @return Whether a train build for 'engine_rail_type' has power on 'track_rail_type'.
	 */
	static bool TrainHasPowerOnRail(AIRail::RailType engine_rail_type, AIRail::RailType track_rail_type);

	/**
	 * Get the RailType that is used on a tile.
	 * @param tile The tile to check.
	 * @pre AITile::HasTransportType(tile, AITile.TRANSPORT_RAIL).
	 * @return The RailType that is used on a tile.
	 */
	static RailType GetRailType(TileIndex tile);

	/**
	 * Convert the tracks on all tiles within a rectangle to another RailType.
	 * @param start_tile One corner of the rectangle.
	 * @param end_tile The opposite corner of the rectangle.
	 * @param convert_to The RailType you want to convert the rails to.
	 * @pre AIMap::IsValidTile(start_tile).
	 * @pre AIMap::IsValidTile(end_tile).
	 * @pre IsRailTypeAvailable(convert_to).
	 * @exception AIRail::ERR_UNSUITABLE_TRACK
	 * @return Whether at least some rail has been converted succesfully.
	 */
	static bool ConvertRailType(TileIndex start_tile, TileIndex end_tile, AIRail::RailType convert_to);

	/**
	 * Gets the tile in front of a rail depot.
	 * @param depot The rail depot tile.
	 * @pre IsRailDepotTile(depot).
	 * @return The tile in front of the depot.
	 */
	static TileIndex GetRailDepotFrontTile(TileIndex depot);

	/**
	 * Gets the direction of a rail station tile.
	 * @param tile The rail station tile.
	 * @pre IsRailStationTile(tile).
	 * @return The direction of the station (either RAILTRACK_NE_SW or RAILTRACK_NW_SE).
	 */
	static RailTrack GetRailStationDirection(TileIndex tile);

	/**
	 * Builds a rail depot.
	 * @param tile Place to build the depot.
	 * @param front The tile exactly in front of the depot.
	 * @pre AIMap::IsValidTile(tile).
	 * @pre AIMap::IsValidTile(front).
	 * @pre 'tile' is not equal to 'front', but in a straight line of it.
	 * @pre IsRailTypeAvailable(GetCurrentRailType()).
	 * @exception AIError::ERR_FLAT_LAND_REQUIRED
	 * @exception AIError::ERR_AREA_NOT_CLEAR
	 * @return Whether the rail depot has been/can be build or not.
	 */
	static bool BuildRailDepot(TileIndex tile, TileIndex front);

	/**
	 * Build a rail station.
	 * @param tile Place to build the station.
	 * @param direction The direction to build the station.
	 * @param num_platforms The number of platforms to build.
	 * @param platform_length The length of each platform.
	 * @param station_id The station to join, AIStation::STATION_NEW or AIStation::STATION_JOIN_ADJACENT.
	 * @pre IsRailTypeAvailable(GetCurrentRailType()).
	 * @pre AIMap::IsValidTile(tile).
	 * @pre direction == RAILTRACK_NW_SE || direction == RAILTRACK_NE_SW.
	 * @pre num_platforms > 0 && num_platforms <= 255.
	 * @pre platform_length > 0 && platform_length <= 255.
	 * @pre station_id == AIStation::STATION_NEW || station_id == AIStation::STATION_JOIN_ADJACENT || AIStation::IsValidStation(station_id).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception AIError::ERR_AREA_NOT_CLEAR
	 * @exception AIError::ERR_FLAT_LAND_REQUIRED
	 * @exception AIStation::ERR_STATION_TOO_CLOSE_TO_ANOTHER_STATION
	 * @exception AIStation::ERR_STATION_TOO_MANY_STATIONS
	 * @exception AIStation::ERR_STATION_TOO_MANY_STATIONS_IN_TOWN
	 * @return Whether the station has been/can be build or not.
	 */
	static bool BuildRailStation(TileIndex tile, RailTrack direction, uint num_platforms, uint platform_length, StationID station_id);

	/**
	 * Build a NewGRF rail station. This calls callback 18 to let a NewGRF
	 *  provide the station class / id to build, so we don't end up with
	 *  only the default stations on the map.
	 * @param tile Place to build the station.
	 * @param direction The direction to build the station.
	 * @param num_platforms The number of platforms to build.
	 * @param platform_length The length of each platform.
	 * @param station_id The station to join, AIStation::STATION_NEW or AIStation::STATION_JOIN_ADJACENT.
	 * @param cargo_id The CargoID of the cargo that will be transported from / to this station.
	 * @param source_industry The IndustryType of the industry you'll transport goods from.
	 * @param goal_industry The IndustryType of the industry you'll transport goods to.
	 * @param distance The manhattan distance you'll transport the cargo over.
	 * @param source_station True if this is the source station, false otherwise.
	 * @pre IsRailTypeAvailable(GetCurrentRailType()).
	 * @pre AIMap::IsValidTile(tile).
	 * @pre direction == RAILTRACK_NW_SE || direction == RAILTRACK_NE_SW.
	 * @pre num_platforms > 0 && num_platforms <= 255.
	 * @pre platform_length > 0 && platform_length <= 255.
	 * @pre station_id == AIStation::STATION_NEW || station_id == AIStation::STATION_JOIN_ADJACENT || AIStation::IsValidStation(station_id).
	 * @exception AIError::ERR_OWNED_BY_ANOTHER_COMPANY
	 * @exception AIError::ERR_AREA_NOT_CLEAR
	 * @exception AIError::ERR_FLAT_LAND_REQUIRED
	 * @exception AIStation::ERR_STATION_TOO_CLOSE_TO_ANOTHER_STATION
	 * @exception AIStation::ERR_STATION_TOO_MANY_STATIONS
	 * @exception AIStation::ERR_STATION_TOO_MANY_STATIONS_IN_TOWN
	 * @return Whether the station has been/can be build or not.
	 */
	static bool BuildNewGRFRailStation(TileIndex tile, RailTrack direction, uint num_platforms, uint platform_length, StationID station_id, CargoID cargo_id, IndustryType source_industry, IndustryType goal_industry, int distance, bool source_station);

	/**
	 * Build a rail waypoint.
	 * @param tile Place to build the waypoint.
	 * @pre AIMap::IsValidTile(tile).
	 * @pre IsRailTile(tile).
	 * @pre GetRailTracks(tile) == RAILTRACK_NE_SW || GetRailTracks(tile) == RAILTRACK_NW_SE.
	 * @pre IsRailTypeAvailable(GetCurrentRailType()).
	 * @exception AIError::ERR_FLAT_LAND_REQUIRED
	 * @return Whether the rail waypoint has been/can be build or not.
	 */
	static bool BuildRailWaypoint(TileIndex tile);

	/**
	 * Remove a rail waypoint.
	 * @param tile Place to remove the waypoint from.
	 * @pre AIMap::IsValidTile(tile).
	 * @pre IsRailWaypointTile(tile).
	 * @return Whether the rail waypoint has been/can be removed or not.
	 */
	static bool RemoveRailWaypoint(TileIndex tile);

	/**
	 * Remove a rectangle of platform pieces from a rail station.
	 * @param tile One corner of the rectangle to clear.
	 * @param tile2 The oppposite corner.
	 * @pre IsValidTile(tile).
	 * @pre IsValidTile(tile2).
	 * @return Whether at least one tile has been/can be cleared or not.
	 */
	static bool RemoveRailStationTileRect(TileIndex tile, TileIndex tile2);

	/**
	 * Get all RailTracks on the given tile.
	 * @param tile The tile to check.
	 * @pre IsRailTile(tile).
	 * @return A bitmask of RailTrack with all RailTracks on the tile.
	 */
	static uint GetRailTracks(TileIndex tile);

	/**
	 * Build rail on the given tile.
	 * @param tile The tile to build on.
	 * @param rail_track The RailTrack to build.
	 * @pre AIMap::IsValidTile(tile).
	 * @pre IsRailTypeAvailable(GetCurrentRailType()).
	 * @exception AIError::ERR_AREA_NOT_CLEAR
	 * @exception AIError::ERR_LAND_SLOPED_WRONG
	 * @exception AIRoad::ERR_ROAD_WORKS_IN_PROGRESS
	 * @exception AIRail::ERR_CROSSING_ON_ONEWAY_ROAD
	 * @exception AIError::ERR_ALREADY_BUILT
	 * @return Whether the rail has been/can be build or not.
	 * @note You can only build a single track with this function so do not
	 *   use the values from RailTrack as bitmask.
	 */
	static bool BuildRailTrack(TileIndex tile, RailTrack rail_track);

	/**
	 * Remove rail on the given tile.
	 * @param tile The tile to remove rail from.
	 * @param rail_track The RailTrack to remove.
	 * @pre AIMap::IsValidTile(tile).
	 * @pre (GetRailTracks(tile) & rail_track) != 0.
	 * @return Whether the rail has been/can be removed or not.
	 * @note You can only remove a single track with this function so do not
	 *   use the values from RailTrack as bitmask.
	 */
	static bool RemoveRailTrack(TileIndex tile, RailTrack rail_track);

	/**
	 * Check if a tile connects two adjacent tiles.
	 * @param from The first tile to connect.
	 * @param tile The tile that is checked.
	 * @param to The second tile to connect.
	 * @pre from != to.
	 * @pre AIMap::DistanceManhattan(from, tile) == 1.
	 * @pre AIMap::DistanceManhattan(to, tile) == 1.
	 * @return True if 'tile' connects 'from' and 'to'.
	 */
	static bool AreTilesConnected(TileIndex from, TileIndex tile, TileIndex to);

	/**
	 * Build a rail connection between two tiles.
	 * @param from The tile just before the tile to build on.
	 * @param tile The first tile to build on.
	 * @param to The tile just after the last tile to build on.
	 * @pre from != to.
	 * @pre AIMap::DistanceManhattan(from, tile) == 1.
	 * @pre AIMap::DistanceManhattan(to, tile) >= 1.
	 * @pre (abs(abs(AIMap::GetTileX(to) - AIMap::GetTileX(tile)) -
	 *          abs(AIMap::GetTileY(to) - AIMap::GetTileY(tile))) <= 1) ||
	 *      (AIMap::GetTileX(from) == AIMap::GetTileX(tile) && AIMap::GetTileX(tile) == AIMap::GetTileX(to)) ||
	 *      (AIMap::GetTileY(from) == AIMap::GetTileY(tile) && AIMap::GetTileY(tile) == AIMap::GetTileY(to)).
	 * @pre IsRailTypeAvailable(GetCurrentRailType()).
	 * @exception AIError::ERR_AREA_NOT_CLEAR
	 * @exception AIError::ERR_LAND_SLOPED_WRONG
	 * @exception AIRail::ERR_CROSSING_ON_ONEWAY_ROAD
	 * @exception AIRoad::ERR_ROAD_WORKS_IN_PROGRESS
	 * @exception AIError::ERR_ALREADY_BUILT
	 * @return Whether the rail has been/can be build or not.
	 */
	static bool BuildRail(TileIndex from, TileIndex tile, TileIndex to);

	/**
	 * Remove a rail connection between two tiles.
	 * @param from The tile just before the tile to remove rail from.
	 * @param tile The first tile to remove rail from.
	 * @param to The tile just after the last tile to remove rail from.
	 * @pre from != to.
	 * @pre AIMap::DistanceManhattan(from, tile) == 1.
	 * @pre AIMap::DistanceManhattan(to, tile) >= 1.
	 * @pre (abs(abs(AIMap::GetTileX(to) - AIMap::GetTileX(tile)) -
	 *          abs(AIMap::GetTileY(to) - AIMap::GetTileY(tile))) <= 1) ||
	 *      (AIMap::GetTileX(from) == AIMap::GetTileX(tile) && AIMap::GetTileX(tile) == AIMap::GetTileX(to)) ||
	 *      (AIMap::GetTileY(from) == AIMap::GetTileY(tile) && AIMap::GetTileY(tile) == AIMap::GetTileY(to)).
	 * @return Whether the rail has been/can be removed or not.
	 */
	static bool RemoveRail(TileIndex from, TileIndex tile, TileIndex to);

	/**
	 * Get the SignalType of the signal on a tile or SIGNALTYPE_NONE if there is no signal.
	 * @pre AIMap::DistanceManhattan(tile, front) == 1.
	 * @param tile The tile that might have a signal.
	 * @param front The tile in front of 'tile'.
	 * @return The SignalType of the signal on 'tile' with it's front to 'front'.
	 */
	static SignalType GetSignalType(TileIndex tile, TileIndex front);

	/**
	 * Build a signal on a tile.
	 * @param tile The tile to build on.
	 * @param front The tile in front of the signal.
	 * @param signal The SignalType to build.
	 * @pre AIMap::DistanceManhattan(tile, front) == 1.
	 * @pre IsRailTile(tile) && !IsRailStationTile(tile) && !IsRailWaypointTile(tile).
	 * @exception AIRail::ERR_UNSUITABLE_TRACK
	 * @return Whether the signal has been/can be build or not.
	 */
	static bool BuildSignal(TileIndex tile, TileIndex front, SignalType signal);

	/**
	 * Remove a signal.
	 * @param tile The tile to remove the signal from.
	 * @param front The tile in front of the signal.
	 * @pre AIMap::DistanceManhattan(tile, front) == 1.
	 * @pre GetSignalType(tile, front) != SIGNALTYPE_NONE.
	 * @return Whether the signal has been/can be removed or not.
	 */
	static bool RemoveSignal(TileIndex tile, TileIndex front);
};

#endif /* AI_RAIL_HPP */
