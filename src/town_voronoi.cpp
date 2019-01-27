/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file town_voronoi.cpp Handling of town Voronoi diagram, containing the TownID of the closest town for every tile. */

#include "stdafx.h"
#include "town.h"
#include "sortlist_type.h"
#include <algorithm>
#include <vector>

bool _voronoi_initialized;

typedef std::vector<const Town*> TownList;

/** Sort by town index */
static bool TownXYSorter(const Town* town_a, const Town* town_b)
{
	return (town_a->xy < town_b->xy);
}

/**
 * Gets the index of the town closest to the given tile.
 *
 * @param tile tile index of the tile in question
 * @return index of the closest town
 */
static TownID GetClosestTown(TileIndex tile)
{
	assert(tile < MapSize());
	return _closest_town[tile];
}

/**
 * Sets the index of the town closest to the given tile.
 *
 * @param tile tile index of the tile in question
 * @param index town index of the closest town to tile
 * @return index of the closest town
 */
static void SetClosestTown(TileIndex tile, TownID index)
{
	assert(tile < MapSize());
	_closest_town[tile] = index;
}

void UninitializeVoronoi() {
	_voronoi_initialized = false;
}

/**
 * Calculates the maximum distance where the tiles between town A and B
 * are closer to town A than town B. If there are tiles with equal distance
 * from town A and B, the tile belongs to the town with the smaller TownID (index).
 *
 * @param town_a pointer of town A
 * @param town_b pointer of town B
 * @return the half distance between town A and B
 */
static uint CalcHalfDistance(const Town* town_a, const Town* town_b)
{
	uint distance = DistanceManhattan(town_a->xy, town_b->xy);

	assert(distance != 0);

	if (distance % 2 == 1) return (distance -1 ) / 2;

	// There are tiles that has the same distance form the two towns
	// The dominant town (with lower index) gets the bigger part
	return (town_a->index < town_b->index) ? distance / 2 : distance / 2 - 1;
}

/**
 * Checks whether town A is closer to given tile than previously closes town (town B)
 * In case of equal distances, the town with the lower TownID (index) is considered the closer one.
 *
 * @param town_a pointer to the competing town
 * @param tile tile index of the tile in question
 * @return true if town A is closer
 */
static bool IsCloserTown(const Town* town_a, TileIndex tile)
{
	Town * town_b = Town::Get(GetClosestTown(tile));

	if (DistanceManhattan(town_a->xy, tile) < DistanceManhattan(town_b->xy, tile)) return true;

	if (DistanceManhattan(town_a->xy, tile) == DistanceManhattan(town_b->xy, tile)) {
		if (town_a->index < town_b->index) return true;
	}

	return false;
}

/**
 * From a given tile, finds the first tile (while moving in the X direction) that is
 * closer to town A than town B. At least one tile must be closer to town A than
 * town B in the given 'Y map slice' for the function to work.
 *
 * @param town_a pointer of town A
 * @param town_b pointer of town B
 * @param limit tile index of the starting tile
 * @return X coordinate of the found tile
 */
static uint FindTownExtentXatYwithOtherTown(const Town* town_a, const Town* town_b, TileIndex limit)
{
	assert(IsCloserTown(town_a, TileXY(TileX(town_a->xy), TileY(limit))));

	/* To understand the different cases below some drawing is required.
	 * Apart from #2, every case describes a different relative position for the two towns.
	 * The case when town A and B is in the opposite corners of a square
	 * is handled in two parts, in #5 and in #6. */

	if (TileX(town_a->xy) == TileX(limit)) return TileX(limit);

	/*  #1: Town A and town B have the same X coordinate. */
	if (TileX(town_a->xy) == TileX(town_b->xy)) return TileX(limit);

	bool ascending = (TileX(town_a->xy) < TileX(limit));

	/* #2: Tile "limit" and town B are on different sides of town A in the X direction.
	 *     Every such tile is closer to town A than town B. */
	if ((TileX(town_a->xy) > TileX(town_b->xy)) == ascending) return TileX(limit);

	uint half_distance = CalcHalfDistance(town_a, town_b);

	/* #3: Town A and town B have the same Y coordinate. */

	if (TileY(town_a->xy) == TileY(town_b->xy)) return ascending ? TileX(town_a->xy) + half_distance : TileX(town_a->xy) - half_distance;

	uint delta_towns_x = Delta(TileX(town_a->xy), TileX(town_b->xy));
	uint delta_y = Delta(TileY(town_a->xy), TileY(limit));

	/* #4: Tile "limit" and town B are on different sides of town A in the Y direction.
	       #5 and #6 work here as well delta_y is considered 0. */
	if ((TileY(town_b->xy) > TileY(town_a->xy)) == (TileY(town_a->xy) > TileY(limit))) delta_y = 0;

	/* #5: Town A and B are closer to each other in the X direction than in the Y direction,
	 *     or the X and Y distances are the same, but town A has a smaller TownID.
	 *     While the relation below is true, all the tiles with the given Y coordinate
	 *     are closer to town A than town B. */
	if (half_distance >= delta_towns_x + delta_y) return TileX(limit);

	uint delta_towns_y = Delta(TileY(town_a->xy), TileY(town_b->xy));

	/* After #5, this shouldn't be negative. */
	uint delta_x = half_distance - min(delta_y, delta_towns_y);

	/* #6: Town placements as in #4, but while the above relation is false, and for placements
	 *     where town A and B are closer to each other in the Y direction than in the X direction,
	 *     or the X and Y distances are the same, but town A has a smaller TownID. */
	return ascending ? TileX(town_a->xy) + delta_x : TileX(town_a->xy) - delta_x;
}

