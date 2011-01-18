/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file station.cpp Implementation of the station base class. */

#include "stdafx.h"
#include "company_func.h"
#include "company_base.h"
#include "roadveh.h"
#include "functions.h"
#include "window_func.h"
#include "date_func.h"
#include "command_func.h"
#include "news_func.h"
#include "aircraft.h"
#include "vehiclelist.h"
#include "core/pool_func.hpp"
#include "station_base.h"
#include "roadstop_base.h"
#include "industry.h"
#include "core/random_func.hpp"

#include "table/strings.h"

StationPool _station_pool("Station");
INSTANTIATE_POOL_METHODS(Station)

BaseStation::~BaseStation()
{
	free(this->name);
	free(this->speclist);

	if (CleaningPool()) return;

	Owner owner = this->owner;
	if (!Company::IsValidID(owner)) owner = _local_company;
	if (!Company::IsValidID(owner)) return; // Spectators
	DeleteWindowById(WC_TRAINS_LIST,   VehicleListIdentifier(VL_STATION_LIST, VEH_TRAIN,    owner, this->index).Pack());
	DeleteWindowById(WC_ROADVEH_LIST,  VehicleListIdentifier(VL_STATION_LIST, VEH_ROAD,     owner, this->index).Pack());
	DeleteWindowById(WC_SHIPS_LIST,    VehicleListIdentifier(VL_STATION_LIST, VEH_SHIP,     owner, this->index).Pack());
	DeleteWindowById(WC_AIRCRAFT_LIST, VehicleListIdentifier(VL_STATION_LIST, VEH_AIRCRAFT, owner, this->index).Pack());

	this->sign.MarkDirty();
}

Station::Station(TileIndex tile) :
	SpecializedStation<Station, false>(tile),
	bus_station(INVALID_TILE, 0, 0),
	truck_station(INVALID_TILE, 0, 0),
	dock_tile(INVALID_TILE),
	indtype(IT_INVALID),
	time_since_load(255),
	time_since_unload(255),
	last_vehicle_type(VEH_INVALID)
{
	/* this->random_bits is set in Station::AddFacility() */
}

/**
 * Clean up a station by clearing vehicle orders and invalidating windows.
 * Aircraft-Hangar orders need special treatment here, as the hangars are
 * actually part of a station (tiletype is STATION), but the order type
 * is OT_GOTO_DEPOT.
 */
Station::~Station()
{
	if (CleaningPool()) return;

	while (!this->loading_vehicles.empty()) {
		this->loading_vehicles.front()->LeaveStation();
	}

	Aircraft *a;
	FOR_ALL_AIRCRAFT(a) {
		if (!a->IsNormalAircraft()) continue;
		if (a->targetairport == this->index) a->targetairport = INVALID_STATION;
	}

	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		/* Forget about this station if this station is removed */
		if (v->last_station_visited == this->index) {
			v->last_station_visited = INVALID_STATION;
		}
	}

	InvalidateWindowData(WC_STATION_LIST, this->owner, 0);

	DeleteWindowById(WC_STATION_VIEW, index);

	/* Now delete all orders that go to the station */
	RemoveOrderFromAllVehicles(OT_GOTO_STATION, this->index);

	/* Remove all news items */
	DeleteStationNews(this->index);

	for (CargoID c = 0; c < NUM_CARGO; c++) {
		this->goods[c].cargo.Truncate(0);
	}

	CargoPacket::InvalidateAllFrom(this->index);
}


/**
 * Invalidating of the JoinStation window has to be done
 * after removing item from the pool.
 * @param index index of deleted item
 */
void BaseStation::PostDestructor(size_t index)
{
	InvalidateWindowData(WC_SELECT_STATION, 0, 0);
}

/**
 * Get the primary road stop (the first road stop) that the given vehicle can load/unload.
 * @param v the vehicle to get the first road stop for
 * @return the first roadstop that this vehicle can load at
 */
RoadStop *Station::GetPrimaryRoadStop(const RoadVehicle *v) const
{
	RoadStop *rs = this->GetPrimaryRoadStop(v->IsBus() ? ROADSTOP_BUS : ROADSTOP_TRUCK);

	for (; rs != NULL; rs = rs->next) {
		/* The vehicle cannot go to this roadstop (different roadtype) */
		if ((GetRoadTypes(rs->xy) & v->compatible_roadtypes) == ROADTYPES_NONE) continue;
		/* The vehicle is articulated and can therefor not go the a standard road stop */
		if (IsStandardRoadStopTile(rs->xy) && v->HasArticulatedPart()) continue;

		/* The vehicle can actually go to this road stop. So, return it! */
		break;
	}

	return rs;
}

