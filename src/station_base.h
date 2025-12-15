/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file station_base.h Base classes/functions for stations. */

#ifndef STATION_BASE_H
#define STATION_BASE_H

#include "core/flatset_type.hpp"
#include "core/random_func.hpp"
#include "base_station_base.h"
#include "newgrf_airport.h"
#include "cargopacket.h"
#include "industry_type.h"
#include "linkgraph/linkgraph_type.h"
#include "newgrf_storage.h"
#include "bitmap_type.h"

static const uint8_t INITIAL_STATION_RATING = 175;
static const uint8_t MAX_STATION_RATING = 255;

/**
 * Flow statistics telling how much flow should be sent along a link. This is
 * done by creating "flow shares" and using std::map's upper_bound() method to
 * look them up with a random number. A flow share is the difference between a
 * key in a map and the previous key. So one key in the map doesn't actually
 * mean anything by itself.
 */
class FlowStat {
public:
	typedef std::map<uint32_t, StationID> SharesMap;

	static const SharesMap empty_sharesmap;

	/**
	 * Invalid constructor. This can't be called as a FlowStat must not be
	 * empty. However, the constructor must be defined and reachable for
	 * FlowStat to be used in a std::map.
	 */
	inline FlowStat() {NOT_REACHED();}

	/**
	 * Create a FlowStat with an initial entry.
	 * @param st Station the initial entry refers to.
	 * @param flow Amount of flow for the initial entry.
	 * @param restricted If the flow to be added is restricted.
	 */
	inline FlowStat(StationID st, uint flow, bool restricted = false)
	{
		assert(flow > 0);
		this->shares[flow] = st;
		this->unrestricted = restricted ? 0 : flow;
	}

	/**
	 * Add some flow to the end of the shares map. Only do that if you know
	 * that the station isn't in the map yet. Anything else may lead to
	 * inconsistencies.
	 * @param st Remote station.
	 * @param flow Amount of flow to be added.
	 * @param restricted If the flow to be added is restricted.
	 */
	inline void AppendShare(StationID st, uint flow, bool restricted = false)
	{
		assert(flow > 0);
		this->shares[(--this->shares.end())->first + flow] = st;
		if (!restricted) this->unrestricted += flow;
	}

	uint GetShare(StationID st) const;

	void ChangeShare(StationID st, int flow);

	void RestrictShare(StationID st);

	void ReleaseShare(StationID st);

	void ScaleToMonthly(uint runtime);

	/**
	 * Get the actual shares as a const pointer so that they can be iterated
	 * over.
	 * @return Actual shares.
	 */
	inline const SharesMap *GetShares() const { return &this->shares; }

	/**
	 * Return total amount of unrestricted shares.
	 * @return Amount of unrestricted shares.
	 */
	inline uint GetUnrestricted() const { return this->unrestricted; }

	/**
	 * Swap the shares maps, and thus the content of this FlowStat with the
	 * other one.
	 * @param other FlowStat to swap with.
	 */
	inline void SwapShares(FlowStat &other)
	{
		this->shares.swap(other.shares);
		std::swap(this->unrestricted, other.unrestricted);
	}

	/**
	 * Get a station a package can be routed to. This done by drawing a
	 * random number between 0 and sum_shares and then looking that up in
	 * the map with lower_bound. So each share gets selected with a
	 * probability dependent on its flow. Do include restricted flows here.
	 * @param is_restricted Output if a restricted flow was chosen.
	 * @return A station ID from the shares map.
	 */
	inline StationID GetViaWithRestricted(bool &is_restricted) const
	{
		assert(!this->shares.empty());
		uint rand = RandomRange((--this->shares.end())->first);
		is_restricted = rand >= this->unrestricted;
		return this->shares.upper_bound(rand)->second;
	}

