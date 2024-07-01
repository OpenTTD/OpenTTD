/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

 /** @file water_regions.cpp Handles dividing the water in the map into square regions to assist pathfinding. */

#include "stdafx.h"
#include "map_func.h"
#include "water_regions.h"
#include "map_func.h"
#include "tilearea_type.h"
#include "track_func.h"
#include "transport_type.h"
#include "landscape.h"
#include "tunnelbridge_map.h"
#include "follow_track.hpp"
#include "ship.h"
#include "debug.h"

using TWaterRegionTraversabilityBits = uint16_t;
constexpr TWaterRegionPatchLabel FIRST_REGION_LABEL = 1;

static_assert(sizeof(TWaterRegionTraversabilityBits) * 8 == WATER_REGION_EDGE_LENGTH);
static_assert(sizeof(TWaterRegionPatchLabel) == sizeof(uint8_t)); // Important for the hash calculation.

static inline TrackBits GetWaterTracks(TileIndex tile) { return TrackStatusToTrackBits(GetTileTrackStatus(tile, TRANSPORT_WATER, 0)); }
static inline bool IsAqueductTile(TileIndex tile) { return IsBridgeTile(tile) && GetTunnelBridgeTransportType(tile) == TRANSPORT_WATER; }

static inline int GetWaterRegionX(TileIndex tile) { return TileX(tile) / WATER_REGION_EDGE_LENGTH; }
static inline int GetWaterRegionY(TileIndex tile) { return TileY(tile) / WATER_REGION_EDGE_LENGTH; }

static inline int GetWaterRegionMapSizeX() { return Map::SizeX() / WATER_REGION_EDGE_LENGTH; }
static inline int GetWaterRegionMapSizeY() { return Map::SizeY() / WATER_REGION_EDGE_LENGTH; }

static inline TWaterRegionIndex GetWaterRegionIndex(int region_x, int region_y) { return GetWaterRegionMapSizeX() * region_y + region_x; }
static inline TWaterRegionIndex GetWaterRegionIndex(TileIndex tile) { return GetWaterRegionIndex(GetWaterRegionX(tile), GetWaterRegionY(tile)); }

using TWaterRegionPatchLabelArray = std::array<TWaterRegionPatchLabel, WATER_REGION_NUMBER_OF_TILES>;

/**
 * The data stored for each water region.
 */
class WaterRegionData {
	friend class WaterRegion;

	std::array<TWaterRegionTraversabilityBits, DIAGDIR_END> edge_traversability_bits{};
	std::unique_ptr<TWaterRegionPatchLabelArray> tile_patch_labels; // Tile patch labels, this may be nullptr in the following trivial cases: region is invalid, region is only land (0 patches), region is only water (1 patch)
	bool has_cross_region_aqueducts = false;
	TWaterRegionPatchLabel number_of_patches = 0; // 0 = no water, 1 = one single patch of water, etc...
};

/**
 * Represents a square section of the map of a fixed size. Within this square individual unconnected patches of water are
 * identified using a Connected Component Labeling (CCL) algorithm. Note that all information stored in this class applies
 * only to tiles within the square section, there is no knowledge about the rest of the map. This makes it easy to invalidate
 * and update a water region if any changes are made to it, such as construction or terraforming.
 */
class WaterRegion
{
private:
	WaterRegionData &data;
	const OrthogonalTileArea tile_area;

	/**
	 * Returns the local index of the tile within the region. The N corner represents 0,
	 * the x direction is positive in the SW direction, and Y is positive in the SE direction.
	 * @param tile Tile within the water region.
	 * @returns The local index.
	 */
	inline int GetLocalIndex(TileIndex tile) const
	{
		assert(this->tile_area.Contains(tile));
		return (TileX(tile) - TileX(this->tile_area.tile)) + WATER_REGION_EDGE_LENGTH * (TileY(tile) - TileY(this->tile_area.tile));
	}

public:
	WaterRegion(int region_x, int region_y, WaterRegionData &water_region_data)
		: data(water_region_data)
		, tile_area(TileXY(region_x * WATER_REGION_EDGE_LENGTH, region_y * WATER_REGION_EDGE_LENGTH), WATER_REGION_EDGE_LENGTH, WATER_REGION_EDGE_LENGTH)
	{}

