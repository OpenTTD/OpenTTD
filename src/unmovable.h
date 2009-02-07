/* $Id$ */

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

	Money GetRemovalCost() const { return (_price.clear_roughland * this->sell_cost_multiplier); }
	Money GetBuildingCost() const { return (_price.clear_roughland * this->buy_cost_multiplier); }

};


#endif /* UNMOVABLE_H */