	/**
	 * Get a station a package can be routed to. This done by drawing a
	 * random number between 0 and sum_shares and then looking that up in
	 * the map with lower_bound. So each share gets selected with a
	 * probability dependent on its flow. Don't include restricted flows.
	 * @return A station ID from the shares map.
	 */
	inline StationID GetVia() const
	{
		assert(!this->shares.empty());
		return this->unrestricted > 0 ?
				this->shares.upper_bound(RandomRange(this->unrestricted))->second :
				StationID::Invalid();
	}

	StationID GetVia(StationID excluded, StationID excluded2 = StationID::Invalid()) const;

	void Invalidate();

private:
	SharesMap shares{}; ///< Shares of flow to be sent via specified station (or consumed locally).
	uint unrestricted = 0; ///< Limit for unrestricted shares.
};

/** Flow descriptions by origin stations. */
class FlowStatMap : public std::map<StationID, FlowStat> {
public:
	uint GetFlow() const;
	uint GetFlowVia(StationID via) const;
	uint GetFlowFrom(StationID from) const;
	uint GetFlowFromVia(StationID from, StationID via) const;

	void AddFlow(StationID origin, StationID via, uint amount);
	void PassOnFlow(StationID origin, StationID via, uint amount);
	std::vector<StationID> DeleteFlows(StationID via);
	void RestrictFlows(StationID via);
	void ReleaseFlows(StationID via);
	void FinalizeLocalConsumption(StationID self);
};

/**
 * Stores station stats for a single cargo.
 */
struct GoodsEntry {
	/** Status of this cargo for the station. */
	enum class State : uint8_t {
		/**
		 * Set when the station accepts the cargo currently for final deliveries.
		 * It is updated every STATION_ACCEPTANCE_TICKS ticks by checking surrounding tiles for acceptance >= 8/8.
		 */
		Acceptance = 0,

		/**
		 * This indicates whether a cargo has a rating at the station.
		 * Set when cargo was ever waiting at the station.
		 * It is set when cargo supplied by surrounding tiles is moved to the station, or when
		 * arriving vehicles unload/transfer cargo without it being a final delivery.
		 *
		 * This flag is cleared after 255 * STATION_RATING_TICKS of not having seen a pickup.
		 */
		Rating = 1,

		/**
		 * Set when a vehicle ever delivered cargo to the station for final delivery.
		 * This flag is never cleared.
		 */
		EverAccepted = 2,

		/**
		 * Set when cargo was delivered for final delivery last month.
		 * This flag is set to the value of State::CurrentMonth at the start of each month.
		 */
		LastMonth = 3,

		/**
		 * Set when cargo was delivered for final delivery this month.
		 * This flag is reset on the beginning of every month.
		 */
		CurrentMonth = 4,

		/**
		 * Set when cargo was delivered for final delivery during the current STATION_ACCEPTANCE_TICKS interval.
		 * This flag is reset every STATION_ACCEPTANCE_TICKS ticks.
		 */
		AcceptedBigtick = 5,
	};
	using States = EnumBitSet<State, uint8_t>;

	struct GoodsEntryData {
		StationCargoList cargo{}; ///< The cargo packets of cargo waiting in this station
		FlowStatMap flows{}; ///< Planned flows through this station.

		bool IsEmpty() const
		{
			return this->cargo.TotalCount() == 0 && this->flows.empty();
		}
	};

	uint max_waiting_cargo = 0; ///< Max cargo from this station waiting at any station.
	NodeID node = INVALID_NODE; ///< ID of node in link graph referring to this goods entry.
	LinkGraphID link_graph = LinkGraphID::Invalid(); ///< Link graph this station belongs to.

	States status{}; ///< Status of this cargo, see #State.

	/**
	 * Number of rating-intervals (up to 255) since the last vehicle tried to load this cargo.
	 * The unit used is STATION_RATING_TICKS.
	 * This does not imply there was any cargo to load.
	 */
	uint8_t time_since_pickup = 255;

	uint8_t rating = INITIAL_STATION_RATING; ///< %Station rating for this cargo.