/**
 * Called when new facility is built on the station. If it is the first facility
 * it initializes also 'xy' and 'random_bits' members
 */
void Station::AddFacility(StationFacility new_facility_bit, TileIndex facil_xy)
{
	if (this->facilities == FACIL_NONE) {
		this->xy = facil_xy;
		this->random_bits = Random();
	}
	this->facilities |= new_facility_bit;
	this->owner = _current_company;
	this->build_date = _date;
}

/**
 * Marks the tiles of the station as dirty.
 *
 * @ingroup dirty
 */
void Station::MarkTilesDirty(bool cargo_change) const
{
	TileIndex tile = this->train_station.tile;
	int w, h;

	if (tile == INVALID_TILE) return;

	/* cargo_change is set if we're refreshing the tiles due to cargo moving
	 * around. */
	if (cargo_change) {
		/* Don't waste time updating if there are no custom station graphics
		 * that might change. Even if there are custom graphics, they might
		 * not change. Unfortunately we have no way of telling. */
		if (this->num_specs == 0) return;
	}

	for (h = 0; h < train_station.h; h++) {
		for (w = 0; w < train_station.w; w++) {
			if (this->TileBelongsToRailStation(tile)) {
				MarkTileDirtyByTile(tile);
			}
			tile += TileDiffXY(1, 0);
		}
		tile += TileDiffXY(-w, 1);
	}
}

/* virtual */ uint Station::GetPlatformLength(TileIndex tile) const
{
	assert(this->TileBelongsToRailStation(tile));

	TileIndexDiff delta = (GetRailStationAxis(tile) == AXIS_X ? TileDiffXY(1, 0) : TileDiffXY(0, 1));

	TileIndex t = tile;
	uint len = 0;
	do {
		t -= delta;
		len++;
	} while (IsCompatibleTrainStationTile(t, tile));

	t = tile;
	do {
		t += delta;
		len++;
	} while (IsCompatibleTrainStationTile(t, tile));

	return len - 1;
}

/* virtual */ uint Station::GetPlatformLength(TileIndex tile, DiagDirection dir) const
{
	TileIndex start_tile = tile;
	uint length = 0;
	assert(IsRailStationTile(tile));
	assert(dir < DIAGDIR_END);

	do {
		length++;
		tile += TileOffsByDiagDir(dir);
	} while (IsCompatibleTrainStationTile(tile, start_tile));

	return length;
}

/**
 * Determines the catchment radius of the station
 * @return The radius
 */
uint Station::GetCatchmentRadius() const
{
	uint ret = CA_NONE;

	if (_settings_game.station.modified_catchment) {
		if (this->bus_stops          != NULL)         ret = max<uint>(ret, CA_BUS);
		if (this->truck_stops        != NULL)         ret = max<uint>(ret, CA_TRUCK);
		if (this->train_station.tile != INVALID_TILE) ret = max<uint>(ret, CA_TRAIN);
		if (this->dock_tile          != INVALID_TILE) ret = max<uint>(ret, CA_DOCK);
		if (this->airport.tile       != INVALID_TILE) ret = max<uint>(ret, this->airport.GetSpec()->catchment);
	} else {
		if (this->bus_stops != NULL || this->truck_stops != NULL || this->train_station.tile != INVALID_TILE || this->dock_tile != INVALID_TILE || this->airport.tile != INVALID_TILE) {
			ret = CA_UNMODIFIED;
		}
	}

	return ret;
}

/**
 * Determines catchment rectangle of this station
 * @return clamped catchment rectangle
 */
Rect Station::GetCatchmentRect() const
{
	assert(!this->rect.IsEmpty());

	/* Compute acceptance rectangle */
	int catchment_radius = this->GetCatchmentRadius();

	Rect ret = {
		max<int>(this->rect.left   - catchment_radius, 0),
		max<int>(this->rect.top    - catchment_radius, 0),
		min<int>(this->rect.right  + catchment_radius, MapMaxX()),
		min<int>(this->rect.bottom + catchment_radius, MapMaxY())
	};

	return ret;
}

/** Rect and pointer to IndustryVector */
struct RectAndIndustryVector {
	Rect rect;
	IndustryVector *industries_near;
};

