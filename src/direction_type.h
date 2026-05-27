/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file direction_type.h Different types to 'show' directions. */

#ifndef DIRECTION_TYPE_H
#define DIRECTION_TYPE_H

#include "core/enum_type.hpp"

/**
 * Defines the 8 directions on the map.
 *
 * This enum defines 8 possible directions which are used for
 * the vehicles in the game. The directions are aligned straight
 * to the viewport, not to the map. So north points to the top of
 * your viewport and not rotated by 45 degrees left or right to get
 * a "north" used in you games.
 */
enum class Direction : uint8_t {
	Begin = 0, ///< Used to iterate
	N = 0, ///< North
	NE = 1, ///< Northeast
	E = 2, ///< East
	SE = 3, ///< Southeast
	S = 4, ///< South
	SW = 5, ///< Southwest
	W = 6, ///< West
	NW = 7, ///< Northwest
	End, ///< Used to iterate
	Invalid = 0xFF, ///< Flag for an invalid direction
};

/** Allow incrementing of Direction variables */
DECLARE_INCREMENT_DECREMENT_OPERATORS(Direction)

using Directions = EnumBitSet<Direction, uint8_t>;

/** All possible directions. */
static constexpr Directions DIRECTIONS_ALL{Direction::N, Direction::NE, Direction::E, Direction::SE, Direction::S, Direction::SW, Direction::W, Direction::NW};

/**
 * Array with \c Direction as index.
 * @tparam T the type contained within the array.
 */
template <typename T>
using DirectionIndexArray = EnumIndexArray<T, Direction, Direction::End>;

/**
 * Enumeration for the difference between two directions.
 *
 * This enumeration is used to mark differences between
 * two directions. If you get one direction you can align
 * a second direction in 8 different ways. This enumeration
 * only contains 6 of these 8 differences, but the remaining
 * two can be calculated by adding to differences together.
 * This also means you can add two differences together and
 * get the difference you really want to get. The difference
 * of 45 degrees left + the difference of 45 degrees right results in the
 * difference of 0 degrees.
 *
 * @note To get this mentioned addition of direction you must use
 *       modulo Direction::End or use the #ChangeDirDiff(DirDiff, DirDiff) function.
 * @see ChangeDirDiff(DirDiff, DirDiff)
 */
enum class DirDiff : uint8_t {
	Same = 0, ///< Both directions faces to the same direction
	Right45 = 1, ///< Angle of 45 degrees right
	Right90 = 2, ///< Angle of 90 degrees right
	Reverse = 4, ///< One direction is the opposite of the other one
	Left90 = 6, ///< Angle of 90 degrees left
	Left45 = 7, ///< Angle of 45 degrees left
};


/**
 * Enumeration for diagonal directions.
 *
 * This enumeration is used for the 4 direction of the tile-edges.
 */
enum class DiagDirection : uint8_t {
	Begin = 0, ///< Used for iterations
	NE = 0, ///< Northeast, upper right on your monitor
	SE = 1, ///< Southeast
	SW = 2, ///< Southwest
	NW = 3, ///< Northwest
	End, ///< Used for iterations
	Invalid = 0xFF, ///< Flag for an invalid DiagDirection
};
DECLARE_INCREMENT_DECREMENT_OPERATORS(DiagDirection)
DECLARE_ENUM_AS_ADDABLE(DiagDirection)

/** Bitset of \c DiagDirection elements. */
using DiagDirections = EnumBitSet<DiagDirection, uint8_t>;

/** All possible diagonal directions. */
static constexpr DiagDirections DIAGDIRECTIONS_ALL{DiagDirection::NE, DiagDirection::SE, DiagDirection::SW, DiagDirection::NW};

/**
 * Array with \c DiagDirection as index.
 * @tparam T the type contained within the array.
 */
template <typename T>
using DiagDirectionIndexArray = EnumIndexArray<T, DiagDirection, DiagDirection::End>;

/**
 * Enumeration for the difference between to DiagDirection.
 *
 * As the DiagDirection only contains 4 possible directions the
 * difference between two of these directions can only be in 4 ways.
 * As the DirDiff enumeration the values can be added together and
 * you will get the resulting difference (use modulo DiagDirection::End).
 *
 * @see DirDiff
 */
enum class DiagDirDiff : uint8_t {
	Same = 0, ///< Same directions
	Right90 = 1, ///< 90 degrees right
	Reverse = 2, ///< Reverse directions
	Left90 = 3, ///< 90 degrees left
};

/**
 * Enumeration for the two axis X and Y
 *
 * This enumeration represents the two axis X and Y in the game.
 * The X axis is the one which goes align the north-west edge
 * (and south-east edge). The Y axis must be so the one which goes
 * align the north-east edge (and south-west) edge.
 */
enum class Axis : uint8_t {
	X = 0, ///< The X axis
	Y = 1, ///< The y axis
	End, ///< End marker.
	Invalid = 0xFF, ///< Flag for an invalid Axis
};
DECLARE_ENUM_AS_ADDABLE(Axis)

/**
 * Array with \c Axis as index.
 * @tparam T the type contained within the array.
 */
template <typename T>
using AxisIndexArray = EnumIndexArray<T, Axis, Axis::End>;

#endif /* DIRECTION_TYPE_H */