	OrthogonalTileIterator begin() const { return this->tile_area.begin(); }
	OrthogonalTileIterator end() const { return this->tile_area.end(); }

	/**
	 * Returns a set of bits indicating whether an edge tile on a particular side is traversable or not. These
	 * values can be used to determine whether a ship can enter/leave the region through a particular edge tile.
	 * @see GetLocalIndex() for a description of the coordinate system used.
	 * @param side Which side of the region we want to know the edge traversability of.
	 * @returns A value holding the edge traversability bits.
	 */
	TWaterRegionTraversabilityBits GetEdgeTraversabilityBits(DiagDirection side) const { return this->data.edge_traversability_bits[side]; }

	/**
	 * @returns The amount of individual water patches present within the water region. A value of
	 * 0 means there is no water present in the water region at all.
	 */
	int NumberOfPatches() const { return this->data.number_of_patches; }

	/**
	 * @returns Whether the water region contains aqueducts that cross the region boundaries.
	 */
	bool HasCrossRegionAqueducts() const { return this->data.has_cross_region_aqueducts; }

	/**
	 * Returns the patch label that was assigned to the tile.
	 * @param tile The tile of which we want to retrieve the label.
	 * @returns The label assigned to the tile.
	 */
	TWaterRegionPatchLabel GetLabel(TileIndex tile) const
	{
		assert(this->tile_area.Contains(tile));
		if (this->data.tile_patch_labels == nullptr) {
			return this->NumberOfPatches() == 0 ? INVALID_WATER_REGION_PATCH : 1;
		}
		return (*this->data.tile_patch_labels)[GetLocalIndex(tile)];
	}

	/**
	 * Performs the connected component labeling and other data gathering.
	 * @see WaterRegion
	 */
	void ForceUpdate()
	{
		Debug(map, 3, "Updating water region ({},{})", GetWaterRegionX(this->tile_area.tile), GetWaterRegionY(this->tile_area.tile));
		this->data.has_cross_region_aqueducts = false;

		/* Acquire a tile patch label array if this region does not already have one */
		if (this->data.tile_patch_labels == nullptr) {
			this->data.tile_patch_labels = std::make_unique<TWaterRegionPatchLabelArray>();
		}

		this->data.tile_patch_labels->fill(INVALID_WATER_REGION_PATCH);
		this->data.edge_traversability_bits.fill(0);

		TWaterRegionPatchLabel current_label = 1;
		TWaterRegionPatchLabel highest_assigned_label = 0;

		/* Perform connected component labeling. This uses a flooding algorithm that expands until no
		 * additional tiles can be added. Only tiles inside the water region are considered. */
		for (const TileIndex start_tile : tile_area) {
			static std::vector<TileIndex> tiles_to_check;
			tiles_to_check.clear();
			tiles_to_check.push_back(start_tile);

			bool increase_label = false;
			while (!tiles_to_check.empty()) {
				const TileIndex tile = tiles_to_check.back();
				tiles_to_check.pop_back();

				const TrackdirBits valid_dirs = TrackBitsToTrackdirBits(GetWaterTracks(tile));
				if (valid_dirs == TRACKDIR_BIT_NONE) continue;

				TWaterRegionPatchLabel &tile_patch = (*this->data.tile_patch_labels)[GetLocalIndex(tile)];
				if (tile_patch != INVALID_WATER_REGION_PATCH) continue;

				tile_patch = current_label;
				highest_assigned_label = current_label;
				increase_label = true;

				for (const Trackdir dir : SetTrackdirBitIterator(valid_dirs)) {
					/* By using a TrackFollower we "play by the same rules" as the actual ship pathfinder */
					CFollowTrackWater ft;
					if (ft.Follow(tile, dir)) {
						if (this->tile_area.Contains(ft.m_new_tile)) {
							tiles_to_check.push_back(ft.m_new_tile);
						} else if (!ft.m_is_bridge) {
							assert(DistanceManhattan(ft.m_new_tile, tile) == 1);
							const auto side = DiagdirBetweenTiles(tile, ft.m_new_tile);
							const int local_x_or_y = DiagDirToAxis(side) == AXIS_X ? TileY(tile) - TileY(this->tile_area.tile) : TileX(tile) - TileX(this->tile_area.tile);
							SetBit(this->data.edge_traversability_bits[side], local_x_or_y);
						} else {
							this->data.has_cross_region_aqueducts = true;
						}
					}
				}
			}

			if (increase_label) current_label++;
		}

		this->data.number_of_patches = highest_assigned_label;

		if (this->data.number_of_patches == 0 || (this->data.number_of_patches == 1 &&
				std::all_of(this->data.tile_patch_labels->begin(), this->data.tile_patch_labels->end(), [](TWaterRegionPatchLabel label) { return label == 1; }))) {
			/* No need for patch storage: trivial cases */
			this->data.tile_patch_labels.reset();
		}
	}

