/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file object.h Functions related to objects. */

#ifndef OBJECT_H
#define OBJECT_H

#include "tile_type.h"
#include "company_type.h"
#include "object_type.h"

void UpdateCompanyHQ(TileIndex tile, uint score);

void BuildObject(ObjectType type, TileIndex tile, CompanyID owner = OWNER_NONE, struct Town *town = nullptr, uint8_t view = 0);

Window *ShowBuildObjectPicker();

#endif /* OBJECT_H */
