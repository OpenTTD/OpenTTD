/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file object_type.h Types related to object tiles. */

#ifndef OBJECT_TYPE_H
#define OBJECT_TYPE_H

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
typedef uint32_t ObjectID;

struct Object;
struct ObjectSpec;

static const ObjectID INVALID_OBJECT = 0xFFFFFFFF; ///< An invalid object

#endif /* OBJECT_TYPE_H */
