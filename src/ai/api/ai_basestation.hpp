/* $Id$ */

/** @file ai_waypoint.hpp Everything to query and build waypoints. */

#ifndef AI_BASESTATION_HPP
#define AI_BASESTATION_HPP

#include "ai_object.hpp"
#include "ai_error.hpp"

/**
 * Base class for stations and waypoints.
 */
class AIBaseStation : public AIObject {
public:
	static const char *GetClassName() { return "AIBaseStation"; }

	/**
	 * Special station IDs for building adjacent/new stations when
	 * the adjacent/distant join features are enabled.
	 */
	enum SpecialStationIDs {
		STATION_NEW = 0xFFFD,           //!< Build a new station
		STATION_JOIN_ADJACENT = 0xFFFE, //!< Join an neighbouring station if one exists
		STATION_INVALID = 0xFFFF,       //!< Invalid station id.
		WAYPOINT_INVALID = 0xFFFF,      //!< @deprecated Use STATION_INVALID instead.
	};

	/**
	 * Checks whether the given basestation is valid and owned by you.
	 * @param station_id The station to check.
	 * @return True if and only if the basestation is valid.
	 * @note IsValidBaseStation == (IsValidStation || IsValidWaypoint).
	 */
	static bool IsValidBaseStation(StationID station_id);

	/**
	 * Get the name of a basestation.
	 * @param station_id The basestation to get the name of.
	 * @pre IsValidBaseStation(station_id).
	 * @return The name of the station.
	 */
	static char *GetName(StationID station_id);

	/**
	 * Set the name this basestation.
	 * @param station_id The basestation to set the name of.
	 * @param name The new name of the station.
	 * @pre IsValidBaseStation(station_id).
	 * @pre 'name' must have at least one character.
	 * @pre 'name' must have at most 30 characters.
	 * @exception AIError::ERR_NAME_IS_NOT_UNIQUE
	 * @return True if the name was changed.
	 */
	static bool SetName(StationID station_id, const char *name);

	/**
	 * Get the current location of a basestation.
	 * @param station_id The basestation to get the location of.
	 * @pre IsValidBaseStation(station_id).
	 * @return The tile the basestation is currently on.
	 */
	static TileIndex GetLocation(StationID station_id);
};

#endif /* AI_BASESTATION_HPP */
