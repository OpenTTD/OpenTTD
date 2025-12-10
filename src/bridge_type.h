/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file bridge_type.h Header file for bridge types. */

#ifndef BRIDGE_TYPE_H
#define BRIDGE_TYPE_H

#include "core/enum_type.hpp"

using BridgeType = uint; ///< Bridge spec number.

/**
 * This enum is related to the definition of bridge pieces,
 * which is used to determine the proper sprite table to use
 * while drawing a given bridge part.
 */
enum BridgePieces : uint8_t {
	BRIDGE_PIECE_NORTH = 0,
	BRIDGE_PIECE_SOUTH,
	BRIDGE_PIECE_INNER_NORTH,
	BRIDGE_PIECE_INNER_SOUTH,
	BRIDGE_PIECE_MIDDLE_ODD,
	BRIDGE_PIECE_MIDDLE_EVEN,
	BRIDGE_PIECE_HEAD,
	NUM_BRIDGE_PIECES,
};

DECLARE_INCREMENT_DECREMENT_OPERATORS(BridgePieces)

/** Number of bridge middles pieces. This is all bridge pieces except BRIDGE_PIECE_HEAD. */
static constexpr uint NUM_BRIDGE_MIDDLE_PIECES = NUM_BRIDGE_PIECES - 1;

/** Obstructed bridge pillars information. */
enum class BridgePillarFlag : uint8_t {
	/* Corners are in the same order as Corner enum. */
	CornerW = 0, ///< West corner is obstructed.
	CornerS = 1, ///< South corner is obstructed.
	CornerE = 2, ///< East corner is obstructed.
	CornerN = 3, ///< North corner is obstructed.
	/* Edges are in the same order as DiagDirection enum. */
	EdgeNE = 4, //< Northeast edge is obstructed.
	EdgeSE = 5, //< Southeast edge is obstructed.
	EdgeSW = 6, //< Southwest edge is obstructed.
	EdgeNW = 7, //< Northwest edge is obstructed.
};
using BridgePillarFlags = EnumBitSet<BridgePillarFlag, uint8_t>;

static constexpr BridgePillarFlags BRIDGEPILLARFLAGS_ALL{
	BridgePillarFlag::CornerW, BridgePillarFlag::CornerS, BridgePillarFlag::CornerE, BridgePillarFlag::CornerN,
	BridgePillarFlag::EdgeNE, BridgePillarFlag::EdgeSE, BridgePillarFlag::EdgeSW, BridgePillarFlag::EdgeNW,
};

/** Information about a tile structure that may have a bridge above. */
struct BridgeableTileInfo {
	uint8_t height = 0; ///< Minimum height for a bridge above. 0 means a bridge is not allowed.
	BridgePillarFlags disallowed_pillars = BRIDGEPILLARFLAGS_ALL; ///< Disallowed pillar flags for a bridge above
};

#endif /* BRIDGE_TYPE_H */
