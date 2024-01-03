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
#include "viewport_func.h"
#include "viewport_kdtree.h"
#include "command_func.h"
#include "news_func.h"
#include "aircraft.h"
#include "vehiclelist.h"
#include "core/pool_func.hpp"
#include "station_base.h"
#include "station_kdtree.h"
#include "roadstop_base.h"
#include "industry.h"
#include "town.h"
#include "core/random_func.hpp"
#include "linkgraph/linkgraph.h"
#include "linkgraph/linkgraphschedule.h"

#include "table/strings.h"

#include "safeguards.h"

/** The pool of stations. */
StationPool _station_pool("Station");
INSTANTIATE_POOL_METHODS(Station)


StationKdtree _station_kdtree(Kdtree_StationXYFunc);

void RebuildStationKdtree()
{
	std::vector<StationID> stids;
	for (const Station *st : Station::Iterate()) {
		stids.push_back(st->index);
	}
	_station_kdtree.Build(stids.begin(), stids.end());
}


BaseStation::~BaseStation()
{
	if (CleaningPool()) return;

	CloseWindowById(WC_TRAINS_LIST,   VehicleListIdentifier(VL_STATION_LIST, VEH_TRAIN,    this->owner, this->index).Pack());
	CloseWindowById(WC_ROADVEH_LIST,  VehicleListIdentifier(VL_STATION_LIST, VEH_ROAD,     this->owner, this->index).Pack());
	CloseWindowById(WC_SHIPS_LIST,    VehicleListIdentifier(VL_STATION_LIST, VEH_SHIP,     this->owner, this->index).Pack());
	CloseWindowById(WC_AIRCRAFT_LIST, VehicleListIdentifier(VL_STATION_LIST, VEH_AIRCRAFT, this->owner, this->index).Pack());

	this->sign.MarkDirty();
}

Station::Station(TileIndex tile) :
	SpecializedStation<Station, false>(tile),
	bus_station(INVALID_TILE, 0, 0),
	truck_station(INVALID_TILE, 0, 0),
	ship_station(INVALID_TILE, 0, 0),
	indtype(IT_INVALID),
	time_since_load(255),
	time_since_unload(255),
	last_vehicle_type(VEH_INVALID)
{
	/* this->random_bits is set in Station::AddFacility() */
}

/**
 * Clean up a station by clearing vehicle orders, invalidating windows and
 * removing link stats.
 * Aircraft-Hangar orders need special treatment here, as the hangars are
 * actually part of a station (tiletype is STATION), but the order type
 * is OT_GOTO_DEPOT.
 */
Station::~Station()
{
	if (CleaningPool()) {
		for (GoodsEntry &ge : this->goods) {
			ge.cargo.OnCleanPool();
		}
		return;
	}

	while (!this->loading_vehicles.empty()) {
		this->loading_vehicles.front()->LeaveStation();
	}

	for (Aircraft *a : Aircraft::Iterate()) {
		if (!a->IsNormalAircraft()) continue;
		if (a->targetairport == this->index) a->targetairport = INVALID_STATION;
	}

	for (CargoID c = 0; c < NUM_CARGO; ++c) {
		LinkGraph *lg = LinkGraph::GetIfValid(this->goods[c].link_graph);
		if (lg == nullptr) continue;

		for (NodeID node = 0; node < lg->Size(); ++node) {
			Station *st = Station::Get((*lg)[node].station);
			st->goods[c].flows.erase(this->index);
			if ((*lg)[node].HasEdgeTo(this->goods[c].node) && (*lg)[node][this->goods[c].node].LastUpdate() != CalendarTime::INVALID_DATE) {
				st->goods[c].flows.DeleteFlows(this->index);
				RerouteCargo(st, c, this->index, st->index);
			}
		}
		lg->RemoveNode(this->goods[c].node);
		if (lg->Size() == 0) {
			LinkGraphSchedule::instance.Unqueue(lg);
			delete lg;
		}
	}

	for (Vehicle *v : Vehicle::Iterate()) {
		/* Forget about this station if this station is removed */
		if (v->last_station_visited == this->index) {
			v->last_station_visited = INVALID_STATION;
		}
		if (v->last_loading_station == this->index) {
			v->last_loading_station = INVALID_STATION;
		}
	}

	/* Remove station from industries and towns that reference it. */
	this->RemoveFromAllNearbyLists();

	/* Clear the persistent storage. */
	delete this->airport.psa;

	if (this->owner == OWNER_NONE) {
		/* Invalidate all in case of oil rigs. */
		InvalidateWindowClassesData(WC_STATION_LIST, 0);
	} else {
		InvalidateWindowData(WC_STATION_LIST, this->owner, 0);
	}

	CloseWindowById(WC_STATION_VIEW, index);

	/* Now delete all orders that go to the station */
	RemoveOrderFromAllVehicles(OT_GOTO_STATION, this->index);

	/* Remove all news items */
	DeleteStationNews(this->index);

	for (GoodsEntry &ge : this->goods) {
		ge.cargo.Truncate();
	}

	CargoPacket::InvalidateAllFrom(this->index);

	_station_kdtree.Remove(this->index);
	if (this->sign.kdtree_valid) _viewport_sign_kdtree.Remove(ViewportSignKdtreeItem::MakeStation(this->index));
}