/**
 * Callback function for Station::RecomputeIndustriesNear()
 * Tests whether tile is an industry and possibly adds
 * the industry to station's industries_near list.
 * @param ind_tile tile to check
 * @param user_data pointer to RectAndIndustryVector
 * @return always false, we want to search all tiles
 */
static bool FindIndustryToDeliver(TileIndex ind_tile, void *user_data)
{
	/* Only process industry tiles */
	if (!IsTileType(ind_tile, MP_INDUSTRY)) return false;

	RectAndIndustryVector *riv = (RectAndIndustryVector *)user_data;
	Industry *ind = Industry::GetByTile(ind_tile);

	/* Don't check further if this industry is already in the list */
	if (riv->industries_near->Contains(ind)) return false;

	/* Only process tiles in the station acceptance rectangle */
	int x = TileX(ind_tile);
	int y = TileY(ind_tile);
	if (x < riv->rect.left || x > riv->rect.right || y < riv->rect.top || y > riv->rect.bottom) return false;

	/* Include only industries that can accept cargo */
	uint cargo_index;
	for (cargo_index = 0; cargo_index < lengthof(ind->accepts_cargo); cargo_index++) {
		if (ind->accepts_cargo[cargo_index] != CT_INVALID) break;
	}
	if (cargo_index >= lengthof(ind->accepts_cargo)) return false;

	*riv->industries_near->Append() = ind;

	return false;
}

/**
 * Recomputes Station::industries_near, list of industries possibly
 * accepting cargo in station's catchment radius
 */
void Station::RecomputeIndustriesNear()
{
	this->industries_near.Clear();
	if (this->rect.IsEmpty()) return;

	RectAndIndustryVector riv = {
		this->GetCatchmentRect(),
		&this->industries_near
	};

	/* Compute maximum extent of acceptance rectangle wrt. station sign */
	TileIndex start_tile = this->xy;
	uint max_radius = max(
		max(DistanceManhattan(start_tile, TileXY(riv.rect.left,  riv.rect.top)), DistanceManhattan(start_tile, TileXY(riv.rect.left,  riv.rect.bottom))),
		max(DistanceManhattan(start_tile, TileXY(riv.rect.right, riv.rect.top)), DistanceManhattan(start_tile, TileXY(riv.rect.right, riv.rect.bottom)))
	);

	CircularTileSearch(&start_tile, 2 * max_radius + 1, &FindIndustryToDeliver, &riv);
}

/**
 * Recomputes Station::industries_near for all stations
 */
/* static */ void Station::RecomputeIndustriesNearForAll()
{
	Station *st;
	FOR_ALL_STATIONS(st) st->RecomputeIndustriesNear();
}

/************************************************************************/
/*                     StationRect implementation                       */
/************************************************************************/

StationRect::StationRect()
{
	this->MakeEmpty();
}

void StationRect::MakeEmpty()
{
	this->left = this->top = this->right = this->bottom = 0;
}

/**
 * Determines whether a given point (x, y) is within a certain distance of
 * the station rectangle.
 * @note x and y are in Tile coordinates
 * @param x X coordinate
 * @param y Y coordinate
 * @param distance The maxmium distance a point may have (L1 norm)
 * @return true if the point is within distance tiles of the station rectangle
 */
bool StationRect::PtInExtendedRect(int x, int y, int distance) const
{
	return this->left - distance <= x && x <= this->right + distance &&
			this->top - distance <= y && y <= this->bottom + distance;
}

bool StationRect::IsEmpty() const
{
	return this->left == 0 || this->left > this->right || this->top > this->bottom;
}

CommandCost StationRect::BeforeAddTile(TileIndex tile, StationRectMode mode)
{
	int x = TileX(tile);
	int y = TileY(tile);
	if (this->IsEmpty()) {
		/* we are adding the first station tile */
		if (mode != ADD_TEST) {
			this->left = this->right = x;
			this->top = this->bottom = y;
		}
	} else if (!this->PtInExtendedRect(x, y)) {
		/* current rect is not empty and new point is outside this rect
		 * make new spread-out rectangle */
		Rect new_rect = {min(x, this->left), min(y, this->top), max(x, this->right), max(y, this->bottom)};

		/* check new rect dimensions against preset max */
		int w = new_rect.right - new_rect.left + 1;
		int h = new_rect.bottom - new_rect.top + 1;
		if (mode != ADD_FORCE && (w > _settings_game.station.station_spread || h > _settings_game.station.station_spread)) {
			assert(mode != ADD_TRY);
			return_cmd_error(STR_ERROR_STATION_TOO_SPREAD_OUT);
		}

		/* spread-out ok, return true */
		if (mode != ADD_TEST) {
			/* we should update the station rect */
			*this = new_rect;
		}
	} else {
		; // new point is inside the rect, we don't need to do anything
	}
	return CommandCost();
}

