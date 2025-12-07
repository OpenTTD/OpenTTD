/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file station_sl.cpp Code handling saving and loading of stations. */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/station_sl_compat.h"

#include "../station_base.h"
#include "../waypoint_base.h"
#include "../roadstop_base.h"
#include "../vehicle_base.h"
#include "../newgrf_station.h"
#include "../newgrf_roadstop.h"
#include "../timer/timer_game_calendar.h"

#include "table/strings.h"

#include "../safeguards.h"

/**
 * Update the buoy orders to be waypoint orders.
 * @param o the order 'list' to check.
 */
static void UpdateWaypointOrder(Order &o)
{
	if (!o.IsType(OT_GOTO_STATION)) return;

	const Station *st = Station::Get(o.GetDestination().ToStationID());
	if ((st->had_vehicle_of_type & HVOT_WAYPOINT) == 0) return;

	o.MakeGoToWaypoint(o.GetDestination().ToStationID());
}

/**
 * Perform all steps to upgrade from the old station buoys to the new version
 * that uses waypoints. This includes some old saveload mechanics.
 */
void MoveBuoysToWaypoints()
{
	/* Buoy orders become waypoint orders */
	for (OrderList *ol : OrderList::Iterate()) {
		VehicleType vt = ol->GetFirstSharedVehicle()->type;
		if (vt != VEH_SHIP && vt != VEH_TRAIN) continue;

		for (Order &o : ol->GetOrders()) UpdateWaypointOrder(o);
	}

	for (Vehicle *v : Vehicle::Iterate()) {
		VehicleType vt = v->type;
		if (vt != VEH_SHIP && vt != VEH_TRAIN) continue;

		UpdateWaypointOrder(v->current_order);
	}

	/* Now make the stations waypoints */
	for (Station *st : Station::Iterate()) {
		if ((st->had_vehicle_of_type & HVOT_WAYPOINT) == 0) continue;

		StationID index    = st->index;
		TileIndex xy       = st->xy;
		Town *town         = st->town;
		StringID string_id = st->string_id;
		std::string name   = st->name;
		TimerGameCalendar::Date build_date = st->build_date;
		/* TTDPatch could use "buoys with rail station" for rail waypoints */
		bool train         = st->train_station.tile != INVALID_TILE;
		TileArea train_st  = st->train_station;

		/* Delete the station, so we can make it a real waypoint. */
		delete st;

		/* Stations and waypoints are in the same pool, so if a station
		 * is deleted there must be place for a Waypoint. */
		assert(Waypoint::CanAllocateItem());
		Waypoint *wp   = new (index) Waypoint(xy);
		wp->town       = town;
		wp->string_id  = train ? STR_SV_STNAME_WAYPOINT : STR_SV_STNAME_BUOY;
		wp->name       = std::move(name);
		wp->delete_ctr = 0; // Just reset delete counter for once.
		wp->build_date = build_date;
		wp->owner      = train ? GetTileOwner(xy) : OWNER_NONE;

		if (IsInsideBS(string_id, STR_SV_STNAME_BUOY, 9)) wp->town_cn = string_id - STR_SV_STNAME_BUOY;

		if (train) {
			/* When we make a rail waypoint of the station, convert the map as well. */
			for (TileIndex t : train_st) {
				Tile tile(t);
				if (!IsTileType(tile, MP_STATION) || GetStationIndex(tile) != index) continue;

				SB(tile.m6(), 3, 3, to_underlying(StationType::RailWaypoint));
				wp->rect.BeforeAddTile(t, StationRect::ADD_FORCE);
			}

			wp->train_station = train_st;
			wp->facilities.Set(StationFacility::Train);
		} else if (IsBuoyTile(xy) && GetStationIndex(xy) == index) {
			wp->rect.BeforeAddTile(xy, StationRect::ADD_FORCE);
			wp->facilities.Set(StationFacility::Dock);
		}
	}
}

void AfterLoadStations()
{
	/* Update the speclists of all stations to point to the currently loaded custom stations. */
	for (BaseStation *st : BaseStation::Iterate()) {
		for (auto &sm : GetStationSpecList<StationSpec>(st)) {
			if (sm.grfid == 0) continue;
			sm.spec = StationClass::GetByGrf(sm.grfid, sm.localidx);
		}
		for (auto &sm : GetStationSpecList<RoadStopSpec>(st)) {
			if (sm.grfid == 0) continue;
			sm.spec = RoadStopClass::GetByGrf(sm.grfid, sm.localidx);
		}

		if (Station::IsExpected(st)) {
			Station *sta = Station::From(st);
			for (const RoadStop *rs = sta->bus_stops; rs != nullptr; rs = rs->next) sta->bus_station.Add(rs->xy);
			for (const RoadStop *rs = sta->truck_stops; rs != nullptr; rs = rs->next) sta->truck_station.Add(rs->xy);
		}

		StationUpdateCachedTriggers(st);
		RoadStopUpdateCachedTriggers(st);
	}

	/* Station blocked, wires and pylon flags need to be stored in the map. This is effectively cached data, so no
	 * version check is necessary. */
	for (const auto t : Map::Iterate()) {
		if (HasStationTileRail(t)) SetRailStationTileFlags(t, GetStationSpec(t));
	}
}

