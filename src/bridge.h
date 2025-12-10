/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file bridge.h Header file for bridges */

#ifndef BRIDGE_H
#define BRIDGE_H

#include "gfx_type.h"
#include "tile_cmd.h"
#include "timer/timer_game_calendar.h"
#include "bridge_type.h"

static const uint MAX_BRIDGES = 13; ///< Maximal number of available bridge specs.
constexpr uint SPRITES_PER_BRIDGE_PIECE = 32; ///< Number of sprites there are per bridge piece.

/* Container for Bridge pillar flags for each axis of each bridge middle piece. */
using BridgeMiddlePillarFlags = std::array<std::array<BridgePillarFlags, AXIS_END>, NUM_BRIDGE_MIDDLE_PIECES>;

/**
 * Struct containing information about a single bridge type
 */
struct BridgeSpec {
	/** Internal flags about each BridgeSpec. */
	enum class ControlFlag : uint8_t {
		CustomPillarFlags, ///< Bridge has set custom pillar flags.
		InvalidPillarFlags, ///< Bridge pillar flags are not valid, i.e. only the tile layout has been modified.
	};
	using ControlFlags = EnumBitSet<ControlFlag, uint8_t>;

	TimerGameCalendar::Year avail_year; ///< the year where it becomes available
	uint8_t min_length;                    ///< the minimum length (not counting start and end tile)
	uint16_t max_length;                  ///< the maximum length (not counting start and end tile)
	uint16_t price;                       ///< the price multiplier
	uint16_t speed;                       ///< maximum travel speed (1 unit = 1/1.6 mph = 1 km-ish/h)
	SpriteID sprite;                    ///< the sprite which is used in the GUI
	PaletteID pal;                      ///< the palette which is used in the GUI
	StringID material;                  ///< the string that contains the bridge description
	StringID transport_name[2];         ///< description of the bridge, when built for road or rail
	std::vector<std::vector<PalSpriteID>> sprite_table; ///< table of sprites for drawing the bridge
	uint8_t flags;                         ///< bit 0 set: disable drawing of far pillars.
	ControlFlags ctrl_flags{}; ///< control flags
	BridgeMiddlePillarFlags pillar_flags{}; ///< bridge pillar flags.
};

extern BridgeSpec _bridge[MAX_BRIDGES];

Foundation GetBridgeFoundation(Slope tileh, Axis axis);
bool HasBridgeFlatRamp(Slope tileh, Axis axis);

/**
 * Get the specification of a bridge type.
 * @param i The type of bridge to get the specification for.
 * @return The specification.
 */
inline const BridgeSpec *GetBridgeSpec(BridgeType i)
{
	assert(i < lengthof(_bridge));
	return &_bridge[i];
}

void DrawBridgeMiddle(const TileInfo *ti, BridgePillarFlags blocked_pillars);

CommandCost CheckBridgeAvailability(BridgeType bridge_type, uint bridge_len, DoCommandFlags flags = {});
int CalcBridgeLenCostFactor(int x);

void ResetBridges();

#endif /* BRIDGE_H */
