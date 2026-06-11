/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file track_type.h All types related to tracks. */

#ifndef TRACK_TYPE_H
#define TRACK_TYPE_H

#include "core/enum_type.hpp"

/**
 * These are used to specify a single track.
 * Can be translated to a trackbit with TrackToTrackbit
 */
enum class Track : uint8_t {
	Begin = 0, ///< Used for iterations
	X = 0, ///< Track along the x-axis (north-east to south-west)
	Y = 1, ///< Track along the y-axis (north-west to south-east)
	Upper = 2, ///< Track in the upper corner of the tile (north)
	Lower = 3, ///< Track in the lower corner of the tile (south)
	Left = 4, ///< Track in the left corner of the tile (west)
	Right = 5, ///< Track in the right corner of the tile (east)
	End, ///< End marker (of regular track bits)

	Wormhole = 6, ///< Special flag indicating vehicle is inside a bridge or tunnel.
	Depot = 7, ///< Special flag indicating a vehicle is inside a depot.

	Invalid = 0xFF, ///< Flag for an invalid track
};

/**
 * Array with \c Track as index.
 * @tparam T the type contained within the array.
 */
template <typename T>
using TrackIndexArray = EnumIndexArray<T, Track, Track::End>;

/** Bitset of \c Track elements. */
using TrackBits = EnumBitSet<Track, uint8_t>;

static constexpr TrackBits TRACK_BIT_CROSS = {Track::X, Track::Y}; ///< X-Y-axis cross
static constexpr TrackBits TRACK_BIT_HORZ = {Track::Upper, Track::Lower}; ///< Upper and lower track
static constexpr TrackBits TRACK_BIT_VERT = {Track::Left, Track::Right}; ///< Left and right track
static constexpr TrackBits TRACK_BIT_3WAY_NE = {Track::X, Track::Upper, Track::Right}; ///< "Arrow" to the north-east
static constexpr TrackBits TRACK_BIT_3WAY_SE = {Track::Y, Track::Lower, Track::Right}; ///< "Arrow" to the south-east
static constexpr TrackBits TRACK_BIT_3WAY_SW = {Track::X, Track::Lower, Track::Left}; ///< "Arrow" to the south-west
static constexpr TrackBits TRACK_BIT_3WAY_NW = {Track::Y, Track::Upper, Track::Left}; ///< "Arrow" to the north-west
static constexpr TrackBits TRACK_BIT_ALL = {Track::X, Track::Y, Track::Upper, Track::Lower, Track::Left, Track::Right}; ///< All possible tracks

/**
 * Enumeration for tracks and directions.
 *
 * These are a combination of tracks and directions. Values are 0-5 in one
 * direction (corresponding to the Track enum) and 8-13 in the other direction.
 * 6, 7, 14 and 15 are used to encode the reversing of road vehicles. Those
 * reversing track dirs are not considered to be 'valid' except in a small
 * corner in the road vehicle controller.
 */
enum class Trackdir : uint8_t {
	X_NE = 0, ///< X-axis and direction to north-east
	Y_SE = 1, ///< Y-axis and direction to south-east
	Upper_E = 2, ///< Upper track and direction to east
	Lower_E = 3, ///< Lower track and direction to east
	Left_S = 4, ///< Left track and direction to south
	Right_S = 5, ///< Right track and direction to south
	RvRev_NE = 6, ///< (Road vehicle) reverse direction north-east
	RvRev_SE = 7, ///< (Road vehicle) reverse direction south-east
	X_SW = 8, ///< X-axis and direction to south-west
	Y_NW = 9, ///< Y-axis and direction to north-west
	Upper_W = 10, ///< Upper track and direction to west
	Lower_W = 11, ///< Lower track and direction to west
	Left_N = 12, ///< Left track and direction to north
	Right_N = 13, ///< Right track and direction to north
	RvRev_SW = 14, ///< (Road vehicle) reverse direction south-west
	RvRev_NW = 15, ///< (Road vehicle) reverse direction north-west

	End, ///< End marker
	Invalid = 0xFF, ///< Flag for an invalid trackdir
};

/**
 * Array with \c Trackdir as index.
 * @tparam T the type contained within the array.
 */
template <typename T>
using TrackdirIndexArray = EnumIndexArray<T, Trackdir, Trackdir::End>;

/** Bitset of \c Trackdir elements. */
using TrackdirBits = EnumBitSet<Trackdir, uint16_t>;

/** Bitset of valid non-road vehicle trackdirs/ */
static constexpr TrackdirBits TRACKDIR_BIT_MASK{
	Trackdir::X_NE, Trackdir::Y_SE, Trackdir::Upper_E, Trackdir::Lower_E, Trackdir::Left_S, Trackdir::Right_S,
	Trackdir::X_SW, Trackdir::Y_NW, Trackdir::Upper_W, Trackdir::Lower_W, Trackdir::Left_N, Trackdir::Right_N,
};

/** Marker for an invalid TrackdirBits value. */
static constexpr TrackdirBits INVALID_TRACKDIR_BIT{UINT16_MAX};

/** Track status of a tile. */
struct TrackStatus {
	TrackdirBits trackdirs; ///< Trackdirs present on the tile.
	TrackdirBits signals; ///< Red signals on the tile.
};

#endif /* TRACK_TYPE_H */