/**
 * Invalidating of the JoinStation window has to be done
 * after removing item from the pool.
 */
void BaseStation::PostDestructor(size_t)
{
	InvalidateWindowData(WC_SELECT_STATION, 0, 0);
}

void BaseStation::SetRoadStopTileData(TileIndex tile, byte data, bool animation)
{
	for (RoadStopTileData &tile_data : this->custom_roadstop_tile_data) {
		if (tile_data.tile == tile) {
			if (animation) {
				tile_data.animation_frame = data;
			} else {
				tile_data.random_bits = data;
			}
			return;
		}
	}
	RoadStopTileData tile_data;
	tile_data.tile = tile;
	tile_data.animation_frame = animation ? data : 0;
	tile_data.random_bits = animation ? 0 : data;
	this->custom_roadstop_tile_data.push_back(tile_data);
}

void BaseStation::RemoveRoadStopTileData(TileIndex tile)
{
	for (RoadStopTileData &tile_data : this->custom_roadstop_tile_data) {
		if (tile_data.tile == tile) {
			tile_data = this->custom_roadstop_tile_data.back();
			this->custom_roadstop_tile_data.pop_back();
			return;
		}
	}
}

/**
 * Get the primary road stop (the first road stop) that the given vehicle can load/unload.
 * @param v the vehicle to get the first road stop for
 * @return the first roadstop that this vehicle can load at
 */
