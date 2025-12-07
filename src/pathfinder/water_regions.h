/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

 /** @file water_regions.h Handles dividing the water in the map into regions to assist pathfinding. */

#ifndef WATER_REGIONS_H
#define WATER_REGIONS_H

#include "../core/strong_typedef_type.hpp"
#include "../tile_type.h"
#include "../map_func.h"

using WaterRegionIndex = StrongType::Typedef<uint, struct TWaterRegionIndexTag, StrongType::Compare>;
using WaterRegionPatchLabel = StrongType::Typedef<uint8_t, struct TWaterRegionPatchLabelTag, StrongType::Compare, StrongType::Integer>;

constexpr int WATER_REGION_EDGE_LENGTH = 16;
constexpr int WATER_REGION_NUMBER_OF_TILES = WATER_REGION_EDGE_LENGTH * WATER_REGION_EDGE_LENGTH;
constexpr WaterRegionPatchLabel INVALID_WATER_REGION_PATCH{0};

/**
 * Describes a single interconnected patch of water within a particular water region.
 */
struct WaterRegionPatchDesc {
	int x; ///< The X coordinate of the water region, i.e. X=2 is the 3rd water region along the X-axis
	int y; ///< The Y coordinate of the water region, i.e. Y=2 is the 3rd water region along the Y-axis
	WaterRegionPatchLabel label; ///< Unique label identifying the patch within the region

	bool operator==(const WaterRegionPatchDesc &other) const { return x == other.x && y == other.y && label == other.label; }
};


/**
 * Describes a single square water region.
 */
struct WaterRegionDesc {
	int x; ///< The X coordinate of the water region, i.e. X=2 is the 3rd water region along the X-axis
	int y; ///< The Y coordinate of the water region, i.e. Y=2 is the 3rd water region along the Y-axis

	WaterRegionDesc(const int x, const int y) : x(x), y(y) {}
	WaterRegionDesc(const WaterRegionPatchDesc &water_region_patch) : x(water_region_patch.x), y(water_region_patch.y) {}

	bool operator==(const WaterRegionDesc &other) const { return x == other.x && y == other.y; }
};

int CalculateWaterRegionPatchHash(const WaterRegionPatchDesc &water_region_patch);

TileIndex GetWaterRegionCenterTile(const WaterRegionDesc &water_region);

WaterRegionDesc GetWaterRegionInfo(TileIndex tile);
WaterRegionPatchDesc GetWaterRegionPatchInfo(TileIndex tile);

void InvalidateWaterRegion(TileIndex tile);

using VisitWaterRegionPatchCallback = std::function<void(const WaterRegionPatchDesc &)>;
void VisitWaterRegionPatchNeighbours(const WaterRegionPatchDesc &water_region_patch, VisitWaterRegionPatchCallback &callback);

void AllocateWaterRegions();

void PrintWaterRegionDebugInfo(TileIndex tile);

#endif /* WATER_REGIONS_H */
