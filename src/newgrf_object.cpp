/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_object.cpp Handling of object NewGRFs. */

#include "stdafx.h"
#include "core/mem_func.hpp"
#include "newgrf_object.h"
#include "object_map.h"

extern const ObjectSpec _original_objects[NEW_OBJECT_OFFSET];
/** All the object specifications. */
static ObjectSpec _object_specs[NUM_OBJECTS];

/* static */ const ObjectSpec *ObjectSpec::Get(ObjectType index)
{
	assert(index < NUM_OBJECTS);
	return &_object_specs[index];
}

/* static */ const ObjectSpec *ObjectSpec::GetByTile(TileIndex tile)
{
	return ObjectSpec::Get(GetObjectType(tile));
}

/** This function initialize the spec arrays of objects. */
void ResetObjects()
{
	/* Clean the pool. */
	MemSetT(_object_specs, 0, lengthof(_object_specs));

	/* And add our originals. */
	MemCpyT(_object_specs, _original_objects, lengthof(_original_objects));
}

