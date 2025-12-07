/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file object_type.h Types related to object tiles. */

#ifndef OBJECT_TYPE_H
#define OBJECT_TYPE_H

#include "core/pool_type.hpp"

/** Types of objects. */
typedef uint16_t ObjectType;

static const ObjectType OBJECT_TRANSMITTER  =   0;    ///< The large antenna
static const ObjectType OBJECT_LIGHTHOUSE   =   1;    ///< The nice lighthouse
static const ObjectType OBJECT_STATUE       =   2;    ///< Statue in towns
static const ObjectType OBJECT_OWNED_LAND   =   3;    ///< Owned land 'flag'
static const ObjectType OBJECT_HQ           =   4;    ///< HeadQuarter of a player

static const ObjectType NEW_OBJECT_OFFSET   =   5;    ///< Offset for new objects
static const ObjectType NUM_OBJECTS         = 64000;  ///< Number of supported objects overall
static const ObjectType NUM_OBJECTS_PER_GRF = NUM_OBJECTS; ///< Number of supported objects per NewGRF
static const ObjectType INVALID_OBJECT_TYPE = 0xFFFF; ///< An invalid object

/** Unique identifier for an object. */
using ObjectID = PoolID<uint32_t, struct ObjectIDTag, 0xFF0000, 0xFFFFFFFF>;

struct Object;
struct ObjectSpec;

/** Animation triggers for objects. */
enum class ObjectAnimationTrigger : uint8_t {
	Built, ///< Triggered when the object is built (for all tiles at the same time).
	TileLoop, ///< Triggered in the periodic tile loop.
	TileLoopNorth, ///< Triggered every 256 ticks (for all tiles at the same time).
};
using ObjectAnimationTriggers = EnumBitSet<ObjectAnimationTrigger, uint16_t>;

#endif /* OBJECT_TYPE_H */
