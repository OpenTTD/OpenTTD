/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_accounting.hpp Everything to handle AI accounting things. */

#ifndef AI_ACCOUNTING_HPP
#define AI_ACCOUNTING_HPP

#include "ai_object.hpp"

/**
 * Class that keeps track of the costs, so you can request how much a block of
 *  commands did cost in total. Works in both Execute as in Test mode.
 * Example:
 * <pre>
 *   {
 *     local costs = AIAccounting();
 *     BuildRoad(from_here, to_here);
 *     BuildRoad(from_there, to_there);
 *     print("Costs for route is: " + costs.GetCosts());
 *   }
 * </pre>
 */
class AIAccounting : public AIObject {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIAccounting"; }

	/**
	 * Creating instance of this class starts counting the costs of commands
	 *   from zero. Saves the current value of GetCosts so we can return to
	 *   the old value when the instance gets deleted.
	 */
	AIAccounting();

	/**
	 * Restore the AIAccounting that was on top when we created this instance.
	 *   So basically restore the value of GetCosts to what it was before we
	 *   created this instance.
	 */
	~AIAccounting();

	/**
	 * Get the current value of the costs.
	 * @return The current costs.
	 * @note when nesting AIAccounting instances all instances' GetCosts
	 *   will always return the value of the 'top' instance.
	 */
	Money GetCosts();

	/**
	 * Reset the costs to zero.
	 * @note when nesting AIAccounting instances all instances' ResetCosts
	 *   will always effect on the 'top' instance.
	 */
	void ResetCosts();

private:
	Money last_costs; ///< The last cost we did return.
};

#endif /* AI_ACCOUNTING_HPP */
