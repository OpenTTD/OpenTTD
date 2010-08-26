/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_airporttiles.h NewGRF handling of airport tiles. */

#ifndef NEWGRF_AIRPORTTILES_H
#define NEWGRF_AIRPORTTILES_H

#include "airport.h"
#include "station_map.h"
#include "newgrf_animation_type.h"
#include "newgrf_commons.h"

/**
 * Defines the data structure of each indivudual tile of an airport.
 */
struct AirportTileSpec {
	AnimationInfo animation;              ///< Information about the animation.
	StringID name;                        ///< Tile Subname string, land information on this tile will give you "AirportName (TileSubname)"
	uint8 callback_mask;                  ///< Bitmask telling which grf callback is set
	uint8 animation_special_flags;        ///< Extra flags to influence the animation
	bool enabled;                         ///< entity still available (by default true). newgrf can disable it, though
	GRFFileProps grf_prop;                ///< properties related the the grf file

	static const AirportTileSpec *Get(StationGfx gfx);
	static const AirportTileSpec *GetByTile(TileIndex tile);

	static void ResetAirportTiles();

private:
	static AirportTileSpec tiles[NUM_AIRPORTTILES];

	friend void AirportTileOverrideManager::SetEntitySpec(const AirportTileSpec *airpts);
};

StationGfx GetTranslatedAirportTileID(StationGfx gfx);
void AnimateAirportTile(TileIndex tile);
void AirportTileAnimationTrigger(Station *st, TileIndex tile, AirpAnimationTrigger trigger, CargoID cargo_type = CT_INVALID);
void AirportAnimationTrigger(Station *st, AirpAnimationTrigger trigger, CargoID cargo_type = CT_INVALID);
bool DrawNewAirportTile(TileInfo *ti, Station *st, StationGfx gfx, const AirportTileSpec *airts);

#endif /* NEWGRF_AIRPORTTILES_H */