/**
 * (Re)building of road stop caches after loading a savegame.
 */
void AfterLoadRoadStops()
{
	/* First construct the drive through entries */
	for (RoadStop *rs : RoadStop::Iterate()) {
		if (IsDriveThroughStopTile(rs->xy)) rs->MakeDriveThrough();
	}
	/* And then rebuild the data in those entries */
	for (RoadStop *rs : RoadStop::Iterate()) {
		if (!rs->status.Test(RoadStop::RoadStopStatusFlag::BaseEntry)) continue;

		rs->GetEntry(DIAGDIR_NE).Rebuild(rs);
		rs->GetEntry(DIAGDIR_NW).Rebuild(rs);
	}
}

static const SaveLoad _roadstop_desc[] = {
	SLE_VAR(RoadStop, xy,           SLE_UINT32),
	SLE_VAR(RoadStop, status,       SLE_UINT8),
	SLE_REF(RoadStop, next,         REF_ROADSTOPS),
};

static uint16_t _waiting_acceptance;
static uint32_t _old_num_flows;
static uint16_t _cargo_source;
static uint32_t _cargo_source_xy;
static uint8_t  _cargo_periods;
static Money  _cargo_feeder_share;

std::list<CargoPacket *> _packets;
uint32_t _old_num_dests;

struct FlowSaveLoad {
	StationID source;
	StationID via;
	uint32_t share;
	bool restricted;
};

typedef std::pair<const StationID, std::list<CargoPacket *> > StationCargoPair;

static OldPersistentStorage _old_st_persistent_storage;

/**
 * Swap the temporary packets with the packets without specific destination in
 * the given goods entry. Assert that at least one of those is empty.
 * @param ge Goods entry to swap with.
 */
static void SwapPackets(GoodsEntry *ge)
{
	StationCargoPacketMap &ge_packets = const_cast<StationCargoPacketMap &>(*ge->GetOrCreateData().cargo.Packets());

	if (_packets.empty()) {
		std::map<StationID, std::list<CargoPacket *> >::iterator it(ge_packets.find(StationID::Invalid()));
		if (it == ge_packets.end()) {
			return;
		} else {
			it->second.swap(_packets);
		}
	} else {
		assert(ge_packets[StationID::Invalid()].empty());
		ge_packets[StationID::Invalid()].swap(_packets);
	}
}

template <typename T>
class SlStationSpecList : public VectorSaveLoadHandler<SlStationSpecList<T>, BaseStation, SpecMapping<T>> {
public:
	static inline const SaveLoad description[] = {
		SLE_CONDVAR(SpecMapping<T>, grfid,    SLE_UINT32,                SLV_27,                    SL_MAX_VERSION),
		SLE_CONDVAR(SpecMapping<T>, localidx, SLE_FILE_U8 | SLE_VAR_U16, SLV_27,                    SLV_EXTEND_ENTITY_MAPPING),
		SLE_CONDVAR(SpecMapping<T>, localidx, SLE_UINT16,                SLV_EXTEND_ENTITY_MAPPING, SL_MAX_VERSION),
	};
	static inline const SaveLoadCompatTable compat_description = _station_spec_list_sl_compat;

	static inline uint8_t last_num_specs; ///< Number of specs of the last loaded station.

	std::vector<SpecMapping<T>> &GetVector(BaseStation *bst) const override { return GetStationSpecList<T>(bst); }

	size_t GetLength() const override
	{
		return IsSavegameVersionBefore(SLV_SAVELOAD_LIST_LENGTH) ? last_num_specs : SlGetStructListLength(UINT8_MAX);
	}
};

/* Instantiate SlStationSpecList classes. */
template class SlStationSpecList<StationSpec>;
template class SlStationSpecList<RoadStopSpec>;

class SlStationCargo : public DefaultSaveLoadHandler<SlStationCargo, GoodsEntry> {
public:
	static inline const SaveLoad description[] = {
		    SLE_VAR(StationCargoPair, first,  SLE_UINT16),
		SLE_REFLIST(StationCargoPair, second, REF_CARGO_PACKET),
	};
	static inline const SaveLoadCompatTable compat_description = _station_cargo_sl_compat;

	void Save(GoodsEntry *ge) const override
	{
		if (!ge->HasData()) {
			SlSetStructListLength(0);
			return;
		}

		const auto *packets = ge->GetData().cargo.Packets();
		SlSetStructListLength(packets->MapSize());
		for (StationCargoPacketMap::ConstMapIterator it(packets->begin()); it != packets->end(); ++it) {
			SlObject(const_cast<StationCargoPacketMap::value_type *>(&(*it)), this->GetDescription());
		}
	}

