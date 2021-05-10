/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_subsidy.hpp Everything to query subsidies. */

#ifndef SCRIPT_SUBSIDY_HPP
#define SCRIPT_SUBSIDY_HPP

#include "script_company.hpp"
#include "script_date.hpp"

/**
 * Class that handles all subsidy related functions.
 * @api ai game
 */
class ScriptSubsidy : public ScriptObject {
public:
	/**
	 * Enumeration for source and destination of a subsidy.
	 * @note The list of values may grow in future.
	 */
	enum SubsidyParticipantType {
		/* Values are important, as they represent the internal state of the game.
		 *  It is originally named SourceType. ST_HEADQUARTERS is intentionally
		 *  left out, as it cannot be used for Subsidies. */
		SPT_INDUSTRY =    0, ///< Subsidy participant is an industry
		SPT_TOWN     =    1, ///< Subsidy participant is a town
		SPT_INVALID  = 0xFF, ///< Invalid/unknown participant type
	};

	/**
	 * Check whether this is a valid SubsidyID.
	 * @param subsidy_id The SubsidyID to check.
	 * @return True if and only if this subsidy is still valid.
	 */
	static bool IsValidSubsidy(SubsidyID subsidy_id);

	/**
	 * Checks whether this subsidy is already awarded to some company.
	 * @param subsidy_id The SubsidyID to check.
	 * @pre IsValidSubsidy(subsidy).
	 * @return True if and only if this subsidy is already awarded.
	 */
	static bool IsAwarded(SubsidyID subsidy_id);

	/**
	 * Create a new subsidy.
	 * @param cargo_type The type of cargo to cary for the subsidy.
	 * @param from_type The type of the subsidy on the 'from' side.
	 * @param from_id The ID of the 'from' side.
	 * @param to_type The type of the subsidy on the 'to' side.
	 * @param to_id The ID of the 'to' side.
	 * @return True if the action succeeded.
	 * @pre ScriptCargo::IsValidCargo(cargo_type)
	 * @pre from_type == SPT_INDUSTRY || from_type == SPT_TOWN.
	 * @pre to_type   == SPT_INDUSTRY || to_type   == SPT_TOWN.
	 * @pre (from_type == SPT_INDUSTRY && ScriptIndustry::IsValidIndustry(from_id)) || (from_type == SPT_TOWN && ScriptTown::IsValidTown(from_id))
	 * @pre (to_type   == SPT_INDUSTRY && ScriptIndustry::IsValidIndustry(to_id))   || (to_type   == SPT_TOWN && ScriptTown::IsValidTown(to_id))
	 * @api -ai
	 */
	static bool Create(CargoID cargo_type, SubsidyParticipantType from_type, uint16 from_id, SubsidyParticipantType to_type, uint16 to_id);

	/**
	 * Get the company index of the company this subsidy is awarded to.
	 * @param subsidy_id The SubsidyID to check.
	 * @pre IsAwarded(subsidy_id).
	 * @return The companyindex of the company this subsidy is awarded to.
	 */
	static ScriptCompany::CompanyID GetAwardedTo(SubsidyID subsidy_id);

	/**
	 * Get the date this subsidy expires. In case the subsidy is already
	 *  awarded, return the date the subsidy expires, else, return the date the
	 *  offer expires.
	 * @param subsidy_id The SubsidyID to check.
	 * @pre IsValidSubsidy(subsidy_id).
	 * @return The last valid date of this subsidy.
	 * @note The return value of this function will change if the subsidy is
	 *  awarded.
	 */
	static ScriptDate::Date GetExpireDate(SubsidyID subsidy_id);

	/**
	 * Get the cargo type that has to be transported in order to be awarded this
	 *  subsidy.
	 * @param subsidy_id The SubsidyID to check.
	 * @pre IsValidSubsidy(subsidy_id).
	 * @return The cargo type to transport.
	 */
	static CargoID GetCargoType(SubsidyID subsidy_id);

	/**
	 * Returns the type of source of subsidy.
	 * @param subsidy_id The SubsidyID to check.
	 * @pre IsValidSubsidy(subsidy_id).
	 * @return Type of source of subsidy.
	 */
	static SubsidyParticipantType GetSourceType(SubsidyID subsidy_id);

	/**
	 * Return the source IndustryID/TownID the subsidy is for.
	 * \li GetSourceType(subsidy_id) == SPT_INDUSTRY -> return the IndustryID.
	 * \li GetSourceType(subsidy_id) == SPT_TOWN -> return the TownID.
	 * @param subsidy_id The SubsidyID to check.
	 * @pre IsValidSubsidy(subsidy_id).
	 * @return One of TownID/IndustryID.
	 */
	static int32 GetSourceIndex(SubsidyID subsidy_id);

	/**
	 * Returns the type of destination of subsidy.
	 * @param subsidy_id The SubsidyID to check.
	 * @pre IsValidSubsidy(subsidy_id).
	 * @return Type of destination of subsidy.
	 */
	static SubsidyParticipantType GetDestinationType(SubsidyID subsidy_id);

	/**
	 * Return the destination IndustryID/TownID the subsidy is for.
	 * \li GetDestinationType(subsidy_id) == SPT_INDUSTRY -> return the IndustryID.
	 * \li GetDestinationType(subsidy_id) == SPT_TOWN -> return the TownID.
	 * @param subsidy_id the SubsidyID to check.
	 * @pre IsValidSubsidy(subsidy_id).
	 * @return One of TownID/IndustryID.
	 */
	static int32 GetDestinationIndex(SubsidyID subsidy_id);
};

#endif /* SCRIPT_SUBSIDY_HPP */
