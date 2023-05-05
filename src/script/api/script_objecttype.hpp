/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_objecttype.hpp Everything to query and build industries. */

#ifndef SCRIPT_OBJECTTYPE_HPP
#define SCRIPT_OBJECTTYPE_HPP

#include "script_list.hpp"

#include "../../newgrf_object.h"

/**
 * Class that handles all object-type related functions.
 * @api ai game
 */
class ScriptObjectType : public ScriptObject {
public:
	/**
	 * Checks whether the given object-type is valid.
	 * @param object_type The type to check.
	 * @return True if and only if the object-type is valid.
	 */
	static bool IsValidObjectType(ObjectType object_type);

	/**
	 * Get the name of an object-type.
	 * @param object_type The type to get the name for.
	 * @pre IsValidObjectType(object_type).
	 * @return The name of an object.
	 */
	static std::optional<std::string> GetName(ObjectType object_type);

	/**
	 * Get the number of views for an object-type.
	 * @param object_type The type to get the number of views for.
	 * @pre IsValidObjectType(object_type).
	 * @return The number of views for an object.
	 */
	static SQInteger GetViews(ObjectType object_type);

	/**
	 * Build an object of the specified type.
	 * @param object_type The type of the object to build.
	 * @param view The view for teh object.
	 * @param tile The tile to build the object on.
	 * @pre IsValidObjectType(object_type).
	 * @return True if the object was successfully build.
	 */
	static bool BuildObject(ObjectType object_type, SQInteger view, TileIndex tile);

	/**
	 * Get a specific object-type from a grf.
	 * @param grfid The ID of the NewGRF.
	 * @param grf_local_id The ID of the object, local to the NewGRF.
	 * @pre 0x00 <= grf_local_id < NUM_OBJECTS_PER_GRF.
	 * @return the object-type ID, local to the current game (this diverges from the grf_local_id).
	 */
	static ObjectType ResolveNewGRFID(SQInteger grfid, SQInteger grf_local_id);
};

#endif /* SCRIPT_OBJECTTYPE_HPP */
