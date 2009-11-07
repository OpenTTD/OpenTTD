/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file unmovable.h Functions related to unmovable objects. */

#ifndef UNMOVABLE_H
#define UNMOVABLE_H

#include "unmovable_map.h"
#include "economy_type.h"
#include "economy_func.h"

void UpdateCompanyHQ(Company *c, uint score);

struct UnmovableSpec {
	StringID name;
	uint8 buy_cost_multiplier;
	uint8 sell_cost_multiplier;

	Money GetRemovalCost() const { return (_price[PR_CLEAR_ROUGH] * this->sell_cost_multiplier); }
	Money GetBuildingCost() const { return (_price[PR_CLEAR_ROUGH] * this->buy_cost_multiplier); }

};


#endif /* UNMOVABLE_H */
