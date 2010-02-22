/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_airporttiles.cpp NewGRF handling of airport tiles. */

#include "stdafx.h"
#include "airport.h"
#include "newgrf.h"
#include "newgrf_airporttiles.h"
#include "table/airporttiles.h"


AirportTileSpec AirportTileSpec::tiles[NUM_AIRPORTTILES];

AirportTileOverrideManager _airporttile_mngr(NEW_AIRPORTTILE_OFFSET, NUM_AIRPORTTILES, INVALID_AIRPORTTILE);

/**
 * Retrieve airport tile spec for the given airport tile
 * @param gfx index of airport tile
 * @return A pointer to the corresponding AirportTileSpec
 */
/* static */ const AirportTileSpec *AirportTileSpec::Get(StationGfx gfx)
{
	assert(gfx < lengthof(AirportTileSpec::tiles));
	return &AirportTileSpec::tiles[gfx];
}

/**
 * This function initializes the tile array of AirportTileSpec
 */
void AirportTileSpec::ResetAirportTiles()
{
	memset(&AirportTileSpec::tiles, 0, sizeof(AirportTileSpec::tiles));
	memcpy(&AirportTileSpec::tiles, &_origin_airporttile_specs, sizeof(_origin_airporttile_specs));

	/* Reset any overrides that have been set. */
	_airporttile_mngr.ResetOverride();
}

void AirportTileOverrideManager::SetEntitySpec(const AirportTileSpec *airpts)
{
	StationGfx airpt_id = this->AddEntityID(airpts->grf_prop.local_id, airpts->grf_prop.grffile->grfid, airpts->grf_prop.subst_id);

	if (airpt_id == invalid_ID) {
		grfmsg(1, "AirportTile.SetEntitySpec: Too many airport tiles allocated. Ignoring.");
		return;
	}

	memcpy(&AirportTileSpec::tiles[airpt_id], airpts, sizeof(*airpts));

	/* Now add the overrides. */
	for (int i = 0; i < max_offset; i++) {
		AirportTileSpec *overridden_airpts = &AirportTileSpec::tiles[i];

		if (entity_overrides[i] != airpts->grf_prop.local_id || grfid_overrides[i] != airpts->grf_prop.grffile->grfid) continue;

		overridden_airpts->grf_prop.override = airpt_id;
		overridden_airpts->enabled = false;
		entity_overrides[i] = invalid_ID;
		grfid_overrides[i] = 0;
	}
}

