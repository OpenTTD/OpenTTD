/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_map.cpp Implementation of ScriptMap. */

#include "../../stdafx.h"
#include "script_map.hpp"
#include "../../tile_map.h"

#include "../../safeguards.h"

/* static */ bool ScriptMap::IsValidTile(TileIndex t)
{
	return ::IsValidTile(t);
}

/* static */ TileIndex ScriptMap::GetMapSize()
{
	return ::MapSize();
}

/* static */ uint32 ScriptMap::GetMapSizeX()
{
	return ::MapSizeX();
}

/* static */ uint32 ScriptMap::GetMapSizeY()
{
	return ::MapSizeY();
}

/* static */ int32 ScriptMap::GetTileX(TileIndex t)
{
	if (!::IsValidTile(t)) return -1;
	return ::TileX(t);
}

/* static */ int32 ScriptMap::GetTileY(TileIndex t)
{
	if (!::IsValidTile(t)) return -1;
	return ::TileY(t);
}

/* static */ TileIndex ScriptMap::GetTileIndex(uint32 x, uint32 y)
{
	return ::TileXY(x, y);
}

/* static */ int32 ScriptMap::DistanceManhattan(TileIndex t1, TileIndex t2)
{
	if (!::IsValidTile(t1) || !::IsValidTile(t2)) return -1;
	return ::DistanceManhattan(t1, t2);
}

/* static */ int32 ScriptMap::DistanceMax(TileIndex t1, TileIndex t2)
{
	if (!::IsValidTile(t1) || !::IsValidTile(t2)) return -1;
	return ::DistanceMax(t1, t2);
}

/* static */ int32 ScriptMap::DistanceSquare(TileIndex t1, TileIndex t2)
{
	if (!::IsValidTile(t1) || !::IsValidTile(t2)) return -1;
	return ::DistanceSquare(t1, t2);
}

/* static */ int32 ScriptMap::DistanceFromEdge(TileIndex t)
{
	if (!::IsValidTile(t)) return -1;
	return ::DistanceFromEdge(t);
}
