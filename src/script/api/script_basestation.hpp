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
#include "script_company.hpp"
#include "script_date.hpp"
#include "../../station_type.h"

/**
 * Base class for stations and waypoints.
 * @api ai game
 */
class ScriptBaseStation : public ScriptObject {
public:
	static constexpr StationID STATION_NEW = ::NEW_STATION; ///< Build a new station or waypoint
	static constexpr StationID STATION_JOIN_ADJACENT = ::ADJACENT_STATION; ///< Join a neighbouring station or waypoint if one exists
	static constexpr StationID STATION_INVALID = ::StationID::Invalid(); ///< Invalid station or waypoint id.

	/**
	 * Checks whether the given basestation is valid and owned by you.
	 * @param station_id The station to check.
	 * @return True if and only if the basestation is valid.
	 * @note IsValidBaseStation == (IsValidStation || IsValidWaypoint).
	 * @api -all
	 */
	static bool IsValidBaseStation(StationID station_id);

	/**
	 * Get the owner of a basestation.
	 * @param station_id The basestation to get the owner of.
	 * @pre IsValidBaseStation(station_id).
	 * @return The owner the basestation has.
	 * @api -ai
	 */
	static ScriptCompany::CompanyID GetOwner(StationID station_id);

	/**
	 * Get the name of a station or waypoint.
	 * @param station_id The station or waypoint to get the name of.
	 * @pre IsValidStation(station_id) || IsValidWaypoint(station_id).
	 * @return The name of the station or waypoint.
	 */
	static std::optional<std::string> GetName(StationID station_id);

	/**
	 * Set the name of a station or waypoint.
	 * @param station_id The station or waypoint to set the name of.
	 * @param name The new name of the station or waypoint (can be either a raw string, or a ScriptText object).
	 * @pre IsValidStation(station_id) || IsValidWaypoint(station_id).
	 * @pre name != null && len(name) != 0.
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @exception ScriptError::ERR_NAME_IS_NOT_UNIQUE
	 * @return True if the name was changed.
	 */
	static bool SetName(StationID station_id, Text *name);

	/**
	 * Get the current location of a station or waypoint.
	 * @param station_id The station or waypoint to get the location of.
	 * @pre IsValidStation(station_id) || IsValidWaypoint(station_id).
	 * @return The tile with the station or waypoint sign above it.
	 * @note The tile is not necessarily a station or waypoint tile (and if it is, it could also belong to another station or waypoint).
	 * @see ScriptTileList_StationType.
	 */
	static TileIndex GetLocation(StationID station_id);

	/**
	 * Get the last calendar-date a station or waypoint part was added to this station or waypoint.
	 * @param station_id The station or waypoint to look at.
	 * @return The last calendar-date some part of this station or waypoint was built.
	 * @see \ref ScriptCalendarTime
	 */
	static ScriptDate::Date GetConstructionDate(StationID station_id);
};

#endif /* SCRIPT_BASESTATION_HPP */
