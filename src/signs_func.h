/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file signs_func.h Functions related to signs. */

#ifndef SIGNS_FUNC_H
#define SIGNS_FUNC_H

#include "signs_type.h"
#include "tile_type.h"

struct Window;

void UpdateAllSignVirtCoords();
void PlaceProc_Sign(TileIndex tile);
bool CompanyCanRenameSign(const Sign *si);

/* signs_gui.cpp */
void ShowRenameSignWindow(const Sign *si);
void HandleClickOnSign(const Sign *si);
void DeleteRenameSignWindow(SignID sign);

Window *ShowSignList();

#endif /* SIGNS_FUNC_H */
