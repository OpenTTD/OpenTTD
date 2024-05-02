/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cachecheck.cpp Check caches. */

#include "stdafx.h"
#include "aircraft.h"
#include "company_base.h"
#include "debug.h"
#include "industry.h"
#include "roadstop_base.h"
#include "roadveh.h"
#include "ship.h"
#include "station_base.h"
#include "station_map.h"
#include "subsidy_func.h"
#include "town.h"
#include "train.h"
#include "vehicle_base.h"

#include "safeguards.h"

extern void AfterLoadCompanyStats();
extern void RebuildTownCaches();

/**
 * Check the validity of some of the caches.
 * Especially in the sense of desyncs between
 * the cached value and what the value would
 * be when calculated from the 'base' data.
 */
void CheckCaches()
{
	/* Return here so it is easy to add checks that are run
	 * always to aid testing of caches. */
	if (_debug_desync_level <= 1) return;

	/* Check the town caches. */
	std::vector<TownCache> old_town_caches;
	for (const Town *t : Town::Iterate()) {
		old_town_caches.push_back(t->cache);
	}

	RebuildTownCaches();
	RebuildSubsidisedSourceAndDestinationCache();

	uint i = 0;
	for (Town *t : Town::Iterate()) {
		if (old_town_caches[i] != t->cache) {
			Debug(desync, 2, "warning: town cache mismatch: town {}", t->index);
		}
		i++;
	}

	/* Check company infrastructure cache. */
	std::vector<CompanyInfrastructure> old_infrastructure;
	for (const Company *c : Company::Iterate()) old_infrastructure.push_back(c->infrastructure);

	AfterLoadCompanyStats();

	i = 0;
	for (const Company *c : Company::Iterate()) {
		if (old_infrastructure[i] != c->infrastructure) {
			Debug(desync, 2, "warning: infrastructure cache mismatch: company {}", c->index);
		}
		i++;
	}

	/* Strict checking of the road stop cache entries */
	for (const RoadStop *rs : RoadStop::Iterate()) {
		if (IsBayRoadStopTile(rs->xy)) continue;

		assert(rs->GetEntry(DIAGDIR_NE) != rs->GetEntry(DIAGDIR_NW));
		rs->GetEntry(DIAGDIR_NE)->CheckIntegrity(rs);
		rs->GetEntry(DIAGDIR_NW)->CheckIntegrity(rs);
	}

	std::vector<NewGRFCache> grf_cache;
	std::vector<VehicleCache> veh_cache;
	std::vector<GroundVehicleCache> gro_cache;
	std::vector<TrainCache> tra_cache;

	for (Vehicle *v : Vehicle::Iterate()) {
		if (v != v->First() || v->vehstatus & VS_CRASHED || !v->IsPrimaryVehicle()) continue;

		for (const Vehicle *u = v; u != nullptr; u = u->Next()) {
			FillNewGRFVehicleCache(u);
			grf_cache.emplace_back(u->grf_cache);
			veh_cache.emplace_back(u->vcache);
			switch (u->type) {
				case VEH_TRAIN:
					gro_cache.emplace_back(Train::From(u)->gcache);
					tra_cache.emplace_back(Train::From(u)->tcache);
					break;
				case VEH_ROAD:
					gro_cache.emplace_back(RoadVehicle::From(u)->gcache);
					break;
				default:
					break;
			}
		}

		switch (v->type) {
			case VEH_TRAIN:    Train::From(v)->ConsistChanged(CCF_TRACK); break;
			case VEH_ROAD:     RoadVehUpdateCache(RoadVehicle::From(v)); break;
			case VEH_AIRCRAFT: UpdateAircraftCache(Aircraft::From(v));   break;
			case VEH_SHIP:     Ship::From(v)->UpdateCache();             break;
			default: break;
		}

		uint length = 0;
		for (const Vehicle *u = v; u != nullptr; u = u->Next()) {
			FillNewGRFVehicleCache(u);
			if (grf_cache[length] != u->grf_cache) {
				Debug(desync, 2, "warning: newgrf cache mismatch: type {}, vehicle {}, company {}, unit number {}, wagon {}", v->type, v->index, v->owner, v->unitnumber, length);
			}
			if (veh_cache[length] != u->vcache) {
				Debug(desync, 2, "warning: vehicle cache mismatch: type {}, vehicle {}, company {}, unit number {}, wagon {}", v->type, v->index, v->owner, v->unitnumber, length);
			}
			switch (u->type) {
				case VEH_TRAIN:
					if (gro_cache[length] != Train::From(u)->gcache) {
						Debug(desync, 2, "warning: train ground vehicle cache mismatch: vehicle {}, company {}, unit number {}, wagon {}", v->index, v->owner, v->unitnumber, length);
					}
					if (tra_cache[length] != Train::From(u)->tcache) {
						Debug(desync, 2, "warning: train cache mismatch: vehicle {}, company {}, unit number {}, wagon {}", v->index, v->owner, v->unitnumber, length);
					}
					break;
				case VEH_ROAD:
					if (gro_cache[length] != RoadVehicle::From(u)->gcache) {
						Debug(desync, 2, "warning: road vehicle ground vehicle cache mismatch: vehicle {}, company {}, unit number {}, wagon {}", v->index, v->owner, v->unitnumber, length);
					}
					break;
				default:
					break;
			}
			length++;
		}

		grf_cache.clear();
		veh_cache.clear();
		gro_cache.clear();
		tra_cache.clear();
	}

	/* Check whether the caches are still valid */
	for (Vehicle *v : Vehicle::Iterate()) {
		[[maybe_unused]] const auto a = v->cargo.PeriodsInTransit();
		[[maybe_unused]] const auto b = v->cargo.TotalCount();
		[[maybe_unused]] const auto c = v->cargo.GetFeederShare();
		v->cargo.InvalidateCache();
		assert(a == v->cargo.PeriodsInTransit());
		assert(b == v->cargo.TotalCount());
		assert(c == v->cargo.GetFeederShare());
	}

	/* Backup stations_near */
	std::vector<StationList> old_town_stations_near;
	for (Town *t : Town::Iterate()) old_town_stations_near.push_back(t->stations_near);

	std::vector<StationList> old_industry_stations_near;
	for (Industry *ind : Industry::Iterate()) old_industry_stations_near.push_back(ind->stations_near);

	std::vector<IndustryList> old_station_industries_near;
	for (Station *st : Station::Iterate()) old_station_industries_near.push_back(st->industries_near);

	for (Station *st : Station::Iterate()) {
		for (GoodsEntry &ge : st->goods) {
			[[maybe_unused]] const auto a = ge.cargo.PeriodsInTransit();
			[[maybe_unused]] const auto b = ge.cargo.TotalCount();
			ge.cargo.InvalidateCache();
			assert(a == ge.cargo.PeriodsInTransit());
			assert(b == ge.cargo.TotalCount());
		}

		/* Check docking tiles */
		TileArea ta;
		std::map<TileIndex, bool> docking_tiles;
		for (TileIndex tile : st->docking_station) {
			ta.Add(tile);
			docking_tiles[tile] = IsDockingTile(tile);
		}
		UpdateStationDockingTiles(st);
		if (ta.tile != st->docking_station.tile || ta.w != st->docking_station.w || ta.h != st->docking_station.h) {
			Debug(desync, 2, "warning: station docking mismatch: station {}, company {}", st->index, st->owner);
		}
		for (TileIndex tile : ta) {
			if (docking_tiles[tile] != IsDockingTile(tile)) {
				Debug(desync, 2, "warning: docking tile mismatch: tile {}", tile);
			}
		}
	}

	Station::RecomputeCatchmentForAll();

	/* Check industries_near */
	i = 0;
	for (Station *st : Station::Iterate()) {
		if (st->industries_near != old_station_industries_near[i]) {
			Debug(desync, 2, "warning: station industries near mismatch: station {}", st->index);
		}
		i++;
	}

	/* Check stations_near */
	i = 0;
	for (Town *t : Town::Iterate()) {
		if (t->stations_near != old_town_stations_near[i]) {
			Debug(desync, 2, "warning: town stations near mismatch: town {}", t->index);
		}
		i++;
	}
	i = 0;
	for (Industry *ind : Industry::Iterate()) {
		if (ind->stations_near != old_industry_stations_near[i]) {
			Debug(desync, 2, "warning: industry stations near mismatch: industry {}", ind->index);
		}
		i++;
	}
}