CommandCost StationRect::BeforeAddRect(TileIndex tile, int w, int h, StationRectMode mode)
{
	if (mode == ADD_FORCE || (w <= _settings_game.station.station_spread && h <= _settings_game.station.station_spread)) {
		/* Important when the old rect is completely inside the new rect, resp. the old one was empty. */
		CommandCost ret = this->BeforeAddTile(tile, mode);
		if (ret.Succeeded()) ret = this->BeforeAddTile(TILE_ADDXY(tile, w - 1, h - 1), mode);
		return ret;
	}
	return CommandCost();
}

/**
 * Check whether station tiles of the given station id exist in the given rectangle
 * @param st_id    Station ID to look for in the rectangle
 * @param left_a   Minimal tile X edge of the rectangle
 * @param top_a    Minimal tile Y edge of the rectangle
 * @param right_a  Maximal tile X edge of the rectangle (inclusive)
 * @param bottom_a Maximal tile Y edge of the rectangle (inclusive)
 * @return \c true if a station tile with the given \a st_id exists in the rectangle, \c false otherwise
 */
/* static */ bool StationRect::ScanForStationTiles(StationID st_id, int left_a, int top_a, int right_a, int bottom_a)
{
	TileArea ta(TileXY(left_a, top_a), TileXY(right_a, bottom_a));
	TILE_AREA_LOOP(tile, ta) {
		if (IsTileType(tile, MP_STATION) && GetStationIndex(tile) == st_id) return true;
	}

	return false;
}

bool StationRect::AfterRemoveTile(BaseStation *st, TileIndex tile)
{
	int x = TileX(tile);
	int y = TileY(tile);

	/* look if removed tile was on the bounding rect edge
	 * and try to reduce the rect by this edge
	 * do it until we have empty rect or nothing to do */
	for (;;) {
		/* check if removed tile is on rect edge */
		bool left_edge = (x == this->left);
		bool right_edge = (x == this->right);
		bool top_edge = (y == this->top);
		bool bottom_edge = (y == this->bottom);

		/* can we reduce the rect in either direction? */
		bool reduce_x = ((left_edge || right_edge) && !ScanForStationTiles(st->index, x, this->top, x, this->bottom));
		bool reduce_y = ((top_edge || bottom_edge) && !ScanForStationTiles(st->index, this->left, y, this->right, y));
		if (!(reduce_x || reduce_y)) break; // nothing to do (can't reduce)

		if (reduce_x) {
			/* reduce horizontally */
			if (left_edge) {
				/* move left edge right */
				this->left = x = x + 1;
			} else {
				/* move right edge left */
				this->right = x = x - 1;
			}
		}
		if (reduce_y) {
			/* reduce vertically */
			if (top_edge) {
				/* move top edge down */
				this->top = y = y + 1;
			} else {
				/* move bottom edge up */
				this->bottom = y = y - 1;
			}
		}

		if (left > right || top > bottom) {
			/* can't continue, if the remaining rectangle is empty */
			this->MakeEmpty();
			return true; // empty remaining rect
		}
	}
	return false; // non-empty remaining rect
}

bool StationRect::AfterRemoveRect(BaseStation *st, TileArea ta)
{
	assert(this->PtInExtendedRect(TileX(ta.tile), TileY(ta.tile)));
	assert(this->PtInExtendedRect(TileX(ta.tile) + ta.w - 1, TileY(ta.tile) + ta.h - 1));

	bool empty = this->AfterRemoveTile(st, ta.tile);
	if (ta.w != 1 || ta.h != 1) empty = empty || this->AfterRemoveTile(st, TILE_ADDXY(ta.tile, ta.w - 1, ta.h - 1));
	return empty;
}

StationRect& StationRect::operator = (const Rect &src)
{
	this->left = src.left;
	this->top = src.top;
	this->right = src.right;
	this->bottom = src.bottom;
	return *this;
}


void InitializeStations()
{
	_station_pool.CleanPool();
}