	void Load(GoodsEntry *ge) const override
	{
		size_t num_dests = IsSavegameVersionBefore(SLV_SAVELOAD_LIST_LENGTH) ? _old_num_dests : SlGetStructListLength(UINT32_MAX);
		if (num_dests == 0) return;

		GoodsEntry::GoodsEntryData &data = ge->GetOrCreateData();
		StationCargoPair pair;
		for (uint j = 0; j < num_dests; ++j) {
			SlObject(&pair, this->GetLoadDescription());
			const_cast<StationCargoPacketMap &>(*(data.cargo.Packets()))[pair.first].swap(pair.second);
			assert(pair.second.empty());
		}
	}

	void FixPointers(GoodsEntry *ge) const override
	{
		if (!ge->HasData()) return;

		for (StationCargoPacketMap::ConstMapIterator it = ge->GetData().cargo.Packets()->begin(); it != ge->GetData().cargo.Packets()->end(); ++it) {
			SlObject(const_cast<StationCargoPair *>(&(*it)), this->GetDescription());
		}
	}
};

class SlStationFlow : public DefaultSaveLoadHandler<SlStationFlow, GoodsEntry> {
public:
	static inline const SaveLoad description[] = {
		    SLE_VAR(FlowSaveLoad, source,     SLE_UINT16),
		    SLE_VAR(FlowSaveLoad, via,        SLE_UINT16),
		    SLE_VAR(FlowSaveLoad, share,      SLE_UINT32),
		SLE_CONDVAR(FlowSaveLoad, restricted, SLE_BOOL, SLV_187, SL_MAX_VERSION),
	};
	static inline const SaveLoadCompatTable compat_description = _station_flow_sl_compat;

	void Save(GoodsEntry *ge) const override
	{
		if (!ge->HasData()) {
			SlSetStructListLength(0);
			return;
		}

		size_t num_flows = 0;
		for (const auto &it : ge->GetData().flows) {
			num_flows += it.second.GetShares()->size();
		}
		SlSetStructListLength(num_flows);

		for (const auto &outer_it : ge->GetData().flows) {
			const FlowStat::SharesMap *shares = outer_it.second.GetShares();
			uint32_t sum_shares = 0;
			FlowSaveLoad flow{};
			flow.source = outer_it.first;
			for (auto &inner_it : *shares) {
				flow.via = inner_it.second;
				flow.share = inner_it.first - sum_shares;
				flow.restricted = inner_it.first > outer_it.second.GetUnrestricted();
				sum_shares = inner_it.first;
				assert(flow.share > 0);
				SlObject(&flow, this->GetDescription());
			}
		}
	}

	void Load(GoodsEntry *ge) const override
	{
		size_t num_flows = IsSavegameVersionBefore(SLV_SAVELOAD_LIST_LENGTH) ? _old_num_flows : SlGetStructListLength(UINT32_MAX);
		if (num_flows == 0) return;

		GoodsEntry::GoodsEntryData &data = ge->GetOrCreateData();
		FlowSaveLoad flow{};
		FlowStat *fs = nullptr;
		StationID prev_source = StationID::Invalid();
		for (uint32_t j = 0; j < num_flows; ++j) {
			SlObject(&flow, this->GetLoadDescription());
			if (fs == nullptr || prev_source != flow.source) {
				fs = &(data.flows.emplace(flow.source, FlowStat(flow.via, flow.share, flow.restricted))).first->second;
			} else {
				fs->AppendShare(flow.via, flow.share, flow.restricted);
			}
			prev_source = flow.source;
		}
	}
};

class SlStationGoods : public DefaultSaveLoadHandler<SlStationGoods, BaseStation> {
public:
	static inline uint cargo_reserved_count;

