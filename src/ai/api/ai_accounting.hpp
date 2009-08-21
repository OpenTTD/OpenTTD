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
 *   {
 *     local costs = AIAccounting();
 *     BuildRoad(from_here, to_here);
 *     BuildRoad(from_there, to_there);
 *     print("Costs for route is: " + costs.GetCosts());
 *   }
 */
class AIAccounting : public AIObject {
public:
	static const char *GetClassName() { return "AIAccounting"; }

	/**
	 * Creating instance of this class starts counting the costs of commands
	 *   from zero.
	 * @note when the instance is destroyed, he restores the costs that was
	 *   current when the instance was created!
	 */
	AIAccounting();

	/**
	 * Destroying this instance reset the costs to the value it was
	 *   in when the instance was created.
	 */
	~AIAccounting();

	/**
	 * Get the current value of the costs.
	 * @return The current costs.
	 */
	Money GetCosts();

	/**
	 * Reset the costs to zero.
	 */
	void ResetCosts();

private:
	Money last_costs;
};

#endif /* AI_ACCOUNTING_HPP */
