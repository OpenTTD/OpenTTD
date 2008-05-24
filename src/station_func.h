/* $Id$ */

/** @file station_func.h Functions related to stations. */

#ifndef STATION_FUNC_H
#define STATION_FUNC_H

#include "station_type.h"
#include "sprite.h"
#include "rail_type.h"
#include "road_type.h"
#include "tile_type.h"
#include "cargo_type.h"
#include "vehicle_type.h"
#include <set>

void ModifyStationRatingAround(TileIndex tile, PlayerID owner, int amount, uint radius);

/** A set of stations (\c const \c Station* ) */
typedef std::set<Station*> StationSet;

StationSet FindStationsAroundIndustryTile(TileIndex tile, int w, int h);

void ShowStationViewWindow(StationID station);
void UpdateAllStationVirtCoord();

void AfterLoadStations();
void GetProductionAroundTiles(AcceptedCargo produced, TileIndex tile, int w, int h, int rad);
void GetAcceptanceAroundTiles(AcceptedCargo accepts, TileIndex tile, int w, int h, int rad);

const DrawTileSprites *GetStationTileLayout(StationType st, byte gfx);
void StationPickerDrawSprite(int x, int y, StationType st, RailType railtype, RoadType roadtype, int image);

bool HasStationInUse(StationID station, PlayerID player);

RoadStop * GetRoadStopByTile(TileIndex tile, RoadStopType type);
uint GetNumRoadStops(const Station* st, RoadStopType type);
RoadStop * AllocateRoadStop();

void ClearSlot(Vehicle *v);

void DeleteOilRig(TileIndex t);

/* Check if a rail station tile is traversable. */
bool IsStationTileBlocked(TileIndex tile);

/* Check if a rail station tile is electrifiable. */
bool IsStationTileElectrifiable(TileIndex tile);

void UpdateAirportsNoise();

#endif /* STATION_FUNC_H */