	void PrintDebugInfo()
	{
		Debug(map, 9, "Water region {},{} labels and edge traversability = ...", GetWaterRegionX(tile_area.tile), GetWaterRegionY(tile_area.tile));

		const size_t max_element_width = std::to_string(this->data.number_of_patches).size();

		std::array<int, 16> traversability_NW{0};
		for (auto bitIndex : SetBitIterator(this->data.edge_traversability_bits[DIAGDIR_NW])) *(traversability_NW.rbegin() + bitIndex) = 1;
		Debug(map, 9, "    {:{}}", fmt::join(traversability_NW, " "), max_element_width);
		Debug(map, 9, "  +{:->{}}+", "", WATER_REGION_EDGE_LENGTH * (max_element_width + 1) + 1);

		for (int y = 0; y < WATER_REGION_EDGE_LENGTH; ++y) {
			std::string line{};
			for (int x = 0; x < WATER_REGION_EDGE_LENGTH; ++x) {
				const auto label = this->GetLabel(TileAddXY(tile_area.tile, x, y));
				const std::string label_str = label == INVALID_WATER_REGION_PATCH ? "." : std::to_string(label);
				line = fmt::format("{:{}}", label_str, max_element_width) + " " + line;
			}
			Debug(map, 9, "{} | {}| {}", GB(this->data.edge_traversability_bits[DIAGDIR_SW], y, 1), line, GB(this->data.edge_traversability_bits[DIAGDIR_NE], y, 1));
		}

		Debug(map, 9, "  +{:->{}}+", "", WATER_REGION_EDGE_LENGTH * (max_element_width + 1) + 1);
		std::array<int, 16> traversability_SE{0};
		for (auto bitIndex : SetBitIterator(this->data.edge_traversability_bits[DIAGDIR_SE])) *(traversability_SE.rbegin() + bitIndex) = 1;
		Debug(map, 9, "    {:{}}", fmt::join(traversability_SE, " "), max_element_width);
	}
};

std::vector<WaterRegionData> _water_region_data;
std::vector<bool> _is_water_region_valid;

TileIndex GetTileIndexFromLocalCoordinate(int region_x, int region_y, int local_x, int local_y)
{
	assert(local_x >= 0 && local_x < WATER_REGION_EDGE_LENGTH);
	assert(local_y >= 0 && local_y < WATER_REGION_EDGE_LENGTH);
	return TileXY(WATER_REGION_EDGE_LENGTH * region_x + local_x, WATER_REGION_EDGE_LENGTH * region_y + local_y);
}

TileIndex GetEdgeTileCoordinate(int region_x, int region_y, DiagDirection side, int x_or_y)
{
	assert(x_or_y >= 0 && x_or_y < WATER_REGION_EDGE_LENGTH);
	switch (side) {
		case DIAGDIR_NE: return GetTileIndexFromLocalCoordinate(region_x, region_y, 0, x_or_y);
		case DIAGDIR_SW: return GetTileIndexFromLocalCoordinate(region_x, region_y, WATER_REGION_EDGE_LENGTH - 1, x_or_y);
		case DIAGDIR_NW: return GetTileIndexFromLocalCoordinate(region_x, region_y, x_or_y, 0);
		case DIAGDIR_SE: return GetTileIndexFromLocalCoordinate(region_x, region_y, x_or_y, WATER_REGION_EDGE_LENGTH - 1);
		default: NOT_REACHED();
	}
}

WaterRegion GetUpdatedWaterRegion(uint16_t region_x, uint16_t region_y)
{
	const int index = GetWaterRegionIndex(region_x, region_y);
	WaterRegion water_region(region_x, region_y, _water_region_data[index]);
	if (!_is_water_region_valid[index]) {
		water_region.ForceUpdate();
		_is_water_region_valid[index] = true;
	}
	return water_region;
}

