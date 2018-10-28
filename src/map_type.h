/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file map_type.h Types related to maps. */

#ifndef MAP_TYPE_H
#define MAP_TYPE_H

/**
 * Data that is stored per tile. Also used TileExtended for this.
 * Look at docs/landscape.html for the exact meaning of the members.
 */
struct Tile {
	byte   type;        ///< The type (bits 4..7), bridges (2..3), rainforest/desert (0..1)
	byte   height;      ///< The height of the northern corner.
	uint16 m2;          ///< Primarily used for indices to towns, industries and stations
	byte   m1;          ///< Primarily used for ownership information
	byte   m3;          ///< General purpose
	byte   m4;          ///< General purpose
	byte   m5;          ///< General purpose
};

assert_compile(sizeof(Tile) == 8);

/**
 * Data that is stored per tile. Also used Tile for this.
 * Look at docs/landscape.html for the exact meaning of the members.
 */
struct TileExtended {
	byte m6;   ///< General purpose
	byte m7;   ///< Primarily used for newgrf support
	uint16 m8; ///< General purpose
};

/**
 * An offset value between to tiles.
 *
 * This value is used for the difference between
 * to tiles. It can be added to a tileindex to get
 * the resulting tileindex of the start tile applied
 * with this saved difference.
 *
 * @see TileDiffXY(int, int)
 */
typedef int32 TileIndexDiff;

/**
 * A pair-construct of a TileIndexDiff.
 *
 * This can be used to save the difference between to
 * tiles as a pair of x and y value.
 */
struct TileIndexDiffC {
	int16 x;        ///< The x value of the coordinate
	int16 y;        ///< The y value of the coordinate
};

/** Minimal and maximal map width and height */
static const uint MIN_MAP_SIZE_BITS = 6;                      ///< Minimal size of map is equal to 2 ^ MIN_MAP_SIZE_BITS
static const uint MAX_MAP_SIZE_BITS = 12;                     ///< Maximal size of map is equal to 2 ^ MAX_MAP_SIZE_BITS
static const uint MIN_MAP_SIZE      = 1 << MIN_MAP_SIZE_BITS; ///< Minimal map size = 64
static const uint MAX_MAP_SIZE      = 1 << MAX_MAP_SIZE_BITS; ///< Maximal map size = 4096

/**
 * Approximation of the length of a straight track, relative to a diagonal
 * track (ie the size of a tile side).
 *
 * \#defined instead of const so it can
 * stay integer. (no runtime float operations) Is this needed?
 * Watch out! There are _no_ brackets around here, to prevent intermediate
 * rounding! Be careful when using this!
 * This value should be sqrt(2)/2 ~ 0.7071
 */
#define STRAIGHT_TRACK_LENGTH 7071/10000

/** Argument for CmdLevelLand describing what to do. */
enum LevelMode {
	LM_LEVEL, ///< Level the land.
	LM_LOWER, ///< Lower the land.
	LM_RAISE, ///< Raise the land.
};

#endif /* MAP_TYPE_H */