	static inline const SaveLoad description[] = {
		SLEG_CONDVAR("waiting_acceptance", _waiting_acceptance, SLE_UINT16,        SL_MIN_VERSION, SLV_68),
		 SLE_CONDVAR(GoodsEntry, status,               SLE_UINT8,                  SLV_68, SL_MAX_VERSION),
		     SLE_VAR(GoodsEntry, time_since_pickup,    SLE_UINT8),
		     SLE_VAR(GoodsEntry, rating,               SLE_UINT8),
		SLEG_CONDVAR("cargo_source", _cargo_source,    SLE_FILE_U8 | SLE_VAR_U16,   SL_MIN_VERSION, SLV_7),
		SLEG_CONDVAR("cargo_source", _cargo_source,    SLE_UINT16,                  SLV_7, SLV_68),
		SLEG_CONDVAR("cargo_source_xy", _cargo_source_xy, SLE_UINT32,               SLV_44, SLV_68),
		SLEG_CONDVAR("cargo_days", _cargo_periods,     SLE_UINT8,                   SL_MIN_VERSION, SLV_68),
		     SLE_VAR(GoodsEntry, last_speed,           SLE_UINT8),
		     SLE_VAR(GoodsEntry, last_age,             SLE_UINT8),
		SLEG_CONDVAR("cargo_feeder_share", _cargo_feeder_share,  SLE_FILE_U32 | SLE_VAR_I64, SLV_14, SLV_65),
		SLEG_CONDVAR("cargo_feeder_share", _cargo_feeder_share,  SLE_INT64,                  SLV_65, SLV_68),
		 SLE_CONDVAR(GoodsEntry, amount_fract,         SLE_UINT8,                 SLV_150, SL_MAX_VERSION),
		SLEG_CONDREFLIST("packets", _packets,          REF_CARGO_PACKET,           SLV_68, SLV_183),
		SLEG_CONDVAR("old_num_dests", _old_num_dests,  SLE_UINT32,                SLV_183, SLV_SAVELOAD_LIST_LENGTH),
		SLEG_CONDVAR("cargo.reserved_count", SlStationGoods::cargo_reserved_count, SLE_UINT,                  SLV_181, SL_MAX_VERSION),
		 SLE_CONDVAR(GoodsEntry, link_graph,           SLE_UINT16,                SLV_183, SL_MAX_VERSION),
		 SLE_CONDVAR(GoodsEntry, node,                 SLE_UINT16,                SLV_183, SL_MAX_VERSION),
		SLEG_CONDVAR("old_num_flows", _old_num_flows,  SLE_UINT32,                SLV_183, SLV_SAVELOAD_LIST_LENGTH),
		 SLE_CONDVAR(GoodsEntry, max_waiting_cargo,    SLE_UINT32,                SLV_183, SL_MAX_VERSION),
		SLEG_CONDSTRUCTLIST("flow", SlStationFlow,                                SLV_183, SL_MAX_VERSION),
		SLEG_CONDSTRUCTLIST("cargo", SlStationCargo,                              SLV_183, SL_MAX_VERSION),
	};

	static inline const SaveLoadCompatTable compat_description = _station_goods_sl_compat;

	/**
	 * Get the number of cargoes used by this savegame version.
	 * @return The number of cargoes used by this savegame version.
	 */
	size_t GetNumCargo() const
	{
		if (IsSavegameVersionBefore(SLV_55)) return 12;
		if (IsSavegameVersionBefore(SLV_EXTEND_CARGOTYPES)) return 32;
		if (IsSavegameVersionBefore(SLV_SAVELOAD_LIST_LENGTH)) return NUM_CARGO;
		/* Read from the savegame how long the list is. */
		return SlGetStructListLength(NUM_CARGO);
	}

	void Save(BaseStation *bst) const override
	{
		Station *st = Station::From(bst);

		SlSetStructListLength(NUM_CARGO);

		for (GoodsEntry &ge : st->goods) {
			SlStationGoods::cargo_reserved_count = ge.HasData() ? ge.GetData().cargo.reserved_count : 0;
			SlObject(&ge, this->GetDescription());
		}
	}

	void Load(BaseStation *bst) const override
	{
		Station *st = Station::From(bst);

		/* Before savegame version 161, persistent storages were not stored in a pool. */
		if (IsSavegameVersionBefore(SLV_161) && !IsSavegameVersionBefore(SLV_145) && st->facilities.Test(StationFacility::Airport)) {
			/* Store the old persistent storage. The GRFID will be added later. */
			assert(PersistentStorage::CanAllocateItem());
			st->airport.psa = new PersistentStorage(0, GSF_INVALID, TileIndex{});
			std::copy(std::begin(_old_st_persistent_storage.storage), std::end(_old_st_persistent_storage.storage), std::begin(st->airport.psa->storage));
		}

		auto end = std::next(std::begin(st->goods), std::min(this->GetNumCargo(), std::size(st->goods)));
		for (auto it = std::begin(st->goods); it != end; ++it) {
			GoodsEntry &ge = *it;
			SlObject(&ge, this->GetLoadDescription());
			if (!IsSavegameVersionBefore(SLV_181) && SlStationGoods::cargo_reserved_count != 0) {
				ge.GetOrCreateData().cargo.reserved_count = SlStationGoods::cargo_reserved_count;
			}
			if (IsSavegameVersionBefore(SLV_183)) {
				SwapPackets(&ge);
			}
			if (IsSavegameVersionBefore(SLV_68)) {
				ge.status.Set(GoodsEntry::State::Acceptance, HasBit(_waiting_acceptance, 15));
				if (GB(_waiting_acceptance, 0, 12) != 0) {
					/* In old versions, enroute_from used 0xFF as StationID::Invalid() */
					StationID source = (IsSavegameVersionBefore(SLV_7) && _cargo_source == 0xFF) ? StationID::Invalid() : static_cast<StationID>(_cargo_source);

					/* Make sure we can allocate the CargoPacket. This is safe
					 * as there can only be ~64k stations and 32 cargoes in these
					 * savegame versions. As the CargoPacketPool has more than
					 * 16 million entries; it fits by an order of magnitude. */
					assert(CargoPacket::CanAllocateItem());

					/* Don't construct the packet with station here, because that'll fail with old savegames */
					CargoPacket *cp = new CargoPacket(GB(_waiting_acceptance, 0, 12), _cargo_periods, source, TileIndex{_cargo_source_xy}, _cargo_feeder_share);
					ge.GetOrCreateData().cargo.Append(cp, StationID::Invalid());
					ge.status.Set(GoodsEntry::State::Rating);
				}
			}
		}
	}

