/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file autoslope.h Functions related to autoslope. */

#ifndef AUTOSLOPE_H
#define AUTOSLOPE_H

#include "company_func.h"
#include "depot_func.h"
#include "slope_func.h"
#include "tile_map.h"

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
 * Tests if autoslope is enabled for _current_company.
 *
 * Autoslope is disabled for town/industry construction.
 *
 * @return true iff autoslope is enabled.
 */
static inline bool AutoslopeEnabled()
{
	return (_settings_game.construction.autoslope &&
	        (_current_company < MAX_COMPANIES ||
	         (_current_company == OWNER_NONE && _game_mode == GM_EDITOR)));
}

#endif /* AUTOSLOPE_H */
