/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_object.cpp Handling of object NewGRFs. */

#include "stdafx.h"
#include "newgrf_object.h"
#include "object_map.h"

extern const ObjectSpec _original_objects[];

/* static */ const ObjectSpec *ObjectSpec::Get(ObjectType index)
{
	assert(index < OBJECT_MAX);
	return &_original_objects[index];
}

/* static */ const ObjectSpec *ObjectSpec::GetByTile(TileIndex tile)
{
	return ObjectSpec::Get(GetObjectType(tile));
}

