/* $Id$ */

/** @file ai_map.cpp Implementation of AIMap. */

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