	void FixPointers(BaseStation *bst) const override
	{
		Station *st = Station::From(bst);

		size_t num_cargo = IsSavegameVersionBefore(SLV_55) ? 12 : IsSavegameVersionBefore(SLV_EXTEND_CARGOTYPES) ? 32 : NUM_CARGO;
		auto end = std::next(std::begin(st->goods), std::min(num_cargo, std::size(st->goods)));
		for (auto it = std::begin(st->goods); it != end; ++it) {
			GoodsEntry &ge = *it;
			if (IsSavegameVersionBefore(SLV_183)) {
				SwapPackets(&ge); // We have to swap back again to be in the format pre-183 expects.
				SlObject(&ge, this->GetDescription());
				SwapPackets(&ge);
			} else {
				SlObject(&ge, this->GetDescription());
			}
		}
	}
};

static const SaveLoad _old_station_desc[] = {
	SLE_CONDVAR(Station, xy,                         SLE_FILE_U16 | SLE_VAR_U32,  SL_MIN_VERSION, SLV_6),
	SLE_CONDVAR(Station, xy,                         SLE_UINT32,                  SLV_6, SL_MAX_VERSION),
	SLE_CONDVAR(Station, train_station.tile,         SLE_FILE_U16 | SLE_VAR_U32,  SL_MIN_VERSION, SLV_6),
	SLE_CONDVAR(Station, train_station.tile,         SLE_UINT32,                  SLV_6, SL_MAX_VERSION),
	SLE_CONDVAR(Station, airport.tile,               SLE_FILE_U16 | SLE_VAR_U32,  SL_MIN_VERSION, SLV_6),
	SLE_CONDVAR(Station, airport.tile,               SLE_UINT32,                  SLV_6, SL_MAX_VERSION),
	    SLE_REF(Station, town,                       REF_TOWN),
	    SLE_VAR(Station, train_station.w,            SLE_FILE_U8 | SLE_VAR_U16),
	SLE_CONDVAR(Station, train_station.h,            SLE_FILE_U8 | SLE_VAR_U16,   SLV_2, SL_MAX_VERSION),

	    SLE_VAR(Station, string_id,                  SLE_STRINGID),
	SLE_CONDSSTR(Station, name,                      SLE_STR | SLF_ALLOW_CONTROL, SLV_84, SL_MAX_VERSION),
	SLE_CONDVAR(Station, indtype,                    SLE_UINT8,                 SLV_103, SL_MAX_VERSION),
	SLE_CONDVAR(Station, had_vehicle_of_type,        SLE_FILE_U16 | SLE_VAR_U8,   SL_MIN_VERSION, SLV_122),
	SLE_CONDVAR(Station, had_vehicle_of_type,        SLE_UINT8,                 SLV_122, SL_MAX_VERSION),

	    SLE_VAR(Station, time_since_load,            SLE_UINT8),
	    SLE_VAR(Station, time_since_unload,          SLE_UINT8),
	    SLE_VAR(Station, delete_ctr,                 SLE_UINT8),
	    SLE_VAR(Station, owner,                      SLE_UINT8),
	    SLE_VAR(Station, facilities,                 SLE_UINT8),
	    SLE_VAR(Station, airport.type,               SLE_UINT8),
	SLE_CONDVARNAME(Station, airport.blocks, "airport.flags", SLE_VAR_U64 | SLE_FILE_U16,  SL_MIN_VERSION,  SLV_3),
	SLE_CONDVARNAME(Station, airport.blocks, "airport.flags", SLE_VAR_U64 | SLE_FILE_U32,  SLV_3, SLV_46),
	SLE_CONDVARNAME(Station, airport.blocks, "airport.flags", SLE_UINT64,                 SLV_46, SL_MAX_VERSION),

	SLE_CONDVAR(Station, last_vehicle_type,          SLE_UINT8,                  SLV_26, SL_MAX_VERSION),

	SLE_CONDVAR(Station, build_date,                 SLE_FILE_U16 | SLE_VAR_I32,  SLV_3, SLV_31),
	SLE_CONDVAR(Station, build_date,                 SLE_INT32,                  SLV_31, SL_MAX_VERSION),

	SLE_CONDREF(Station, bus_stops,                  REF_ROADSTOPS,               SLV_6, SL_MAX_VERSION),
	SLE_CONDREF(Station, truck_stops,                REF_ROADSTOPS,               SLV_6, SL_MAX_VERSION),

	/* Used by newstations for graphic variations */
	SLE_CONDVAR(Station, random_bits,                SLE_UINT16,                 SLV_27, SL_MAX_VERSION),
	SLE_CONDVARNAME(Station, waiting_random_triggers, "waiting_triggers", SLE_UINT8, SLV_27, SL_MAX_VERSION),
	SLEG_CONDVAR("num_specs", SlStationSpecList<StationSpec>::last_num_specs, SLE_UINT8, SLV_27, SL_MAX_VERSION),

	SLE_CONDREFLIST(Station, loading_vehicles,       REF_VEHICLE,                SLV_57, SL_MAX_VERSION),

	SLEG_STRUCTLIST("goods", SlStationGoods),
	SLEG_CONDSTRUCTLIST("speclist", SlStationSpecList<StationSpec>, SLV_27, SL_MAX_VERSION),
};