/**
 * Finds the upper or the lower extent of town A, where the tiles with the
 * given Y coordinates between town A and the extent tile are all closer to
 * town A than any other (already processed) town.
 *
 * @param town_a pointer of town A
 * @param y Y coordinate of the selected map slice
 * @param x_extent the X coordinate of tile where the search starts
 * @return X coordinate of the new extent tile
 */
static uint FindTownExtentXatY(const Town* town_a, uint y, uint x_extent)
{
	Town* town_b;
	TileIndex extent_tile = TileXY(x_extent, y);

	do {
		town_b = Town::Get(GetClosestTown(extent_tile));
		extent_tile = TileXY(FindTownExtentXatYwithOtherTown(town_a, town_b, extent_tile), y);
	} while (town_b != Town::Get(GetClosestTown(extent_tile)));

	return TileX(extent_tile);
}

/**
 * Fills a continuous line between two tiles with the same Y coordinate
 * with a town index in the town Voronoi diagram array.
 *
 * @param y Y coordinate of the chosen map slice
 * @param x_start X coordinate of the first tile to fill
 * @param x_end X coordinate of the last tile to fill
 * @param index town index of the chosen town
 */
static void FillLinePartWithIndex(uint y, uint x_start, uint x_end, TownID index)
{
	TileIndexDiff tile_step = TileDiffXY(1, 0);
	TileIndex start_tile = TileXY(x_start, y);
	TileIndex end_tile = TileXY(x_end, y);

	for (TileIndex t = start_tile; t <= end_tile; t += tile_step) SetClosestTown(t, index);
}

/**
 * Fills the town Voronoi diagram where the given town is the closest among the towns already
 * added to the diagram until the line with the Y coordinate same at the given town.
 * The actual filling starts from the given town going towards the Y=0 line.
 *
 * @param town pointer to the town we are adding to the diagram
 * @param bakwards true if direction is toward smaller Y coordinates
 */
static void FillTownTilesInDirection(const Town* town, bool backwards)
{
	uint ascending_x_extent = MapMaxX();
	uint descending_x_extent = 0;

	for (uint y = (backwards ? TileY(town->xy) : TileY(town->xy) + 1); y < MapSizeY(); backwards ? y-- : y++){
		TileIndex tile = TileXY(TileX(town->xy), y);

		if (!IsCloserTown(town, tile)) return;
		ascending_x_extent = FindTownExtentXatY(town, y, ascending_x_extent);
		descending_x_extent = FindTownExtentXatY(town, y, descending_x_extent);

		FillLinePartWithIndex(y, descending_x_extent, ascending_x_extent, town->index);
	}
}

/**
 * Fills a line in the diagram given a Y coordinate with the data found
 * in the previous  line (with Y coordinate of y - 1).
 * @param y Y coordinate of the line in the map we want to fill
 */
static void CopyFromPreviousLine(uint y)
{
	TileIndexDiff step_x = TileDiffXY(1, 0);
	TileIndexDiff step_y = TileDiffXY(0, 1);
	TileIndex end_tile = TileXY(0, y + 1);

	for (TileIndex t = TileXY(0, y); t != end_tile; t += step_x) {
		SetClosestTown(t, GetClosestTown(t - step_y));
	}
}

static void FillAndSortTownList(TownList* towns)
{
	const Town *t;
	uint i = 0;

	FOR_ALL_TOWNS(t) {
		(*towns)[i++] = t;
	}

	std::sort(towns->begin(), towns->end(), TownXYSorter);
}

void InitializeDiagramWithValue(TownID index)
{
	for (TileIndex t = 0; t < MapSize(); t++) {
		SetClosestTown(t, index);
	}
}

/**
 * Builds town Voronoi diagram. Overwrites previously built diagram if any.
 */
void BuildVoronoiDiagram()
{
	if (Town::GetNumItems() == 0) {
		_voronoi_initialized = false;
		return;
	}

	TownList towns(Town::GetNumItems());
	FillAndSortTownList(&towns);

	_voronoi_initialized = true;

	if (towns.size() == 1) {
		InitializeDiagramWithValue(towns[0]->index);
		return;
	}

	// Fill the first few lines as closest to towns[0]
	for (uint y = 0; y <= TileY(towns[1]->xy); y++) FillLinePartWithIndex(y, 0, MapMaxX(), towns[0]->index);

	// Fill the lines from the second town until the last town
	for (uint i = 1; i < towns.size(); i++) {
		for (uint y = TileY(towns[i - 1]->xy) + 1; y <= TileY(towns[i]->xy); y++) {
			CopyFromPreviousLine(y);
		}
		FillTownTilesInDirection(towns[i], true);
	}

	// Fill the remaining lines after the last town
	for (uint y = TileY(towns[towns.size() - 1]->xy) + 1; y < MapSizeY(); y++) {
		CopyFromPreviousLine(y);
	}
}

void AddTownToVoronoi(const Town* t)
{
	if (!_voronoi_initialized) return;

	FillTownTilesInDirection(t, true);
	FillTownTilesInDirection(t, false);
}

/**
 * Gets the index of the town closest to the given tile.
 *
 * @param tile tile index of the tile in question
 * @return index of the closest town
 */
TownID GetClosestTownFromTile(TileIndex tile)
{
	if (!_voronoi_initialized) {
		BuildVoronoiDiagram();
		if (Town::GetNumItems() == 0) return INVALID_TOWN;
	}

	return GetClosestTown(tile);
}
