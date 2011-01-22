/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_subsidy.hpp Everything to query subsidies. */

#ifndef AI_SUBSIDY_HPP
#define AI_SUBSIDY_HPP

#include "ai_company.hpp"

/**
 * Class that handles all subsidy related functions.
 */
class AISubsidy : public AIObject {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AISubsidy"; }

	/**
	 * Enumeration for source and destination of a subsidy.
	 * @note The list of values may grow in future.
	 */
	enum SubsidyParticipantType {
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
	 * Get the company index of the company this subsidy is awarded to.
	 * @param subsidy_id The SubsidyID to check.
	 * @pre IsAwarded(subsidy_id).
	 * @return The companyindex of the company this subsidy is awarded to.
	 */
	static AICompany::CompanyID GetAwardedTo(SubsidyID subsidy_id);

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
	static int32 GetExpireDate(SubsidyID subsidy_id);

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

#endif /* AI_SUBSIDY_HPP */