struct STNSChunkHandler : ChunkHandler {
	STNSChunkHandler() : ChunkHandler('STNS', CH_READONLY) {}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_old_station_desc, _old_station_sl_compat);

		_cargo_source_xy = 0;
		_cargo_periods = 0;
		_cargo_feeder_share = 0;

		int index;
		while ((index = SlIterateArray()) != -1) {
			Station *st = new (StationID(index)) Station();

			_waiting_acceptance = 0;
			SlObject(st, slt);
		}
	}

	void FixPointers() const override
	{
		/* From SLV_123 we store stations in STNN; before that in STNS. So do not
		 * fix pointers when the version is SLV_123 or up, as that would fix
		 * pointers twice: once in STNN chunk and once here. */
		if (!IsSavegameVersionBefore(SLV_123)) return;

		for (Station *st : Station::Iterate()) {
			SlObject(st, _old_station_desc);
		}
	}
};

class SlRoadStopTileData : public VectorSaveLoadHandler<SlRoadStopTileData, BaseStation, RoadStopTileData> {
public:
	static inline const SaveLoad description[] = {
	    SLE_VAR(RoadStopTileData, tile,            SLE_UINT32),
	    SLE_VAR(RoadStopTileData, random_bits,     SLE_UINT8),
	    SLE_VAR(RoadStopTileData, animation_frame, SLE_UINT8),
	};
	static inline const SaveLoadCompatTable compat_description = {};

	std::vector<RoadStopTileData> &GetVector(BaseStation *bst) const override { return bst->custom_roadstop_tile_data; }
};

/**
 * SaveLoad handler for the BaseStation, which all other stations / waypoints
 * make use of.
 */
class SlStationBase : public DefaultSaveLoadHandler<SlStationBase, BaseStation> {
public:
	static inline const SaveLoad description[] = {
		    SLE_VAR(BaseStation, xy,                     SLE_UINT32),
		    SLE_REF(BaseStation, town,                   REF_TOWN),
		    SLE_VAR(BaseStation, string_id,              SLE_STRINGID),
		   SLE_SSTR(BaseStation, name,                   SLE_STR | SLF_ALLOW_CONTROL),
		    SLE_VAR(BaseStation, delete_ctr,             SLE_UINT8),
		    SLE_VAR(BaseStation, owner,                  SLE_UINT8),
		    SLE_VAR(BaseStation, facilities,             SLE_UINT8),
		    SLE_VAR(BaseStation, build_date,             SLE_INT32),

		/* Used by newstations for graphic variations */
		    SLE_VAR(BaseStation, random_bits,            SLE_UINT16),
		    SLE_VARNAME(BaseStation, waiting_random_triggers, "waiting_triggers", SLE_UINT8),
	   SLEG_CONDVAR("num_specs", SlStationSpecList<StationSpec>::last_num_specs, SLE_UINT8, SL_MIN_VERSION, SLV_SAVELOAD_LIST_LENGTH),
	};
	static inline const SaveLoadCompatTable compat_description = _station_base_sl_compat;

	void Save(BaseStation *bst) const override
	{
		SlObject(bst, this->GetDescription());
	}

	void Load(BaseStation *bst) const override
	{
		SlObject(bst, this->GetLoadDescription());
	}

	void FixPointers(BaseStation *bst) const override
	{
		SlObject(bst, this->GetDescription());
	}
};

/**
 * SaveLoad handler for a normal station (read: not a waypoint).
 */