	/**
	 * Maximum speed (up to 255) of the last vehicle that tried to load this cargo.
	 * This does not imply there was any cargo to load.
	 * The unit used is a special vehicle-specific speed unit for station ratings.
	 *  - Trains: km-ish/h
	 *  - RV: km-ish/h
	 *  - Ships: 0.5 * km-ish/h
	 *  - Aircraft: 8 * mph
	 */
	uint8_t last_speed = 0;

	/**
	 * Age in years (up to 255) of the last vehicle that tried to load this cargo.
	 * This does not imply there was any cargo to load.
	 */
	uint8_t last_age = 255;

	uint8_t amount_fract = 0; ///< Fractional part of the amount in the cargo list

	/**
	 * Reports whether a vehicle has ever tried to load the cargo at this station.
	 * This does not imply that there was cargo available for loading. Refer to GoodsEntry::State::Rating for that.
	 * @return true if vehicle tried to load.
	 */
	bool HasVehicleEverTriedLoading() const { return this->last_speed != 0; }

	/**
	 * Does this cargo have a rating at this station?
	 * @return true if the cargo has a rating, i.e. cargo has been moved to the station.
	 */
	inline bool HasRating() const
	{
		return this->status.Test(GoodsEntry::State::Rating);
	}

	/**
	 * Get the best next hop for a cargo packet from station source.
	 * @param source Source of the packet.
	 * @return The chosen next hop or StationID::Invalid() if none was found.
	 */
	inline StationID GetVia(StationID source) const
	{
		if (!this->HasData()) return StationID::Invalid();

		FlowStatMap::const_iterator flow_it(this->GetData().flows.find(source));
		return flow_it != this->GetData().flows.end() ? flow_it->second.GetVia() : StationID::Invalid();
	}

	/**
	 * Get the best next hop for a cargo packet from station source, optionally
	 * excluding one or two stations.
	 * @param source Source of the packet.
	 * @param excluded If this station would be chosen choose the second best one instead.
	 * @param excluded2 Second station to be excluded, if != StationID::Invalid().
	 * @return The chosen next hop or StationID::Invalid() if none was found.
	 */
	inline StationID GetVia(StationID source, StationID excluded, StationID excluded2 = StationID::Invalid()) const
	{
		if (!this->HasData()) return StationID::Invalid();

		FlowStatMap::const_iterator flow_it(this->GetData().flows.find(source));
		return flow_it != this->GetData().flows.end() ? flow_it->second.GetVia(excluded, excluded2) : StationID::Invalid();
	}

	/**
	 * Test if this goods entry has optional cargo packet/flow data.
	 * @returns true iff optional data is present.
	 */
	[[debug_inline]] inline bool HasData() const { return this->data != nullptr; }

	/**
	 * Clear optional cargo packet/flow data.
	 */
	void ClearData() { this->data.reset(); }

	/**
	 * Get optional cargo packet/flow data.
	 * @pre HasData()
	 * @returns cargo packet/flow data.
	 */
	[[debug_inline]] inline const GoodsEntryData &GetData() const
	{
		assert(this->HasData());
		return *this->data;
	}

	/**
	 * Get non-const optional cargo packet/flow data.
	 * @pre HasData()
	 * @returns non-const cargo packet/flow data.
	 */
	[[debug_inline]] inline GoodsEntryData &GetData()
	{
		assert(this->HasData());
		return *this->data;
	}

	/**
	 * Get optional cargo packet/flow data. The data is create if it is not already present.
	 * @returns cargo packet/flow data.
	 */
	inline GoodsEntryData &GetOrCreateData()
	{
		if (!this->HasData()) this->data = std::make_unique<GoodsEntryData>();
		return *this->data;
	}

	uint8_t ConvertState() const;

	/**
	 * Returns sum of cargo still available for loading at the station.
	 * (i.e. not counting cargo which is already reserved for loading)
	 * @return Cargo on board the vehicle.
	 */
	inline uint AvailableCount() const
	{
		return this->HasData() ? this->GetData().cargo.AvailableCount() : 0;
	}

