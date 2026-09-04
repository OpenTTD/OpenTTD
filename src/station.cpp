/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file station.cpp Implementation of the station base class. */

#include "stdafx.h"
#include "core/flatset_type.hpp"
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


StationKdtree _station_kdtree{};

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

	CloseWindowById(WindowClass::TrainList, VehicleListIdentifier(VehicleListType::Station, VehicleType::Train, this->owner, this->index).ToWindowNumber());
	CloseWindowById(WindowClass::RoadVehicleList, VehicleListIdentifier(VehicleListType::Station, VehicleType::Road, this->owner, this->index).ToWindowNumber());
	CloseWindowById(WindowClass::ShipList, VehicleListIdentifier(VehicleListType::Station, VehicleType::Ship, this->owner, this->index).ToWindowNumber());
	CloseWindowById(WindowClass::AircraftList, VehicleListIdentifier(VehicleListType::Station, VehicleType::Aircraft, this->owner, this->index).ToWindowNumber());

	this->sign.MarkDirty();
}

Station::Station(StationID index, TileIndex tile) :
	SpecializedStation<Station, false>(index, tile),
	bus_station(INVALID_TILE, 0, 0),
	truck_station(INVALID_TILE, 0, 0),
	ship_station(INVALID_TILE, 0, 0),
	indtype(IT_INVALID),
	time_since_load(255),
	time_since_unload(255),
	last_vehicle_type(VehicleType::Invalid)
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
			if (!ge.HasData()) continue;
			ge.GetData().cargo.OnCleanPool();
		}
		return;
	}

	while (!this->loading_vehicles.empty()) {
		this->loading_vehicles.front()->LeaveStation();
	}

	for (Aircraft *a : Aircraft::Iterate()) {
		if (!a->IsNormalAircraft()) continue;
		if (a->targetairport == this->index) a->targetairport = StationID::Invalid();
	}

	for (CargoType cargo : EnumRange(NUM_CARGO)) {
		LinkGraph *lg = LinkGraph::GetIfValid(this->goods[cargo].link_graph);
		if (lg == nullptr) continue;

		for (NodeID node = 0; node < lg->Size(); ++node) {
			Station *st = Station::Get((*lg)[node].station);
			if (!st->goods[cargo].HasData()) continue;
			st->goods[cargo].GetData().flows.erase(this->index);
			if ((*lg)[node].HasEdgeTo(this->goods[cargo].node) && (*lg)[node][this->goods[cargo].node].LastUpdate() != EconomyTime::INVALID_DATE) {
				st->goods[cargo].GetData().flows.DeleteFlows(this->index);
				RerouteCargo(st, cargo, this->index, st->index);
			}
		}
		lg->RemoveNode(this->goods[cargo].node);
		if (lg->Size() == 0) {
			LinkGraphSchedule::instance.Dequeue(lg);
			delete lg;
		}
	}

	for (Vehicle *v : Vehicle::Iterate()) {
		/* Forget about this station if this station is removed */
		if (v->last_station_visited == this->index) {
			v->last_station_visited = StationID::Invalid();
		}
		if (v->last_loading_station == this->index) {
			v->last_loading_station = StationID::Invalid();
		}
	}

	/* Remove station from industries and towns that reference it. */
	this->RemoveFromAllNearbyLists();

	/* Clear the persistent storage. */
	delete this->airport.psa;

	if (this->owner == OWNER_NONE) {
		/* Invalidate all in case of oil rigs. */
		InvalidateWindowClassesData(WindowClass::StationList, 0);
	} else {
		InvalidateWindowData(WindowClass::StationList, this->owner, 0);
	}

	CloseWindowById(WindowClass::StationView, index);

	/* Now delete all orders that go to the station */
	RemoveOrderFromAllVehicles(OT_GOTO_STATION, this->index);

	/* Remove all news items */
	DeleteStationNews(this->index);

	for (GoodsEntry &ge : this->goods) {
		if (!ge.HasData()) continue;
		ge.GetData().cargo.Truncate();
	}

	CargoPacket::InvalidateAllFrom(this->index);

	_station_kdtree.Remove(this->index);
	if (this->sign.kdtree_valid) _viewport_sign_kdtree.Remove(ViewportSignKdtreeItem::MakeStation(this->index));
}


