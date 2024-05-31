/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file void_map.h Map accessors for void tiles. */

#ifndef VOID_MAP_H
#define VOID_MAP_H

#include "tile_map.h"

/**
 * Make a nice void tile ;)
 * @param t the tile to make void
 */
inline void MakeVoid(Tile t)
{
	SetTileType(t, MP_VOID);
	SetTileHeight(t, 0);
	t.m1() = 0;
	t.m2() = 0;
	t.m3() = 0;
	t.m4() = 0;
	t.m5() = 0;
	t.m6() = 0;
	t.m7() = 0;
	t.m8() = 0;
	t.m9() = 0;
}

#endif /* VOID_MAP_H */
