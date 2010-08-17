/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file object_base.h Base for all objects. */

#ifndef OBJECT_BASE_H
#define OBJECT_BASE_H

#include "core/pool_type.hpp"
#include "object_type.h"
#include "tilearea_type.h"
#include "town_type.h"
#include "date_type.h"

typedef Pool<Object, ObjectID, 64, 64000> ObjectPool;
extern ObjectPool _object_pool;

/** An object, such as transmitter, on the map. */
struct Object : ObjectPool::PoolItem<&_object_pool> {
	Town *town;        ///< Town the object is built in
	TileArea location; ///< Location of the object
	Date build_date;   ///< Date of construction

	/** Make sure the object isn't zeroed. */
	Object() {}
	/** Make sure the right destructor is called as well! */
	~Object() {}

	/**
	 * Get the object associated with a tile.
	 * @param tile The tile to fetch the object for.
	 * @return The object.
	 */
	static Object *GetByTile(TileIndex tile);
};

#define FOR_ALL_OBJECTS_FROM(var, start) FOR_ALL_ITEMS_FROM(Object, object_index, var, start)
#define FOR_ALL_OBJECTS(var) FOR_ALL_OBJECTS_FROM(var, 0)

#endif /* OBJECT_BASE_H */
