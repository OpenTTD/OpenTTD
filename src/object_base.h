/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file object_base.h Base for all objects. */

#ifndef OBJECT_BASE_H
#define OBJECT_BASE_H

#include "core/pool_type.hpp"
#include "object_type.h"
#include "tilearea_type.h"
#include "town_type.h"
#include "timer/timer_game_calendar.h"

using ObjectPool = Pool<Object, ObjectID, 64>;
extern ObjectPool _object_pool;

/** An object, such as transmitter, on the map. */
struct Object : ObjectPool::PoolItem<&_object_pool> {
	ObjectType type = INVALID_OBJECT_TYPE; ///< Type of the object
	Town *town = nullptr; ///< Town the object is built in
	TileArea location{INVALID_TILE, 0, 0}; ///< Location of the object
	TimerGameCalendar::Date build_date{}; ///< Date of construction
	uint8_t colour = 0; ///< Colour of the object, for display purpose
	uint8_t view = 0; ///< The view setting for this object

	/** Make sure the object isn't zeroed. */
	Object() {}
	Object(ObjectType type, Town *town, TileArea location, TimerGameCalendar::Date build_date, uint8_t view) :
		type(type), town(town), location(location), build_date(build_date), view(view) {}
	/** Make sure the right destructor is called as well! */
	~Object() {}

	static Object *GetByTile(TileIndex tile);

	/**
	 * Increment the count of objects for this type.
	 * @param type ObjectType to increment
	 * @pre type < NUM_OBJECTS
	 */
	static inline void IncTypeCount(ObjectType type)
	{
		assert(type < NUM_OBJECTS);
		Object::counts[type]++;
	}

	/**
	 * Decrement the count of objects for this type.
	 * @param type ObjectType to decrement
	 * @pre type < NUM_OBJECTS
	 */
	static inline void DecTypeCount(ObjectType type)
	{
		assert(type < NUM_OBJECTS);
		Object::counts[type]--;
	}

	/**
	 * Get the count of objects for this type.
	 * @param type ObjectType to query
	 * @pre type < NUM_OBJECTS
	 */
	static inline uint16_t GetTypeCount(ObjectType type)
	{
		assert(type < NUM_OBJECTS);
		return Object::counts[type];
	}

	/** Resets object counts. */
	static inline void ResetTypeCounts()
	{
		Object::counts.fill(0);
	}

protected:
	static std::array<uint16_t, NUM_OBJECTS> counts; ///< Number of objects per type ingame
};

/**
 * Keeps track of removed objects during execution/testruns of commands.
 */
struct ClearedObjectArea {
	TileIndex first_tile;  ///< The first tile being cleared, which then causes the whole object to be cleared.
	TileArea area;         ///< The area of the object.
};

ClearedObjectArea *FindClearedObject(TileIndex tile);
extern std::vector<ClearedObjectArea> _cleared_object_areas;

#endif /* OBJECT_BASE_H */