WaterRegion GetUpdatedWaterRegion(TileIndex tile)
{
	return GetUpdatedWaterRegion(GetWaterRegionX(tile), GetWaterRegionY(tile));
}

/**
 * Returns the index of the water region.
 * @param water_region The water region to return the index for.
 */
TWaterRegionIndex GetWaterRegionIndex(const WaterRegionDesc &water_region)
{
	return GetWaterRegionIndex(water_region.x, water_region.y);
}

/**
 * Calculates a number that uniquely identifies the provided water region patch.
 * @param water_region_patch The Water region to calculate the hash for.
 */
int CalculateWaterRegionPatchHash(const WaterRegionPatchDesc &water_region_patch)
{
	return water_region_patch.label | GetWaterRegionIndex(water_region_patch) << 8;
}

/**
 * Returns the center tile of a particular water region.
 * @param water_region The water region to find the center tile for.
 * @returns The center tile of the water region.
 */
TileIndex GetWaterRegionCenterTile(const WaterRegionDesc &water_region)
{
	return TileXY(water_region.x * WATER_REGION_EDGE_LENGTH + (WATER_REGION_EDGE_LENGTH / 2), water_region.y * WATER_REGION_EDGE_LENGTH + (WATER_REGION_EDGE_LENGTH / 2));
}

/**
 * Returns basic water region information for the provided tile.
 * @param tile The tile for which the information will be calculated.
 */
WaterRegionDesc GetWaterRegionInfo(TileIndex tile)
{
	return WaterRegionDesc{ GetWaterRegionX(tile), GetWaterRegionY(tile) };
}

/**
 * Returns basic water region patch information for the provided tile.
 * @param tile The tile for which the information will be calculated.
 */
WaterRegionPatchDesc GetWaterRegionPatchInfo(TileIndex tile)
{
	const WaterRegion region = GetUpdatedWaterRegion(tile);
	return WaterRegionPatchDesc{ GetWaterRegionX(tile), GetWaterRegionY(tile), region.GetLabel(tile)};
}

/**
 * Marks the water region that tile is part of as invalid.
 * @param tile Tile within the water region that we wish to invalidate.
 */
void InvalidateWaterRegion(TileIndex tile)
{
	if (!IsValidTile(tile)) return;

	auto invalidate_region = [](TileIndex tile) {
		const int water_region_index = GetWaterRegionIndex(tile);
		if (!_is_water_region_valid[water_region_index]) Debug(map, 3, "Invalidated water region ({},{})", GetWaterRegionX(tile), GetWaterRegionY(tile));
		_is_water_region_valid[water_region_index] = false;
	};

	invalidate_region(tile);

	/* When updating the water region we look into the first tile of adjacent water regions to determine edge
	 * traversability. This means that if we invalidate any region edge tiles we might also change the traversability
	 * of the adjacent region. This code ensures the adjacent regions also get invalidated in such a case. */
	for (DiagDirection side = DIAGDIR_BEGIN; side < DIAGDIR_END; side++) {
		const TileIndex adjacent_tile = TileAddByDiagDir(tile, side);
		if (GetWaterRegionIndex(adjacent_tile) != GetWaterRegionIndex(tile)) invalidate_region(adjacent_tile);
	}
}

/**
 * Calls the provided callback function for all water region patches
 * accessible from one particular side of the starting patch.
 * @param water_region_patch Water patch within the water region to start searching from
 * @param side Side of the water region to look for neigboring patches of water
 * @param callback The function that will be called for each neighbor that is found
 */