	/**
	 * Returns total count of cargo at the station, including
	 * cargo which is already reserved for loading.
	 * @return Total cargo count.
	 */
	inline uint TotalCount() const
	{
		return this->HasData() ? this->GetData().cargo.TotalCount() : 0;
	}

private:
	std::unique_ptr<GoodsEntryData> data = nullptr; ///< Optional cargo packet and flow data.
};

/** All airport-related information. Only valid if tile != INVALID_TILE. */
struct Airport : public TileArea {
	Airport() : TileArea(INVALID_TILE, 0, 0) {}

	AirportBlocks blocks{}; ///< stores which blocks on the airport are taken. was 16 bit earlier on, then 32
	uint8_t type = 0; ///< Type of this airport, @see AirportTypes
	uint8_t layout = 0; ///< Airport layout number.
	Direction rotation = INVALID_DIR; ///< How this airport is rotated.

	PersistentStorage *psa = nullptr; ///< Persistent storage for NewGRF airports.

	/**
	 * Get the AirportSpec that from the airport type of this airport. If there
	 * is no airport (\c tile == INVALID_TILE) then return the dummy AirportSpec.
	 * @return The AirportSpec for this airport.
	 */
	const AirportSpec *GetSpec() const
	{
		if (this->tile == INVALID_TILE) return &AirportSpec::dummy;
		return AirportSpec::Get(this->type);
	}

	/**
	 * Get the finite-state machine for this airport or the finite-state machine
	 * for the dummy airport in case this isn't an airport.
	 * @pre this->type < NEW_AIRPORT_OFFSET.
	 * @return The state machine for this airport.
	 */
	const AirportFTAClass *GetFTA() const
	{
		return this->GetSpec()->fsm;
	}

	/** Check if this airport has at least one hangar. */
	inline bool HasHangar() const
	{
		return !this->GetSpec()->depots.empty();
	}

	/**
	 * Add the tileoffset to the base tile of this airport but rotate it first.
	 * The base tile is the northernmost tile of this airport. This function
	 * helps to make sure that getting the tile of a hangar works even for
	 * rotated airport layouts without requiring a rotated array of hangar tiles.
	 * @param tidc The tilediff to add to the airport tile.
	 * @return The tile of this airport plus the rotated offset.
	 */
	inline TileIndex GetRotatedTileFromOffset(TileIndexDiffC tidc) const
	{
		const AirportSpec *as = this->GetSpec();
		switch (this->rotation) {
			case DIR_N: return this->tile + ToTileIndexDiff(tidc);

			case DIR_E: return this->tile + TileDiffXY(tidc.y, as->size_x - 1 - tidc.x);

			case DIR_S: return this->tile + TileDiffXY(as->size_x - 1 - tidc.x, as->size_y - 1 - tidc.y);

			case DIR_W: return this->tile + TileDiffXY(as->size_y - 1 - tidc.y, tidc.x);

			default: NOT_REACHED();
		}
	}

	/**
	 * Get the first tile of the given hangar.
	 * @param hangar_num The hangar to get the location of.
	 * @pre hangar_num < GetNumHangars().
	 * @return A tile with the given hangar.
	 */
	inline TileIndex GetHangarTile(uint hangar_num) const
	{
		for (const auto &depot : this->GetSpec()->depots) {
			if (depot.hangar_num == hangar_num) {
				return this->GetRotatedTileFromOffset(depot.ti);
			}
		}
		NOT_REACHED();
	}

	/**
	 * Get the exit direction of the hangar at a specific tile.
	 * @param tile The tile to query.
	 * @pre IsHangarTile(tile).
	 * @return The exit direction of the hangar, taking airport rotation into account.
	 */
	inline Direction GetHangarExitDirection(TileIndex tile) const
	{
		const AirportSpec *as = this->GetSpec();
		const HangarTileTable *htt = GetHangarDataByTile(tile);
		return ChangeDir(htt->dir, DirDifference(this->rotation, as->layouts[0].rotation));
	}

