/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
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

enum class BridgePillarFlag : uint8_t {
	CornerW = 0,
	CornerS = 1,
	CornerE = 2,
	CornerN = 3,
	EdgeNE = 4,
	EdgeSE = 5,
	EdgeSW = 6,
	EdgeNW = 7,
};
using BridgePillarFlags = EnumBitSet<BridgePillarFlag, uint8_t>;

static constexpr BridgePillarFlags BRIDGEPILLARFLAGS_ALL{
	BridgePillarFlag::CornerW, BridgePillarFlag::CornerS, BridgePillarFlag::CornerE, BridgePillarFlag::CornerN,
	BridgePillarFlag::EdgeNE, BridgePillarFlag::EdgeSE, BridgePillarFlag::EdgeSW, BridgePillarFlag::EdgeNW,
};

#endif /* BRIDGE_TYPE_H */