static inline void VisitAdjacentWaterRegionPatchNeighbors(const WaterRegionPatchDesc &water_region_patch, DiagDirection side, TVisitWaterRegionPatchCallBack &func)
{
	if (water_region_patch.label == INVALID_WATER_REGION_PATCH) return;

	const WaterRegion current_region = GetUpdatedWaterRegion(water_region_patch.x, water_region_patch.y);

	const TileIndexDiffC offset = TileIndexDiffCByDiagDir(side);
	const int nx = water_region_patch.x + offset.x;
	const int ny = water_region_patch.y + offset.y;

	if (nx < 0 || ny < 0 || nx >= GetWaterRegionMapSizeX() || ny >= GetWaterRegionMapSizeY()) return;

	const WaterRegion neighboring_region = GetUpdatedWaterRegion(nx, ny);
	const DiagDirection opposite_side = ReverseDiagDir(side);

	/* Indicates via which local x or y coordinates (depends on the "side" parameter) we can cross over into the adjacent region. */
	const TWaterRegionTraversabilityBits traversability_bits = current_region.GetEdgeTraversabilityBits(side)
		& neighboring_region.GetEdgeTraversabilityBits(opposite_side);
	if (traversability_bits == 0) return;

	if (current_region.NumberOfPatches() == 1 && neighboring_region.NumberOfPatches() == 1) {
		func(WaterRegionPatchDesc{ nx, ny, FIRST_REGION_LABEL }); // No further checks needed because we know there is just one patch for both adjacent regions
		return;
	}

	/* Multiple water patches can be reached from the current patch. Check each edge tile individually. */
	static std::vector<TWaterRegionPatchLabel> unique_labels; // static and vector-instead-of-map for performance reasons
	unique_labels.clear();
	for (int x_or_y = 0; x_or_y < WATER_REGION_EDGE_LENGTH; ++x_or_y) {
		if (!HasBit(traversability_bits, x_or_y)) continue;

		const TileIndex current_edge_tile = GetEdgeTileCoordinate(water_region_patch.x, water_region_patch.y, side, x_or_y);
		const TWaterRegionPatchLabel current_label = current_region.GetLabel(current_edge_tile);
		if (current_label != water_region_patch.label) continue;

		const TileIndex neighbor_edge_tile = GetEdgeTileCoordinate(nx, ny, opposite_side, x_or_y);
		const TWaterRegionPatchLabel neighbor_label = neighboring_region.GetLabel(neighbor_edge_tile);
		assert(neighbor_label != INVALID_WATER_REGION_PATCH);
		if (std::find(unique_labels.begin(), unique_labels.end(), neighbor_label) == unique_labels.end()) unique_labels.push_back(neighbor_label);
	}
	for (TWaterRegionPatchLabel unique_label : unique_labels) func(WaterRegionPatchDesc{ nx, ny, unique_label });
}

/**
 * Calls the provided callback function on all accessible water region patches in
 * each cardinal direction, plus any others that are reachable via aqueducts.
 * @param water_region_patch Water patch within the water region to start searching from
 * @param callback The function that will be called for each accessible water patch that is found
 */
void VisitWaterRegionPatchNeighbors(const WaterRegionPatchDesc &water_region_patch, TVisitWaterRegionPatchCallBack &callback)
{
	if (water_region_patch.label == INVALID_WATER_REGION_PATCH) return;

	const WaterRegion current_region = GetUpdatedWaterRegion(water_region_patch.x, water_region_patch.y);

	/* Visit adjacent water region patches in each cardinal direction */
	for (DiagDirection side = DIAGDIR_BEGIN; side < DIAGDIR_END; side++) VisitAdjacentWaterRegionPatchNeighbors(water_region_patch, side, callback);

	/* Visit neigboring water patches accessible via cross-region aqueducts */
	if (current_region.HasCrossRegionAqueducts()) {
		for (const TileIndex tile : current_region) {
			if (GetWaterRegionPatchInfo(tile) == water_region_patch && IsAqueductTile(tile)) {
				const TileIndex other_end_tile = GetOtherBridgeEnd(tile);
				if (GetWaterRegionIndex(tile) != GetWaterRegionIndex(other_end_tile)) callback(GetWaterRegionPatchInfo(other_end_tile));
			}
		}
	}
}

/**
 * Allocates the appropriate amount of water regions for the current map size
 */
void AllocateWaterRegions()
{
	const int number_of_regions = GetWaterRegionMapSizeX() * GetWaterRegionMapSizeY();

	_water_region_data.clear();
	_water_region_data.resize(number_of_regions);

	_is_water_region_valid.clear();
	_is_water_region_valid.resize(number_of_regions, false);

	Debug(map, 2, "Allocating {} x {} water regions", GetWaterRegionMapSizeX(), GetWaterRegionMapSizeY());
	assert(_is_water_region_valid.size() == _water_region_data.size());
}

void PrintWaterRegionDebugInfo(TileIndex tile)
{
	GetUpdatedWaterRegion(tile).PrintDebugInfo();
}
