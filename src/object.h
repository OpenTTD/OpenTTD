/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file object.h Functions related to objects. */

#ifndef OBJECT_H
#define OBJECT_H

#include "economy_func.h"
#include "strings_type.h"
#include "object_type.h"

/**
 * Update the CompanyHQ to the state associated with the given score
 * @param tile  The (northern) tile of the company HQ, or INVALID_TILE.
 * @param score The current (performance) score of the company.
 */
void UpdateCompanyHQ(TileIndex tile, uint score);

/**
 * Actually build the object.
 * @param type  The type of object to build.
 * @param tile  The tile to build the northern tile of the object on.
 * @param owner The owner of the object.
 * @param town  Town the tile is related with.
 * @pre All preconditions for building the object at that location
 *      are met, e.g. slope and clearness of tiles are checked.
 */
void BuildObject(ObjectType type, TileIndex tile, CompanyID owner = OWNER_NONE, struct Town *town = NULL);


/** Various object behaviours. */
enum ObjectFlags {
	OBJECT_FLAG_NONE               =       0, ///< Just nothing.
	OBJECT_FLAG_ONLY_IN_SCENEDIT   = 1 <<  0, ///< Object can only be constructed in the scenario editor.
	OBJECT_FLAG_CANNOT_REMOVE      = 1 <<  1, ///< Object can not be removed.
	OBJECT_FLAG_AUTOREMOVE         = 1 <<  2, ///< Object get automatically removed (like "owned land").
	OBJECT_FLAG_BUILT_ON_WATER     = 1 <<  3, ///< Object can be built on water (not required).
	OBJECT_FLAG_CLEAR_INCOME       = 1 <<  4, ///< When object is cleared a positive income is generated instead of a cost.
	OBJECT_FLAG_HAS_NO_FOUNDATION  = 1 <<  5, ///< Do not display foundations when on a slope.
	OBJECT_FLAG_ANIMATION          = 1 <<  6, ///< Object has animated tiles.
	OBJECT_FLAG_ONLY_IN_GAME       = 1 <<  7, ///< Object can only be built in game.
	OBJECT_FLAG_2CC_COLOUR         = 1 <<  8, ///< Object wants 2CC colour mapping.
	OBJECT_FLAG_NOT_ON_LAND        = 1 <<  9, ///< Object can not be on land, implicitly sets #OBJECT_FLAG_BUILT_ON_WATER.
	OBJECT_FLAG_DRAW_WATER         = 1 << 10, ///< Object wants to be drawn on water.
	OBJECT_FLAG_ALLOW_UNDER_BRIDGE = 1 << 11, ///< Object can built under a bridge.
	OBJECT_FLAG_REQUIRE_FLAT       = 1 << 12, ///< Object can only be build of flat land, i.e. not on foundations!
};
DECLARE_ENUM_AS_BIT_SET(ObjectFlags)


/** An object that isn't use for transport, industries or houses. */
struct ObjectSpec {
	StringID name;               ///< The name for this object.
	uint8 size;                  ///< The size of this objects; low nibble for X, high nibble for Y.
	uint8 build_cost_multiplier; ///< Build cost multiplier per tile.
	uint8 clear_cost_multiplier; ///< Clear cost multiplier per tile.
	ObjectFlags flags;           ///< Flags/settings related to the object.

	/**
	 * Get the cost for building a structure of this type.
	 * @return The cost for building.
	 */
	Money GetBuildCost() const { return (_price[PR_BUILD_OBJECT] * this->build_cost_multiplier); }

	/**
	 * Get the cost for clearing a structure of this type.
	 * @return The cost for clearing.
	 */
	Money GetClearCost() const { return (_price[PR_CLEAR_OBJECT] * this->clear_cost_multiplier); }

	/**
	 * Get the specification associated with a specific ObjectType.
	 * @param index The object type to fetch.
	 * @return The specification.
	 */
	static const ObjectSpec *Get(ObjectType index);

	/**
	 * Get the specification associated with a tile.
	 * @param tile The tile to fetch the data for.
	 * @return The specification.
	 */
	static const ObjectSpec *GetByTile(TileIndex tile);
};


#endif /* OBJECT_H */
