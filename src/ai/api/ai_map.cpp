/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_map.cpp Implementation of AIMap. */

#include "../../stdafx.h"
#include "ai_map.hpp"
#include "../../tile_map.h"

/* static */ bool AIMap::IsValidTile(TileIndex t)
{
	return ::IsValidTile(t);
}

/* static */ TileIndex AIMap::GetMapSize()
{
	return ::MapSize();
}

/* static */ uint32 AIMap::GetMapSizeX()
{
	return ::MapSizeX();
}

/* static */ uint32 AIMap::GetMapSizeY()
{
	return ::MapSizeY();
}

/* static */ int32 AIMap::GetTileX(TileIndex t)
{
	if (!::IsValidTile(t)) return -1;
	return ::TileX(t);
}

/* static */ int32 AIMap::GetTileY(TileIndex t)
{
	if (!::IsValidTile(t)) return -1;
	return ::TileY(t);
}

/* static */ TileIndex AIMap::GetTileIndex(uint32 x, uint32 y)
{
	return ::TileXY(x, y);
}

/* static */ int32 AIMap::DistanceManhattan(TileIndex t1, TileIndex t2)
{
	if (!::IsValidTile(t1) || !::IsValidTile(t2)) return -1;
	return ::DistanceManhattan(t1, t2);
}

/* static */ int32 AIMap::DistanceMax(TileIndex t1, TileIndex t2)
{
	if (!::IsValidTile(t1) || !::IsValidTile(t2)) return -1;
	return ::DistanceMax(t1, t2);
}

/* static */ int32 AIMap::DistanceSquare(TileIndex t1, TileIndex t2)
{
	if (!::IsValidTile(t1) || !::IsValidTile(t2)) return -1;
	return ::DistanceSquare(t1, t2);
}

/* static */ int32 AIMap::DistanceFromEdge(TileIndex t)
{
	if (!::IsValidTile(t)) return -1;
	return ::DistanceFromEdge(t);
}