class SlStationNormal : public DefaultSaveLoadHandler<SlStationNormal, BaseStation> {
public:
	static inline const SaveLoad description[] = {
		SLEG_STRUCT("base", SlStationBase),
		    SLE_VAR(Station, train_station.tile,         SLE_UINT32),
		    SLE_VAR(Station, train_station.w,            SLE_FILE_U8 | SLE_VAR_U16),
		    SLE_VAR(Station, train_station.h,            SLE_FILE_U8 | SLE_VAR_U16),

		    SLE_REF(Station, bus_stops,                  REF_ROADSTOPS),
		    SLE_REF(Station, truck_stops,                REF_ROADSTOPS),
		SLE_CONDVAR(Station, ship_station.tile,          SLE_UINT32,                SLV_MULTITILE_DOCKS, SL_MAX_VERSION),
		SLE_CONDVAR(Station, ship_station.w,             SLE_FILE_U8 | SLE_VAR_U16, SLV_MULTITILE_DOCKS, SL_MAX_VERSION),
		SLE_CONDVAR(Station, ship_station.h,             SLE_FILE_U8 | SLE_VAR_U16, SLV_MULTITILE_DOCKS, SL_MAX_VERSION),
		SLE_CONDVAR(Station, docking_station.tile,       SLE_UINT32,                SLV_MULTITILE_DOCKS, SL_MAX_VERSION),
		SLE_CONDVAR(Station, docking_station.w,          SLE_FILE_U8 | SLE_VAR_U16, SLV_MULTITILE_DOCKS, SL_MAX_VERSION),
		SLE_CONDVAR(Station, docking_station.h,          SLE_FILE_U8 | SLE_VAR_U16, SLV_MULTITILE_DOCKS, SL_MAX_VERSION),
		    SLE_VAR(Station, airport.tile,               SLE_UINT32),
		SLE_CONDVAR(Station, airport.w,                  SLE_FILE_U8 | SLE_VAR_U16, SLV_140, SL_MAX_VERSION),
		SLE_CONDVAR(Station, airport.h,                  SLE_FILE_U8 | SLE_VAR_U16, SLV_140, SL_MAX_VERSION),
		    SLE_VAR(Station, airport.type,               SLE_UINT8),
		SLE_CONDVAR(Station, airport.layout,             SLE_UINT8,                 SLV_145, SL_MAX_VERSION),
		SLE_VARNAME(Station, airport.blocks, "airport.flags", SLE_UINT64),
		SLE_CONDVAR(Station, airport.rotation,           SLE_UINT8,                 SLV_145, SL_MAX_VERSION),
		SLEG_CONDARR("storage", _old_st_persistent_storage.storage,  SLE_UINT32, 16, SLV_145, SLV_161),
		SLE_CONDREF(Station, airport.psa,                REF_STORAGE,               SLV_161, SL_MAX_VERSION),

		    SLE_VAR(Station, indtype,                    SLE_UINT8),

		    SLE_VAR(Station, time_since_load,            SLE_UINT8),
		    SLE_VAR(Station, time_since_unload,          SLE_UINT8),
		    SLE_VAR(Station, last_vehicle_type,          SLE_UINT8),
		    SLE_VAR(Station, had_vehicle_of_type,        SLE_UINT8),
		SLE_REFLIST(Station, loading_vehicles,           REF_VEHICLE),
		SLE_CONDVAR(Station, always_accepted,            SLE_FILE_U32 | SLE_VAR_U64, SLV_127, SLV_EXTEND_CARGOTYPES),
		SLE_CONDVAR(Station, always_accepted,            SLE_UINT64,                 SLV_EXTEND_CARGOTYPES, SL_MAX_VERSION),
		SLEG_CONDSTRUCTLIST("speclist", SlRoadStopTileData,                          SLV_NEWGRF_ROAD_STOPS, SLV_ROAD_STOP_TILE_DATA),
		SLEG_STRUCTLIST("goods", SlStationGoods),
	};
	static inline const SaveLoadCompatTable compat_description = _station_normal_sl_compat;

	void Save(BaseStation *bst) const override
	{
		if (bst->facilities.Test(StationFacility::Waypoint)) return;
		SlObject(bst, this->GetDescription());
	}

	void Load(BaseStation *bst) const override
	{
		if (bst->facilities.Test(StationFacility::Waypoint)) return;
		SlObject(bst, this->GetLoadDescription());
	}

	void FixPointers(BaseStation *bst) const override
	{
		if (bst->facilities.Test(StationFacility::Waypoint)) return;
		SlObject(bst, this->GetDescription());
	}
};

