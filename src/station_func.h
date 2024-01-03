/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file station_func.h Functions related to stations. */

#ifndef STATION_FUNC_H
#define STATION_FUNC_H

#include "sprite.h"
#include "rail_type.h"
#include "road_type.h"
#include "vehicle_type.h"
#include "economy_func.h"
#include "rail.h"
#include "road.h"
#include "linkgraph/linkgraph_type.h"
#include "industry_type.h"

void ModifyStationRatingAround(TileIndex tile, Owner owner, int amount, uint radius);

void ShowStationViewWindow(StationID station);
void UpdateAllStationVirtCoords();
void ClearAllStationCachedNames();

CargoArray GetProductionAroundTiles(TileIndex tile, int w, int h, int rad);
CargoArray GetAcceptanceAroundTiles(TileIndex tile, int w, int h, int rad, CargoTypes *always_accepted = nullptr);

void UpdateStationAcceptance(Station *st, bool show_msg);
CargoTypes GetAcceptanceMask(const Station *st);
CargoTypes GetEmptyMask(const Station *st);

const DrawTileSprites *GetStationTileLayout(StationType st, byte gfx);
void StationPickerDrawSprite(int x, int y, StationType st, RailType railtype, RoadType roadtype, int image);

bool HasStationInUse(StationID station, bool include_company, CompanyID company);

void DeleteOilRig(TileIndex t);
void UpdateStationDockingTiles(Station *st);
void RemoveDockingTile(TileIndex t);
void ClearDockingTilesCheckingNeighbours(TileIndex tile);

void UpdateAirportsNoise();

bool SplitGroundSpriteForOverlay(const TileInfo *ti, SpriteID *ground, RailTrackOffset *overlay_offset);

void IncreaseStats(Station *st, const Vehicle *v, StationID next_station_id, uint32_t time);
void IncreaseStats(Station *st, CargoID cargo, StationID next_station_id, uint capacity, uint usage, uint32_t time, EdgeUpdateMode mode);
void RerouteCargo(Station *st, CargoID c, StationID avoid, StationID avoid2);

/**
 * Calculates the maintenance cost of a number of station tiles.
 * @param num Number of station tiles.
 * @return Total cost.
 */
static inline Money StationMaintenanceCost(uint32_t num)
{
	return (_price[PR_INFRASTRUCTURE_STATION] * num * (1 + IntSqrt(num))) >> 7; // 7 bits scaling.
}

Money AirportMaintenanceCost(Owner owner);

#endif /* STATION_FUNC_H */
