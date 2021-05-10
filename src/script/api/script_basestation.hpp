/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_basestation.hpp Base for stations/waypoint handling. */

#ifndef SCRIPT_BASESTATION_HPP
#define SCRIPT_BASESTATION_HPP

#include "script_text.hpp"
#include "script_date.hpp"

/**
 * Base class for stations and waypoints.
 * @api ai game
 */
class ScriptBaseStation : public ScriptObject {
public:
	/**
	 * Special station IDs for building adjacent/new stations when
	 * the adjacent/distant join features are enabled.
	 */
	enum SpecialStationIDs {
		STATION_NEW = 0xFFFD,           ///< Build a new station
		STATION_JOIN_ADJACENT = 0xFFFE, ///< Join an neighbouring station if one exists
		STATION_INVALID = 0xFFFF,       ///< Invalid station id.
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
	 * @param name The new name of the station (can be either a raw string, or a ScriptText object).
	 * @pre IsValidBaseStation(station_id).
	 * @pre name != nullptr && len(name) != 0.
	 * @game @pre Valid ScriptCompanyMode active in scope.
	 * @exception ScriptError::ERR_NAME_IS_NOT_UNIQUE
	 * @return True if the name was changed.
	 */
	static bool SetName(StationID station_id, Text *name);

	/**
	 * Get the current location of a basestation.
	 * @param station_id The basestation to get the location of.
	 * @pre IsValidBaseStation(station_id).
	 * @return The tile the basestation sign above it.
	 * @note The tile is not necessarily a station tile (and if it is, it could also belong to another station).
	 * @see ScriptTileList_StationType.
	 */
	static TileIndex GetLocation(StationID station_id);

	/**
	 * Get the last date a station part was added to this station.
	 * @param station_id The station to look at.
	 * @return The last date some part of this station was build.
	 */
	static ScriptDate::Date GetConstructionDate(StationID station_id);
};

#endif /* SCRIPT_BASESTATION_HPP */