/**
 * Invalidating of the JoinStation window has to be done
 * after removing item from the pool.
 * @copydoc Pool::PoolItem::PostDestructor
 */
void BaseStation::PostDestructor([[maybe_unused]] size_t index)
{
	InvalidateWindowData(WindowClass::JoinStation, 0, 0);
}

bool BaseStation::SetRoadStopTileData(TileIndex tile, uint8_t data, bool animation)
{
	for (RoadStopTileData &tile_data : this->custom_roadstop_tile_data) {
		if (tile_data.tile == tile) {
			uint8_t &v = animation ? tile_data.animation_frame : tile_data.random_bits;
			if (v == data) return false;
			v = data;
			return true;
		}
	}
	RoadStopTileData tile_data;
	tile_data.tile = tile;
	tile_data.animation_frame = animation ? data : 0;
	tile_data.random_bits = animation ? 0 : data;
	this->custom_roadstop_tile_data.push_back(tile_data);
	return data != 0;
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
	RoadStop *rs = this->GetPrimaryRoadStop(v->IsBus() ? RoadStopType::Bus : RoadStopType::Truck);

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
 * @param new_facility_bit The new facility.
 * @param facil_xy The location where this facility is built.
 */
void Station::AddFacility(StationFacility new_facility_bit, TileIndex facil_xy)
{
	if (this->facilities.None()) {
		this->MoveSign(facil_xy);
		this->random_bits = Random();
	}
	this->facilities.Set(new_facility_bit);
	this->owner = _current_company;
	this->build_date = TimerGameCalendar::date;
	SetWindowClassesDirty(WindowClass::VehicleOrders);
}

/**
 * Marks the tiles of the station as dirty.
 * @param cargo_change Whether only cargo amounts changed.
 * @ingroup dirty
 */
void Station::MarkTilesDirty(bool cargo_change) const
{
	if (this->train_station.IsEmpty()) return;

	/* cargo_change is set if we're refreshing the tiles due to cargo moving
	 * around. */
	if (cargo_change) {
		/* Don't waste time updating if there are no custom station graphics
		 * that might change. Even if there are custom graphics, they might
		 * not change. Unfortunately we have no way of telling. */
		if (this->speclist.empty()) return;
	}

	for (TileIndex tile : this->train_station) {
		if (this->TileBelongsToRailStation(tile)) {
			MarkTileDirtyByTile(tile);
		}
	}
}

/* virtual */ uint Station::GetPlatformLength(TileIndex tile) const
{
	assert(this->TileBelongsToRailStation(tile));

	TileIndexDiff delta = TileOffsByAxis(GetRailStationAxis(tile));

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
	assert(dir < DiagDirection::End);

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
 * @pre IsTileType(tile, TileType::Station)
 * @return The catchment size of the station tile.
 */
static uint GetTileCatchmentRadius(TileIndex tile, const Station *st)
{
	assert(IsTileType(tile, TileType::Station));

	if (_settings_game.station.modified_catchment) {
		switch (GetStationType(tile)) {
			case StationType::Rail:    return CA_TRAIN;
			case StationType::Oilrig:  return CA_UNMODIFIED;
			case StationType::Airport: return st->airport.GetSpec()->catchment;
			case StationType::Truck:   return CA_TRUCK;
			case StationType::Bus:     return CA_BUS;
			case StationType::Dock:    return CA_DOCK;

			default: NOT_REACHED();
			case StationType::Buoy:
			case StationType::RailWaypoint:
			case StationType::RoadWaypoint: return CA_NONE;
		}
	} else {
		switch (GetStationType(tile)) {
			default:               return CA_UNMODIFIED;
			case StationType::Buoy:
			case StationType::RailWaypoint:
			case StationType::RoadWaypoint: return CA_NONE;
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
		if (this->bus_stops != nullptr) ret = std::max<uint>(ret, CA_BUS);
		if (this->truck_stops != nullptr) ret = std::max<uint>(ret, CA_TRUCK);
		if (!this->train_station.IsEmpty()) ret = std::max<uint>(ret, CA_TRAIN);
		if (!this->ship_station.IsEmpty()) ret = std::max<uint>(ret, CA_DOCK);
		if (!this->airport.IsEmpty()) ret = std::max<uint>(ret, this->airport.GetSpec()->catchment);
	} else {
		if (this->bus_stops != nullptr || this->truck_stops != nullptr || !this->train_station.IsEmpty() || !this->ship_station.IsEmpty() || !this->airport.IsEmpty()) {
			ret = CA_UNMODIFIED;
		}
	}

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
	/* Using DistanceMax to get about the same order as with previously used SpiralTileSequence. */
	uint distance = DistanceMax(this->xy, tile);

	/* Don't check further if this industry is already in the list but update the distance if it's closer */
	auto pos = std::ranges::find(this->industries_near, ind, &IndustryListEntry::industry);
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
	auto pos = std::ranges::find(this->industries_near, ind, &IndustryListEntry::industry);
	if (pos != this->industries_near.end()) {
		this->industries_near.erase(pos);
	}
}


/**
 * Remove this station from the nearby stations lists of nearby towns and industries.
 */
void Station::RemoveFromAllNearbyLists()
{
	FlatSet<TownID> towns;
	FlatSet<IndustryID> industries;

	for (const auto &tile : this->catchment_tiles) {
		TileType type = GetTileType(tile);
		if (type == TileType::House) {
			towns.insert(GetTownIndex(tile));
		} else if (type == TileType::Industry) {
			industries.insert(GetIndustryIndex(tile));
		}
	}

	for (const TownID &townid : towns) { Town::Get(townid)->stations_near.erase(this); }
	for (const IndustryID &industryid : industries) { Industry::Get(industryid)->stations_near.erase(this); }
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
		if (IsTileType(tile, TileType::House) && GetTownIndex(tile) == t) return true;
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

	if (this->spread.IsEmpty()) {
		this->catchment_tiles.Reset();
		return;
	}

	if (!_settings_game.station.serve_neutral_industries && this->industry != nullptr) {
		/* Station is associated with an industry, so we only need to deliver to that industry. */
		this->catchment_tiles.Initialize(this->industry->location);
		for (TileIndex tile : this->industry->location) {
			if (IsTileType(tile, TileType::Industry) && GetIndustryIndex(tile) == this->industry->index) {
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

	this->catchment_tiles.Initialize(TileArea{this->spread}.Expand(this->GetCatchmentRadius()));

	/* Loop finding all station tiles */
	for (TileIndex tile : this->spread) {
		if (!IsTileType(tile, TileType::Station) || GetStationIndex(tile) != this->index) continue;

		uint r = GetTileCatchmentRadius(tile, this);
		if (r == CA_NONE) continue;

		/* This tile sub-loop doesn't need to test any tiles, they are simply added to the catchment set. */
		TileArea ta2 = TileArea(tile, 1, 1).Expand(r);
		for (TileIndex tile2 : ta2) this->catchment_tiles.SetTile(tile2);
	}

	/* Search catchment tiles for towns and industries */
	BitmapTileIterator it(this->catchment_tiles);
	for (TileIndex tile = it; tile != INVALID_TILE; tile = ++it) {
		if (IsTileType(tile, TileType::House)) {
			Town *t = Town::GetByTile(tile);
			t->stations_near.insert(this);
		}
		if (IsTileType(tile, TileType::Industry)) {
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

/**
 * Check if adding a new area to a station will exceed the maximum station spread.
 * @param area The existing station spread area.
 * @param new_area The new area to add.
 * @return CommandCost
 */
CommandCost CheckStationSpread(TileArea area, const TileArea &new_area)
{
	/* Check for incorrect width / length. */
	if (new_area.w == 0 || new_area.h == 0) return CMD_ERROR;

	/* Check if the first and last tile are valid. */
	if (!IsValidTile(new_area.tile) || TileAddWrap(new_area.tile, new_area.w - 1, new_area.h - 1) == INVALID_TILE) return CMD_ERROR;

	area.Add(new_area);

	if (area.w > _settings_game.station.station_spread || area.h > _settings_game.station.station_spread) {
		return CommandCost(STR_ERROR_STATION_TOO_SPREAD_OUT);
	}

	return CommandCost();
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
		if (st->owner == owner && st->facilities.Test(StationFacility::Airport)) {
			total_cost += _price[Price::InfrastructureAirport] * st->airport.GetSpec()->maintenance_cost;
		}
	}
	/* 3 bits fraction for the maintenance cost factor. */
	return total_cost >> 3;
}

bool StationCompare::operator() (const Station *lhs, const Station *rhs) const
{
	return lhs->index < rhs->index;
}
