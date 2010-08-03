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

#include "economy_func.h"
#include "strings_type.h"
#include "unmovable_type.h"

/**
 * Update the CompanyHQ to the state associated with the given score
 * @param c     The company to (possibly) update the HQ of.
 * @param score The current (performance) score of the company.
 * @pre c != NULL
 */
void UpdateCompanyHQ(Company *c, uint score);

/**
 * Actually build the unmovable object.
 * @param type  The type of object to build.
 * @param tile  The tile to build the northern tile of the object on.
 * @param owner The owner of the object.
 * @param index A (generic) index to be stored on the tile, e.g. TownID for statues.
 * @pre All preconditions for building the object at that location
 *      are met, e.g. slope and clearness of tiles are checked.
 */
void BuildUnmovable(UnmovableType type, TileIndex tile, CompanyID owner = OWNER_NONE, uint index = 0);


/** An (unmovable) object that isn't use for transport, industries or houses. */
struct UnmovableSpec {
	StringID name;               ///< The name for this object.
	uint8 size;                  ///< The size of this objects; low nibble for X, high nibble for Y.
	uint8 build_cost_multiplier; ///< Build cost multiplier per tile.
	uint8 clear_cost_multiplier; ///< Clear cost multiplier per tile.

	/**
	 * Get the cost for building a structure of this type.
	 * @return The cost for building.
	 */
	Money GetBuildCost() const { return (_price[PR_BUILD_UNMOVABLE] * this->build_cost_multiplier); }

	/**
	 * Get the cost for clearing a structure of this type.
	 * @return The cost for clearing.
	 */
	Money GetClearCost() const { return (_price[PR_CLEAR_UNMOVABLE] * this->clear_cost_multiplier); }

	/**
	 * Get the specification associated with a specific UnmovableType.
	 * @param index The unmovable type to fetch.
	 * @return The specification.
	 */
	static const UnmovableSpec *Get(UnmovableType index);

	/**
	 * Get the specification associated with a tile.
	 * @param tile The tile to fetch the data for.
	 * @return The specification.
	 */
	static const UnmovableSpec *GetByTile(TileIndex tile);
};


#endif /* UNMOVABLE_H */
