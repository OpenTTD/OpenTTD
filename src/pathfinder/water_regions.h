/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

 /** @file water_regions.h Handles dividing the water in the map into regions to assist pathfinding. */

#ifndef WATER_REGIONS_H
#define WATER_REGIONS_H

#include "tile_type.h"
#include "map_func.h"

using TWaterRegionPatchLabel = uint8_t;
using TWaterRegionIndex = uint;

constexpr int WATER_REGION_EDGE_LENGTH = 16;
constexpr int WATER_REGION_NUMBER_OF_TILES = WATER_REGION_EDGE_LENGTH * WATER_REGION_EDGE_LENGTH;
constexpr TWaterRegionPatchLabel INVALID_WATER_REGION_PATCH = 0;

/**
 * Describes a single interconnected patch of water within a particular water region.
 */
struct WaterRegionPatchDesc
{
	int x; ///< The X coordinate of the water region, i.e. X=2 is the 3rd water region along the X-axis
	int y; ///< The Y coordinate of the water region, i.e. Y=2 is the 3rd water region along the Y-axis
	TWaterRegionPatchLabel label; ///< Unique label identifying the patch within the region

	bool operator==(const WaterRegionPatchDesc &other) const { return x == other.x && y == other.y && label == other.label; }
	bool operator!=(const WaterRegionPatchDesc &other) const { return !(*this == other); }
};


/**
 * Describes a single square water region.
 */
struct WaterRegionDesc
{
	int x; ///< The X coordinate of the water region, i.e. X=2 is the 3rd water region along the X-axis
	int y; ///< The Y coordinate of the water region, i.e. Y=2 is the 3rd water region along the Y-axis

	WaterRegionDesc(const int x, const int y) : x(x), y(y) {}
	WaterRegionDesc(const WaterRegionPatchDesc &water_region_patch) : x(water_region_patch.x), y(water_region_patch.y) {}

	bool operator==(const WaterRegionDesc &other) const { return x == other.x && y == other.y; }
	bool operator!=(const WaterRegionDesc &other) const { return !(*this == other); }
};

TWaterRegionIndex GetWaterRegionIndex(const WaterRegionDesc &water_region);

int CalculateWaterRegionPatchHash(const WaterRegionPatchDesc &water_region_patch);

TileIndex GetWaterRegionCenterTile(const WaterRegionDesc &water_region);

WaterRegionDesc GetWaterRegionInfo(TileIndex tile);
WaterRegionPatchDesc GetWaterRegionPatchInfo(TileIndex tile);

void InvalidateWaterRegion(TileIndex tile);

using TVisitWaterRegionPatchCallBack = std::function<void(const WaterRegionPatchDesc &)>;
void VisitWaterRegionPatchNeighbors(const WaterRegionPatchDesc &water_region_patch, TVisitWaterRegionPatchCallBack &callback);

void AllocateWaterRegions();

void PrintWaterRegionDebugInfo(TileIndex tile);

#endif /* WATER_REGIONS_H */
