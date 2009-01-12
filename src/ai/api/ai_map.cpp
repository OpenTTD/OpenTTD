/* $Id$ */

/** @file ai_map.cpp Implementation of AIMap. */

#include "ai_map.hpp"
#include "../../map_func.h"
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

/* static */ uint32 AIMap::GetTileX(TileIndex t)
{
	return ::TileX(t);
}

/* static */ uint32 AIMap::GetTileY(TileIndex t)
{
	return ::TileY(t);
}

/* static */ TileIndex AIMap::GetTileIndex(uint32 x, uint32 y)
{
	return ::TileXY(x, y);
}

/* static */ uint32 AIMap::DistanceManhattan(TileIndex t1, TileIndex t2)
{
	return ::DistanceManhattan(t1, t2);
}

/* static */ uint32 AIMap::DistanceMax(TileIndex t1, TileIndex t2)
{
	return ::DistanceMax(t1, t2);
}

/* static */ uint32 AIMap::DistanceSquare(TileIndex t1, TileIndex t2)
{
	return ::DistanceSquare(t1, t2);
}

/* static */ uint32 AIMap::DistanceFromEdge(TileIndex t)
{
	return ::DistanceFromEdge(t);
}