	/**
	 * Get the hangar number of the hangar at a specific tile.
	 * @param tile The tile to query.
	 * @pre IsHangarTile(tile).
	 * @return The hangar number of the hangar at the given tile.
	 */
	inline uint GetHangarNum(TileIndex tile) const
	{
		const HangarTileTable *htt = GetHangarDataByTile(tile);
		return htt->hangar_num;
	}

	/** Get the number of hangars on this airport. */
	inline uint GetNumHangars() const
	{
		uint num = 0;
		uint counted = 0;
		for (const auto &depot : this->GetSpec()->depots) {
			if (!HasBit(counted, depot.hangar_num)) {
				num++;
				SetBit(counted, depot.hangar_num);
			}
		}
		return num;
	}

private:
	/**
	 * Retrieve hangar information of a hangar at a given tile.
	 * @param tile %Tile containing the hangar.
	 * @return The requested hangar information.
	 * @pre The \a tile must be at a hangar tile at an airport.
	 */
	inline const HangarTileTable *GetHangarDataByTile(TileIndex tile) const
	{
		for (const auto &depot : this->GetSpec()->depots) {
			if (this->GetRotatedTileFromOffset(depot.ti) == tile) {
				return &depot;
			}
		}
		NOT_REACHED();
	}
};

struct IndustryListEntry {
	uint distance = 0;
	Industry *industry = nullptr;

	bool operator== (const IndustryListEntry &other) const { return this->distance == other.distance && this->industry == other.industry; };
};

struct IndustryCompare {
	bool operator() (const IndustryListEntry &lhs, const IndustryListEntry &rhs) const;
};

typedef std::set<IndustryListEntry, IndustryCompare> IndustryList;

/** Station data structure */
struct Station final : SpecializedStation<Station, false> {
public:
	RoadStop *GetPrimaryRoadStop(RoadStopType type) const
	{
		return type == RoadStopType::Bus ? bus_stops : truck_stops;
	}

	RoadStop *GetPrimaryRoadStop(const struct RoadVehicle *v) const;

	RoadStop *bus_stops = nullptr; ///< All the road stops
	TileArea bus_station{}; ///< Tile area the bus 'station' part covers
	RoadStop *truck_stops = nullptr; ///< All the truck stops
	TileArea truck_station{}; ///< Tile area the truck 'station' part covers

	Airport airport{}; ///< Tile area the airport covers
	TileArea ship_station{}; ///< Tile area the ship 'station' part covers
	TileArea docking_station{}; ///< Tile area the docking tiles cover

	IndustryType indtype = IT_INVALID; ///< Industry type to get the name from

	BitmapTileArea catchment_tiles{}; ///< NOSAVE: Set of individual tiles covered by catchment area

	StationHadVehicleOfType had_vehicle_of_type{};

	uint8_t time_since_load = 0;
	uint8_t time_since_unload = 0;

	uint8_t last_vehicle_type = 0;
	std::list<Vehicle *> loading_vehicles{};
	std::array<GoodsEntry, NUM_CARGO> goods; ///< Goods at this station
	CargoTypes always_accepted{}; ///< Bitmask of always accepted cargo types (by houses, HQs, industry tiles when industry doesn't accept cargo)

	IndustryList industries_near{}; ///< Cached list of industries near the station that can accept cargo, @see DeliverGoodsToIndustry()
	Industry *industry = nullptr; ///< NOSAVE: Associated industry for neutral stations. (Rebuilt on load from Industry->st)

	Station(TileIndex tile = INVALID_TILE);
	~Station();

	void AddFacility(StationFacility new_facility_bit, TileIndex facil_xy);

	void MarkTilesDirty(bool cargo_change) const;

	void UpdateVirtCoord() override;

	void MoveSign(TileIndex new_xy) override;

	void AfterStationTileSetChange(bool adding, StationType type);

