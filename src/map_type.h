/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_type.h Types related to maps. */

#ifndef MAP_TYPE_H
#define MAP_TYPE_H

/**
 * An offset value between two tiles.
 *
 * This value is used for the difference between
 * two tiles. It can be added to a TileIndex to get
 * the resulting TileIndex of the start tile applied
 * with this saved difference.
 *
 * @see TileDiffXY(int, int)
 */
typedef int32_t TileIndexDiff;

/**
 * A pair-construct of a TileIndexDiff.
 *
 * This can be used to save the difference between to
 * tiles as a pair of x and y value.
 */
struct TileIndexDiffC {
	int16_t x;        ///< The x value of the coordinate
	int16_t y;        ///< The y value of the coordinate
};

/** Minimal and maximal map width and height */
static const uint MIN_MAP_SIZE_BITS = 6;                       ///< Minimal size of map is equal to 2 ^ MIN_MAP_SIZE_BITS
static const uint MAX_MAP_SIZE_BITS = 12;                      ///< Maximal size of map is equal to 2 ^ MAX_MAP_SIZE_BITS
static const uint MIN_MAP_SIZE      = 1U << MIN_MAP_SIZE_BITS; ///< Minimal map size = 64
static const uint MAX_MAP_SIZE      = 1U << MAX_MAP_SIZE_BITS; ///< Maximal map size = 4096

static constexpr uint LOG_2_OF_TILES_PER_TILE_INDEX = 2; ///< TILES_PER_TILE_INDEX is a power of two. This is the power to wich 2 has to be raised.
static constexpr uint TILES_PER_TILE_INDEX = 1 << LOG_2_OF_TILES_PER_TILE_INDEX; ///< How many tiles can be under one TileIndex.
/* While TILES_PER_TILE_INDEX is less than 11 the uint8_t is more memory efficient than uint16_t.
 * That's because std::vector usually takes up 24 bytes.
 * To calculate this the following formula can be used, where: n is number of bits used to store offset and l is log 2 from number of tile indexes per map.
 * `8 * sizeof(std::vector<Map::TileBase>) * TILES_PER_TILE_INDEX * (1 << (l - n)) + (1 << l) * n`
 * @note The result on how many bits to use to store the offset is independent on chosen value of l. */
using MapOffsetType = uint8_t; ///< Type used for Map::offset.
static constexpr uint LOG_2_OF_TILE_INDEXES_PER_CHUNK = sizeof(MapOffsetType) * 8 - LOG_2_OF_TILES_PER_TILE_INDEX; ///< TILE_INDEXES_PER_CHUNK is a power of two. This is the power to wich 2 has to be raised.
static constexpr uint TILE_INDEXES_PER_CHUNK = 1U << LOG_2_OF_TILE_INDEXES_PER_CHUNK; ///< How many tile indexes fit into one chunk of map array.

/** Argument for CmdLevelLand describing what to do. */
enum LevelMode : uint8_t {
	LM_LEVEL, ///< Level the land.
	LM_LOWER, ///< Lower the land.
	LM_RAISE, ///< Raise the land.
};

#endif /* MAP_TYPE_H */
