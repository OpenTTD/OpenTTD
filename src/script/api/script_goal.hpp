/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_goal.hpp Everything to manipulate the current running goal. */

#ifndef SCRIPT_GOAL_HPP
#define SCRIPT_GOAL_HPP

#include "script_object.hpp"
#include "script_company.hpp"
#include "../../goal_type.h"

/**
 * Class that handles some goal related functions.
 * @api game
 */
class ScriptGoal : public ScriptObject {
public:
	/**
	 * The goal IDs.
	 */
	enum GoalID {
		/* Note: these values represent part of the in-game GoalID enum */
		GOAL_INVALID = ::INVALID_GOALTYPE, ///< An invalid goal id.
	};

	/**
	 * Goal types that can be given to a goal.
	 */
	enum GoalType {
		/* Note: these values represent part of the in-game GoalType enum */
		GT_NONE     = ::GT_NONE,     ///< Destination is not linked.
		GT_TILE     = ::GT_TILE,     ///< Destination is a tile.
		GT_INDUSTRY = ::GT_INDUSTRY, ///< Destination is an industry.
		GT_TOWN     = ::GT_TOWN,     ///< Destination is a town.
		GT_COMPANY  = ::GT_COMPANY,  ///< Destination is a company.
	};

	/**
	 * Check whether this is a valid goalID.
	 * @param goal_id The GoalID to check.
	 * @return True if and only if this goal is valid.
	 */
	static bool IsValidGoal(GoalID goal_id);

	/**
	 * Create a new goal.
	 * @param company The company to create the goal for, or ScriptCompany::COMPANY_INVALID for all.
	 * @param goal The goal to add to the GUI.
	 * @param type The type of the goal.
	 * @param destination The destination of the #type type.
	 * @return The new GoalID, or GOAL_INVALID if it failed.
	 * @pre goal != NULL.
	 * @pre company == COMPANY_INVALID || ResolveCompanyID(company) != COMPANY_INVALID.
	 */
	static GoalID New(ScriptCompany::CompanyID company, const char *goal, GoalType type, uint32 destination);

	/**
	 * Remove a goal from the list.
	 * @param goal_id The goal to remove.
	 * @return True if the action succeeded.
	 * @pre IsValidGoal(goal_id).
	 */
	static bool Remove(GoalID goal_id);
};

#endif /* SCRIPT_GOAL_HPP */