RoadStop *Station::GetPrimaryRoadStop(const RoadVehicle *v) const
{
	RoadStop *rs = this->GetPrimaryRoadStop(v->IsBus() ? ROADSTOP_BUS : ROADSTOP_TRUCK);

	for (; rs != nullptr; rs = rs->next) {
		/* The vehicle cannot go to this roadstop (different roadtype) */
		if (!HasTileAnyRoadType(rs->xy, v->compatible_roadtypes)) continue;
		/* The vehicle is articulated and can therefore not go to a standard road stop. */
		if (IsBayRoadStopTile(rs->xy) && v->HasArticulatedPart()) continue;

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
		this->MoveSign(facil_xy);
		this->random_bits = Random();
	}
	this->facilities |= new_facility_bit;
	this->owner = _current_company;
	this->build_date = TimerGameCalendar::date;
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
		if (this->speclist.empty()) return;
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
 * Get the catchment size of an individual station tile.
 * @param tile Station tile to get catchment size of.
 * @param st Associated station of station tile.
 * @pre IsTileType(tile, MP_STATION)
 * @return The catchment size of the station tile.
 */
static uint GetTileCatchmentRadius(TileIndex tile, const Station *st)
{
	assert(IsTileType(tile, MP_STATION));

	if (_settings_game.station.modified_catchment) {
		switch (GetStationType(tile)) {
			case STATION_RAIL:    return CA_TRAIN;
			case STATION_OILRIG:  return CA_UNMODIFIED;
			case STATION_AIRPORT: return st->airport.GetSpec()->catchment;
			case STATION_TRUCK:   return CA_TRUCK;
			case STATION_BUS:     return CA_BUS;
			case STATION_DOCK:    return CA_DOCK;

			default: NOT_REACHED();
			case STATION_BUOY:
			case STATION_WAYPOINT: return CA_NONE;
		}
	} else {
		switch (GetStationType(tile)) {
			default:               return CA_UNMODIFIED;
			case STATION_BUOY:
			case STATION_WAYPOINT: return CA_NONE;
		}
	}
}

/**
 * Determines the catchment radius of the station
 * @return The radius
 */
uint Station::GetCatchmentRadius() const
{
	uint ret = CA_NONE;

	if (_settings_game.station.modified_catchment) {
		if (this->bus_stops          != nullptr)      ret = std::max<uint>(ret, CA_BUS);
		if (this->truck_stops        != nullptr)      ret = std::max<uint>(ret, CA_TRUCK);
		if (this->train_station.tile != INVALID_TILE) ret = std::max<uint>(ret, CA_TRAIN);
		if (this->ship_station.tile  != INVALID_TILE) ret = std::max<uint>(ret, CA_DOCK);
		if (this->airport.tile       != INVALID_TILE) ret = std::max<uint>(ret, this->airport.GetSpec()->catchment);
	} else {
		if (this->bus_stops != nullptr || this->truck_stops != nullptr || this->train_station.tile != INVALID_TILE || this->ship_station.tile != INVALID_TILE || this->airport.tile != INVALID_TILE) {
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
		std::max<int>(this->rect.left   - catchment_radius, 0),
		std::max<int>(this->rect.top    - catchment_radius, 0),
		std::min<int>(this->rect.right  + catchment_radius, Map::MaxX()),
		std::min<int>(this->rect.bottom + catchment_radius, Map::MaxY())
	};

	return ret;
}

/**
 * Add nearby industry to station's industries_near list if it accepts cargo.
 * For industries that are already on the list update distance if it's closer.
 * @param ind  Industry
 * @param tile Tile of the industry to measure distance to.
 */
void Station::AddIndustryToDeliver(Industry *ind, TileIndex tile)
{
	/* Using DistanceMax to get about the same order as with previously used CircularTileSearch. */
	uint distance = DistanceMax(this->xy, tile);

	/* Don't check further if this industry is already in the list but update the distance if it's closer */
	auto pos = std::find_if(this->industries_near.begin(), this->industries_near.end(), [&](const IndustryListEntry &e) { return e.industry->index == ind->index; });
	if (pos != this->industries_near.end()) {
		if (pos->distance > distance) {
			auto node = this->industries_near.extract(pos);
			node.value().distance = distance;
			this->industries_near.insert(std::move(node));
		}
		return;
	}

	/* Include only industries that can accept cargo */
	if (!ind->IsCargoAccepted()) return;

	this->industries_near.insert(IndustryListEntry{distance, ind});
}

/**
 * Remove nearby industry from station's industries_near list.
 * @param ind  Industry
 */
void Station::RemoveIndustryToDeliver(Industry *ind)
{
	auto pos = std::find_if(this->industries_near.begin(), this->industries_near.end(), [&](const IndustryListEntry &e) { return e.industry->index == ind->index; });
	if (pos != this->industries_near.end()) {
		this->industries_near.erase(pos);
	}
}


/**
 * Remove this station from the nearby stations lists of all towns and industries.
 */
void Station::RemoveFromAllNearbyLists()
{
	for (Town *t : Town::Iterate()) { t->stations_near.erase(this); }
	for (Industry *i : Industry::Iterate()) { i->stations_near.erase(this); }
}

/**
 * Test if the given town ID is covered by our catchment area.
 * This is used when removing a house tile to determine if it was the last house tile
 * within our catchment.
 * @param t TownID to test.
 * @return true if at least one house tile of TownID is covered.
 */
bool Station::CatchmentCoversTown(TownID t) const
{
	BitmapTileIterator it(this->catchment_tiles);
	for (TileIndex tile = it; tile != INVALID_TILE; tile = ++it) {
		if (IsTileType(tile, MP_HOUSE) && GetTownIndex(tile) == t) return true;
	}
	return false;
}

/**
 * Recompute tiles covered in our catchment area.
 * This will additionally recompute nearby towns and industries.
 * @param no_clear_nearby_lists If Station::RemoveFromAllNearbyLists does not need to be called.
 */
void Station::RecomputeCatchment(bool no_clear_nearby_lists)
{
	this->industries_near.clear();
	if (!no_clear_nearby_lists) this->RemoveFromAllNearbyLists();

	if (this->rect.IsEmpty()) {
		this->catchment_tiles.Reset();
		return;
	}

	if (!_settings_game.station.serve_neutral_industries && this->industry != nullptr) {
		/* Station is associated with an industry, so we only need to deliver to that industry. */
		this->catchment_tiles.Initialize(this->industry->location);
		for (TileIndex tile : this->industry->location) {
			if (IsTileType(tile, MP_INDUSTRY) && GetIndustryIndex(tile) == this->industry->index) {
				this->catchment_tiles.SetTile(tile);
			}
		}
		/* The industry's stations_near may have been computed before its neutral station was built so clear and re-add here. */
		for (Station *st : this->industry->stations_near) {
			st->RemoveIndustryToDeliver(this->industry);
		}
		this->industry->stations_near.clear();
		this->industry->stations_near.insert(this);
		this->industries_near.insert(IndustryListEntry{0, this->industry});
		return;
	}

	this->catchment_tiles.Initialize(GetCatchmentRect());

	/* Loop finding all station tiles */
	TileArea ta(TileXY(this->rect.left, this->rect.top), TileXY(this->rect.right, this->rect.bottom));
	for (TileIndex tile : ta) {
		if (!IsTileType(tile, MP_STATION) || GetStationIndex(tile) != this->index) continue;

		uint r = GetTileCatchmentRadius(tile, this);
		if (r == CA_NONE) continue;

		/* This tile sub-loop doesn't need to test any tiles, they are simply added to the catchment set. */
		TileArea ta2 = TileArea(tile, 1, 1).Expand(r);
		for (TileIndex tile2 : ta2) this->catchment_tiles.SetTile(tile2);
	}

	/* Search catchment tiles for towns and industries */
	BitmapTileIterator it(this->catchment_tiles);
	for (TileIndex tile = it; tile != INVALID_TILE; tile = ++it) {
		if (IsTileType(tile, MP_HOUSE)) {
			Town *t = Town::GetByTile(tile);
			t->stations_near.insert(this);
		}
		if (IsTileType(tile, MP_INDUSTRY)) {
			Industry *i = Industry::GetByTile(tile);

			/* Ignore industry if it has a neutral station. It already can't be this station. */
			if (!_settings_game.station.serve_neutral_industries && i->neutral_station != nullptr) continue;

			i->stations_near.insert(this);

			/* Add if we can deliver to this industry as well */
			this->AddIndustryToDeliver(i, tile);
		}
	}
}

/**
 * Recomputes catchment of all stations.
 * This will additionally recompute nearby stations for all towns and industries.
 */
/* static */ void Station::RecomputeCatchmentForAll()
{
	for (Town *t : Town::Iterate()) { t->stations_near.clear(); }
	for (Industry *i : Industry::Iterate()) { i->stations_near.clear(); }
	for (Station *st : Station::Iterate()) { st->RecomputeCatchment(true); }
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
 * @param distance The maximum distance a point may have (L1 norm)
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
		Rect new_rect = {std::min(x, this->left), std::min(y, this->top), std::max(x, this->right), std::max(y, this->bottom)};

		/* check new rect dimensions against preset max */
		int w = new_rect.Width();
		int h = new_rect.Height();
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
	for (TileIndex tile : ta) {
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

/**
 * Calculates the maintenance cost of all airports of a company.
 * @param owner Company.
 * @return Total cost.
 */
Money AirportMaintenanceCost(Owner owner)
{
	Money total_cost = 0;

	for (const Station *st : Station::Iterate()) {
		if (st->owner == owner && (st->facilities & FACIL_AIRPORT)) {
			total_cost += _price[PR_INFRASTRUCTURE_AIRPORT] * st->airport.GetSpec()->maintenance_cost;
		}
	}
	/* 3 bits fraction for the maintenance cost factor. */
	return total_cost >> 3;
}

bool StationCompare::operator() (const Station *lhs, const Station *rhs) const
{
	return lhs->index < rhs->index;
}
