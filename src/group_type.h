/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file group_type.h Types of a group. */

#ifndef GROUP_TYPE_H
#define GROUP_TYPE_H

#include "core/pool_type.hpp"

using GroupID = PoolID<uint16_t, struct GroupIDTag, 64000, 0xFFFF>;
static constexpr GroupID NEW_GROUP{0xFFFC}; ///< Sentinel for a to-be-created group.
static constexpr GroupID ALL_GROUP{0xFFFD}; ///< All vehicles are in this group.
static constexpr GroupID DEFAULT_GROUP{0xFFFE}; ///< Ungrouped vehicles are in this group.

static const uint MAX_LENGTH_GROUP_NAME_CHARS = 32; ///< The maximum length of a group name in characters including '\0'

struct Group;

#endif /* GROUP_TYPE_H */