	uint GetPlatformLength(TileIndex tile, DiagDirection dir) const override;
	uint GetPlatformLength(TileIndex tile) const override;
	void RecomputeCatchment(bool no_clear_nearby_lists = false);
	static void RecomputeCatchmentForAll();

	uint GetCatchmentRadius() const;
	Rect GetCatchmentRect() const;
	bool CatchmentCoversTown(TownID t) const;
	void AddIndustryToDeliver(Industry *ind, TileIndex tile);
	void RemoveIndustryToDeliver(Industry *ind);
	void RemoveFromAllNearbyLists();

	inline bool TileIsInCatchment(TileIndex tile) const
	{
		return this->catchment_tiles.HasTile(tile);
	}

	inline bool TileBelongsToRailStation(TileIndex tile) const override
	{
		return IsRailStationTile(tile) && GetStationIndex(tile) == this->index;
	}

	inline bool TileBelongsToRoadStop(TileIndex tile) const
	{
		return IsStationRoadStopTile(tile) && GetStationIndex(tile) == this->index;
	}

	inline bool TileBelongsToAirport(TileIndex tile) const
	{
		return IsAirportTile(tile) && GetStationIndex(tile) == this->index;
	}

	uint32_t GetNewGRFVariable(const ResolverObject &object, uint8_t variable, uint8_t parameter, bool &available) const override;

	TileArea GetTileArea(StationType type) const override;
};

/** Iterator to iterate over all tiles belonging to an airport. */
class AirportTileIterator : public OrthogonalTileIterator {
private:
	const Station *st = nullptr; ///< The station the airport is a part of.

public:
	/**
	 * Construct the iterator.
	 * @param st Station the airport is part of.
	 */
	AirportTileIterator(const Station *st) : OrthogonalTileIterator(st->airport), st(st)
	{
		if (!st->TileBelongsToAirport(this->tile)) ++(*this);
	}

	inline TileIterator& operator ++() override
	{
		this->OrthogonalTileIterator::operator++();
		while (this->tile != INVALID_TILE && !st->TileBelongsToAirport(this->tile)) {
			this->OrthogonalTileIterator::operator++();
		}
		return *this;
	}

	std::unique_ptr<TileIterator> Clone() const override
	{
		return std::make_unique<AirportTileIterator>(*this);
	}
};

void RebuildStationKdtree();

/**
 * Call a function on all stations that have any part of the requested area within their catchment.
 * @tparam Func The type of function to call
 * @param area The TileArea to check
 * @param func The function to call, must take two parameters: Station* and TileIndex and return true
 *             if coverage of that tile is acceptable for a given station or false if search should continue
 */
template <typename Func>
void ForAllStationsAroundTiles(const TileArea &ta, Func func)
{
	/* There are no stations, so we will never find anything. */
	if (Station::GetNumItems() == 0) return;

	/* Not using, or don't have a nearby stations list, so we need to scan. */
	FlatSet<StationID> seen_stations;

	/* Scan an area around the building covering the maximum possible station
	 * to find the possible nearby stations. */
	uint max_c = _settings_game.station.modified_catchment ? MAX_CATCHMENT : CA_UNMODIFIED;
	TileArea ta_ext = TileArea(ta).Expand(max_c);
	for (TileIndex tile : ta_ext) {
		if (!IsTileType(tile, MP_STATION)) continue;
		seen_stations.insert(GetStationIndex(tile));
	}

	for (StationID stationid : seen_stations) {
		Station *st = Station::GetIfValid(stationid);
		if (st == nullptr) continue; /* Waypoint */

		/* Check if station is attached to an industry */
		if (!_settings_game.station.serve_neutral_industries && st->industry != nullptr) continue;

		/* Test if the tile is within the station's catchment */
		for (TileIndex tile : ta) {
			if (st->TileIsInCatchment(tile)) {
				if (func(st, tile)) break;
			}
		}
	}
}

#endif /* STATION_BASE_H */