class SlStationWaypoint : public DefaultSaveLoadHandler<SlStationWaypoint, BaseStation> {
public:
	static inline const SaveLoad description[] = {
		SLEG_STRUCT("base", SlStationBase),
		    SLE_VAR(Waypoint, town_cn,                   SLE_UINT16),

		SLE_CONDVAR(Waypoint, train_station.tile,        SLE_UINT32,                  SLV_124, SL_MAX_VERSION),
		SLE_CONDVAR(Waypoint, train_station.w,           SLE_FILE_U8 | SLE_VAR_U16,   SLV_124, SL_MAX_VERSION),
		SLE_CONDVAR(Waypoint, train_station.h,           SLE_FILE_U8 | SLE_VAR_U16,   SLV_124, SL_MAX_VERSION),
		SLE_CONDVAR(Waypoint, waypoint_flags,            SLE_UINT16,                  SLV_ROAD_WAYPOINTS, SL_MAX_VERSION),
		SLE_CONDVAR(Waypoint, road_waypoint_area.tile,   SLE_UINT32,                  SLV_ROAD_WAYPOINTS, SL_MAX_VERSION),
		SLE_CONDVAR(Waypoint, road_waypoint_area.w,      SLE_FILE_U8 | SLE_VAR_U16,   SLV_ROAD_WAYPOINTS, SL_MAX_VERSION),
		SLE_CONDVAR(Waypoint, road_waypoint_area.h,      SLE_FILE_U8 | SLE_VAR_U16,   SLV_ROAD_WAYPOINTS, SL_MAX_VERSION),
	};
	static inline const SaveLoadCompatTable compat_description = _station_waypoint_sl_compat;

	void Save(BaseStation *bst) const override
	{
		if (!bst->facilities.Test(StationFacility::Waypoint)) return;
		SlObject(bst, this->GetDescription());
	}

	void Load(BaseStation *bst) const override
	{
		if (!bst->facilities.Test(StationFacility::Waypoint)) return;
		SlObject(bst, this->GetLoadDescription());
	}

	void FixPointers(BaseStation *bst) const override
	{
		if (!bst->facilities.Test(StationFacility::Waypoint)) return;
		SlObject(bst, this->GetDescription());
	}
};

static const SaveLoad _station_desc[] = {
	SLE_SAVEBYTE(BaseStation, facilities),
	SLEG_STRUCT("normal", SlStationNormal),
	SLEG_STRUCT("waypoint", SlStationWaypoint),
	SLEG_CONDSTRUCTLIST("speclist", SlStationSpecList<StationSpec>, SLV_27, SL_MAX_VERSION),
	SLEG_CONDSTRUCTLIST("roadstopspeclist", SlStationSpecList<RoadStopSpec>, SLV_NEWGRF_ROAD_STOPS, SL_MAX_VERSION),
	SLEG_CONDSTRUCTLIST("roadstoptiledata", SlRoadStopTileData, SLV_ROAD_STOP_TILE_DATA, SL_MAX_VERSION),
};

struct STNNChunkHandler : ChunkHandler {
	STNNChunkHandler() : ChunkHandler('STNN', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_station_desc);

		/* Write the stations */
		for (BaseStation *st : BaseStation::Iterate()) {
			SlSetArrayIndex(st->index);
			SlObject(st, _station_desc);
		}
	}


	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_station_desc, _station_sl_compat);

		_old_num_flows = 0;

		int index;
		while ((index = SlIterateArray()) != -1) {
			bool waypoint = static_cast<StationFacilities>(SlReadByte()).Test(StationFacility::Waypoint);

			BaseStation *bst = waypoint ? (BaseStation *)new (StationID(index)) Waypoint() : new (StationID(index)) Station();
			SlObject(bst, slt);
		}
	}

	void FixPointers() const override
	{
		/* From SLV_123 we store stations in STNN; before that in STNS. So do not
		 * fix pointers when the version is below SLV_123, as that would fix
		 * pointers twice: once in STNS chunk and once here. */
		if (IsSavegameVersionBefore(SLV_123)) return;

		for (BaseStation *bst : BaseStation::Iterate()) {
			SlObject(bst, _station_desc);
		}
	}
};

struct ROADChunkHandler : ChunkHandler {
	ROADChunkHandler() : ChunkHandler('ROAD', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_roadstop_desc);

		for (RoadStop *rs : RoadStop::Iterate()) {
			SlSetArrayIndex(rs->index);
			SlObject(rs, _roadstop_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_roadstop_desc, _roadstop_sl_compat);

		int index;

		while ((index = SlIterateArray()) != -1) {
			RoadStop *rs = new (RoadStopID(index)) RoadStop(INVALID_TILE);

			SlObject(rs, slt);
		}
	}

	void FixPointers() const override
	{
		for (RoadStop *rs : RoadStop::Iterate()) {
			SlObject(rs, _roadstop_desc);
		}
	}
};

static const STNSChunkHandler STNS;
static const STNNChunkHandler STNN;
static const ROADChunkHandler ROAD;
static const ChunkHandlerRef station_chunk_handlers[] = {
	STNS,
	STNN,
	ROAD,
};

extern const ChunkHandlerTable _station_chunk_handlers(station_chunk_handlers);
