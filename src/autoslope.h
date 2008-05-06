/* $Id$ */

/** @file autoslope.h Functions related to autoslope. */

#ifndef AUTOSLOPE_H
#define AUTOSLOPE_H

#include "settings_type.h"
#include "player_func.h"
#include "depot_func.h"

/**
 * Autoslope check for tiles with an entrance on an edge.
 * E.g. depots and non-drive-through-road-stops.
 *
 * The test succeeds if the slope is not steep and at least one corner of the entrance edge is on the TileMaxZ() level.
 *
 * @note The test does not check if autoslope is enabled at all.
 *
 * @param tile The tile.
 * @param z_new New TileZ.
 * @param tileh_new New TileSlope.
 * @param entrance Entrance edge.
 * @return true iff terraforming is allowed.
 */
static inline bool AutoslopeCheckForEntranceEdge(TileIndex tile, uint z_new, Slope tileh_new, DiagDirection entrance)
{
	if (IsSteepSlope(tileh_new) || (GetTileMaxZ(tile) != z_new + GetSlopeMaxZ(tileh_new))) return false;
	return ((tileh_new == SLOPE_FLAT) || CanBuildDepotByTileh(entrance, tileh_new));
}

/**
 * Tests if autoslope is enabled for _current_player.
 *
 * Autoslope is disabled for town/industry construction and old ai players.
 *
 * @return true iff autoslope is enabled.
 */
static inline bool AutoslopeEnabled()
{
	return (_patches.autoslope &&
	        ((IsValidPlayer(_current_player) && !_is_old_ai_player) ||
	         (_current_player == OWNER_NONE && _game_mode == GM_EDITOR)));
}

#endif /* AUTOSLOPE_H */
