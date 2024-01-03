/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargopacket.h Base class for cargo packets. */

#ifndef CARGOPACKET_H
#define CARGOPACKET_H

#include "core/pool_type.hpp"
#include "economy_type.h"
#include "station_type.h"
#include "order_type.h"
#include "cargo_type.h"
#include "vehicle_type.h"
#include "core/multimap.hpp"
#include "saveload/saveload.h"

/** Unique identifier for a single cargo packet. */
typedef uint32_t CargoPacketID;
struct CargoPacket;

/** Type of the pool for cargo packets for a little over 16 million packets. */
typedef Pool<CargoPacket, CargoPacketID, 1024, 0xFFF000, PT_NORMAL, true, false> CargoPacketPool;
/** The actual pool with cargo packets. */
extern CargoPacketPool _cargopacket_pool;

struct GoodsEntry; // forward-declare for Stage() and RerouteStalePackets()

template <class Tinst, class Tcont> class CargoList;
class StationCargoList; // forward-declare, so we can use it in VehicleCargoList.
extern SaveLoadTable GetCargoPacketDesc();

/**
 * Container for cargo from the same location and time.
 */
struct CargoPacket : CargoPacketPool::PoolItem<&_cargopacket_pool> {
private:
	/* A mathematical vector from (0,0). */
	struct Vector {
		int16_t x;
		int16_t y;
	};

	uint16_t count{0}; ///< The amount of cargo in this packet.
	uint16_t periods_in_transit{0}; ///< Amount of cargo aging periods this packet has been in transit.

	Money feeder_share{0}; ///< Value of feeder pickup to be paid for on delivery of cargo.

	TileIndex source_xy{INVALID_TILE}; ///< The origin of the cargo.
	Vector travelled{0, 0}; ///< If cargo is in station: the vector from the unload tile to the source tile. If in vehicle: an intermediate value.

	SourceID source_id{INVALID_SOURCE}; ///< Index of industry/town/HQ, INVALID_SOURCE if unknown/invalid.
	SourceType source_type{SourceType::Industry}; ///< Type of \c source_id.

#ifdef WITH_ASSERT
	bool in_vehicle{false}; ///< NOSAVE: Whether this cargo is in a vehicle or not.
#endif /* WITH_ASSERT */

	StationID first_station{INVALID_STATION}; ///< The station where the cargo came from first.
	StationID next_hop{INVALID_STATION}; ///< Station where the cargo wants to go next.

	/** The CargoList caches, thus needs to know about it. */
	template <class Tinst, class Tcont> friend class CargoList;
	friend class VehicleCargoList;
	friend class StationCargoList;
	/** We want this to be saved, right? */
	friend SaveLoadTable GetCargoPacketDesc();
public:
	/** Maximum number of items in a single cargo packet. */
	static const uint16_t MAX_COUNT = UINT16_MAX;

	CargoPacket();
	CargoPacket(StationID first_station, uint16_t count, SourceType source_type, SourceID source_id);
	CargoPacket(uint16_t count, uint16_t periods_in_transit, StationID first_station, TileIndex source_xy, Money feeder_share);
	CargoPacket(uint16_t count, Money feeder_share, CargoPacket &original);

	/** Destroy the packet. */
	~CargoPacket() { }

	CargoPacket *Split(uint new_size);
	void Merge(CargoPacket *cp);
	void Reduce(uint count);

	/**
	 * Sets the station where the packet is supposed to go next.
	 * @param next_hop Next station the packet should go to.
	 */
	void SetNextHop(StationID next_hop)
	{
		this->next_hop = next_hop;
	}

	/**
	 * Update for the cargo being loaded on this tile.
	 *
	 * When a CargoPacket is created, it is moved to a station. But at that
	 * moment in time it is not known yet at which tile the cargo will be
	 * picked up. As this tile is used for payment information, we delay
	 * setting the source_xy till first pickup, getting a better idea where
	 * a cargo started from.
	 *
	 * Further more, we keep track of the amount of tiles the cargo moved
	 * inside a vehicle. This is used in GetDistance() below.
	 *
	 * @param tile Tile the cargo is being picked up from.
	 */
	void UpdateLoadingTile(TileIndex tile)
	{
		if (this->source_xy == INVALID_TILE) {
			this->source_xy = tile;
		}

#ifdef WITH_ASSERT
		assert(!this->in_vehicle);
		this->in_vehicle = true;
#endif /* WITH_ASSERT */

		/* We want to calculate the vector from tile-unload to tile-load. As
		 * we currently only know the latter, add it. When we know where we unload,
		 * we subtract is, giving us our vector (unload - load). */
		this->travelled.x += TileX(tile);
		this->travelled.y += TileY(tile);
	}

	/**
	 * Update for the cargo being unloaded on this tile.
	 *
	 * @param tile Tile the cargo is being dropped off at.
	 */
	void UpdateUnloadingTile(TileIndex tile)
	{
#ifdef WITH_ASSERT
		assert(this->in_vehicle);
		this->in_vehicle = false;
#endif /* WITH_ASSERT */

		this->travelled.x -= TileX(tile);
		this->travelled.y -= TileY(tile);
	}

	/**
	 * Adds some feeder share to the packet.
	 * @param new_share Feeder share to be added.
	 */
	void AddFeederShare(Money new_share)
	{
		this->feeder_share += new_share;
	}

	/**
	 * Gets the number of 'items' in this packet.
	 * @return Item count.
	 */
	inline uint16_t Count() const
	{
		return this->count;
	}

	/**
	 * Gets the amount of money already paid to earlier vehicles in
	 * the feeder chain.
	 * @return Feeder share.
	 */
	inline Money GetFeederShare() const
	{
		return this->feeder_share;
	}

	/**
	 * Gets part of the amount of money already paid to earlier vehicles in
	 * the feeder chain.
	 * @param part Amount of cargo to get the share for.
	 * @return Feeder share for the given amount of cargo.
	 */
	inline Money GetFeederShare(uint part) const
	{
		return this->feeder_share * part / static_cast<uint>(this->count);
	}

	/**
	 * Gets the number of cargo aging periods this cargo has been in transit.
	 * By default a period is 2.5 days (CARGO_AGING_TICKS = 185 ticks), however
	 * vehicle NewGRFs can overide the length of the cargo aging period. The
	 * value is capped at UINT16_MAX.
	 * @return Length this cargo has been in transit.
	 */
	inline uint16_t GetPeriodsInTransit() const
	{
		return this->periods_in_transit;
	}

	/**
	 * Gets the type of the cargo's source. industry, town or head quarter.
	 * @return Source type.
	 */
	inline SourceType GetSourceType() const
	{
		return this->source_type;
	}

	/**
	 * Gets the ID of the cargo's source. An IndustryID, TownID or CompanyID.
	 * @return Source ID.
	 */
	inline SourceID GetSourceID() const
	{
		return this->source_id;
	}

	/**
	 * Gets the ID of the station where the cargo was loaded for the first time.
	 * @return StationID.
	 */
	inline StationID GetFirstStation() const
	{
		return this->first_station;
	}

	/**
	 * Get the current distance the cargo has traveled.
	 *
	 * @param current_tile Current tile of the cargo.
	 * @return uint The distance (in tiles) traveled.
	 */
	inline uint GetDistance(TileIndex current_tile) const
	{
		assert(this->source_xy != INVALID_TILE);
#ifdef WITH_ASSERT
		assert(this->in_vehicle);
#endif /* WITH_ASSERT */

		/* Distance is always requested when the cargo is still inside the
		 * vehicle. So first finish the calculation for travelled to
		 * become a vector. */
		auto local_travelled = travelled;
		local_travelled.x -= TileX(current_tile);
		local_travelled.y -= TileY(current_tile);

		/* Cargo-movement is a vector that indicates how much the cargo has
		 * actually traveled in a vehicle. This is the distance you get paid
		 * for. However, one could construct a route where this vector would
		 * be really long. To not overpay the player, cap out at the distance
		 * between source and destination.
		 *
		 * This way of calculating is to counter people moving cargo for free
		 * and instantly in stations, where you deliver it in one part of the
		 * station and pick it up in another. By using the actual distance
		 * traveled in a vehicle, using this trick doesn't give you more money.
		 *
		 * However, especially in large networks with large transfer station,
		 * etc, one could actually make the route a lot longer. In that case,
		 * use the actual distance between source and destination.
		 */

		uint distance_travelled = abs(local_travelled.x) + abs(local_travelled.y);
		uint distance_source_dest = DistanceManhattan(this->source_xy, current_tile);
		return std::min(distance_travelled, distance_source_dest);
	}

	/**
	 * Gets the ID of station the cargo wants to go next.
	 * @return Next station for this packets.
	 */
	inline StationID GetNextHop() const
	{
		return this->next_hop;
	}

	static void InvalidateAllFrom(SourceType src_type, SourceID src);
	static void InvalidateAllFrom(StationID sid);
	static void AfterLoad();
};

/**
 * Simple collection class for a list of cargo packets.
 * @tparam Tinst Actual instantiation of this cargo list.
 */
template <class Tinst, class Tcont>
class CargoList {
public:
	/** The iterator for our container. */
	typedef typename Tcont::iterator Iterator;
	/** The reverse iterator for our container. */
	typedef typename Tcont::reverse_iterator ReverseIterator;
	/** The const iterator for our container. */
	typedef typename Tcont::const_iterator ConstIterator;
	/** The const reverse iterator for our container. */
	typedef typename Tcont::const_reverse_iterator ConstReverseIterator;

	/** Kind of actions that could be done with packets on move. */
	enum MoveToAction {
		MTA_BEGIN = 0,
		MTA_TRANSFER = 0, ///< Transfer the cargo to the station.
		MTA_DELIVER,      ///< Deliver the cargo to some town or industry.
		MTA_KEEP,         ///< Keep the cargo in the vehicle.
		MTA_LOAD,         ///< Load the cargo from the station.
		MTA_END,
		NUM_MOVE_TO_ACTION = MTA_END
	};

protected:
	uint count;                   ///< Cache for the number of cargo entities.
	uint64_t cargo_periods_in_transit; ///< Cache for the sum of number of cargo aging periods in transit of each entity; comparable to man-hours.

	Tcont packets;              ///< The cargo packets in this list.

	void AddToCache(const CargoPacket *cp);

	void RemoveFromCache(const CargoPacket *cp, uint count);

	static bool TryMerge(CargoPacket *cp, CargoPacket *icp);

public:
	/** Create the cargo list. */
	CargoList() {}

	~CargoList();

	void OnCleanPool();

	/**
	 * Returns a pointer to the cargo packet list (so you can iterate over it etc).
	 * @return Pointer to the packet list.
	 */
	inline const Tcont *Packets() const
	{
		return &this->packets;
	}

	/**
	 * Returns average number of cargo aging periods in transit for a cargo entity.
	 * @return The before mentioned number.
	 */
	inline uint PeriodsInTransit() const
	{
		return this->count == 0 ? 0 : this->cargo_periods_in_transit / this->count;
	}

	void InvalidateCache();
};

typedef std::list<CargoPacket *> CargoPacketList;

/**
 * CargoList that is used for vehicles.
 */
class VehicleCargoList : public CargoList<VehicleCargoList, CargoPacketList> {
protected:
	/** The (direct) parent of this class. */
	typedef CargoList<VehicleCargoList, CargoPacketList> Parent;

	Money feeder_share;                     ///< Cache for the feeder share.
	uint action_counts[NUM_MOVE_TO_ACTION]; ///< Counts of cargo to be transferred, delivered, kept and loaded.

	template<class Taction>
	void ShiftCargo(Taction action);

	template<class Taction>
	void PopCargo(Taction action);

	/**
	 * Assert that the designation counts add up.
	 */
	inline void AssertCountConsistency() const
	{
		assert(this->action_counts[MTA_KEEP] +
				this->action_counts[MTA_DELIVER] +
				this->action_counts[MTA_TRANSFER] +
				this->action_counts[MTA_LOAD] == this->count);
	}

	void AddToCache(const CargoPacket *cp);
	void RemoveFromCache(const CargoPacket *cp, uint count);

	void AddToMeta(const CargoPacket *cp, MoveToAction action);
	void RemoveFromMeta(const CargoPacket *cp, MoveToAction action, uint count);

	static MoveToAction ChooseAction(const CargoPacket *cp, StationID cargo_next,
			StationID current_station, bool accepted, StationIDStack next_station);

public:
	/** The station cargo list needs to control the unloading. */
	friend class StationCargoList;
	/** The super class ought to know what it's doing. */
	friend class CargoList<VehicleCargoList, CargoPacketList>;
	/* So we can use private/protected variables in the saveload code */
	friend class SlVehicleCommon;

	friend class CargoShift;
	friend class CargoTransfer;
	friend class CargoDelivery;
	template<class Tsource>
	friend class CargoRemoval;
	friend class CargoReturn;
	friend class VehicleCargoReroute;

	/**
	 * Returns the first station of the first cargo packet in this list.
	 * @return The before mentioned station.
	 */
	inline StationID GetFirstStation() const
	{
		return this->count == 0 ? INVALID_STATION : this->packets.front()->first_station;
	}

	/**
	 * Returns total sum of the feeder share for all packets.
	 * @return The before mentioned number.
	 */
	inline Money GetFeederShare() const
	{
		return this->feeder_share;
	}

	/**
	 * Returns the amount of cargo designated for a given purpose.
	 * @param action Action the cargo is designated for.
	 * @return Amount of cargo designated for the given action.
	 */
	inline uint ActionCount(MoveToAction action) const
	{
		return this->action_counts[action];
	}

	/**
	 * Returns sum of cargo on board the vehicle (ie not only
	 * reserved).
	 * @return Cargo on board the vehicle.
	 */
	inline uint StoredCount() const
	{
		return this->count - this->action_counts[MTA_LOAD];
	}

	/**
	 * Returns sum of cargo, including reserved cargo.
	 * @return Sum of cargo.
	 */
	inline uint TotalCount() const
	{
		return this->count;
	}

	/**
	 * Returns sum of reserved cargo.
	 * @return Sum of reserved cargo.
	 */
	inline uint ReservedCount() const
	{
		return this->action_counts[MTA_LOAD];
	}

	/**
	 * Returns sum of cargo to be moved out of the vehicle at the current station.
	 * @return Cargo to be moved.
	 */
	inline uint UnloadCount() const
	{
		return this->action_counts[MTA_TRANSFER] + this->action_counts[MTA_DELIVER];
	}

	/**
	 * Returns the sum of cargo to be kept in the vehicle at the current station.
	 * @return Cargo to be kept or loaded.
	 */
	inline uint RemainingCount() const
	{
		return this->action_counts[MTA_KEEP] + this->action_counts[MTA_LOAD];
	}

	void Append(CargoPacket *cp, MoveToAction action = MTA_KEEP);

	void AgeCargo();

	void InvalidateCache();

	bool Stage(bool accepted, StationID current_station, StationIDStack next_station, uint8_t order_flags, const GoodsEntry *ge, CargoPayment *payment, TileIndex current_tile);

	/**
	 * Marks all cargo in the vehicle as to be kept. This is mostly useful for
	 * loading old savegames. When loading is aborted the reserved cargo has
	 * to be returned first.
	 */
	inline void KeepAll()
	{
		this->action_counts[MTA_DELIVER] = this->action_counts[MTA_TRANSFER] = this->action_counts[MTA_LOAD] = 0;
		this->action_counts[MTA_KEEP] = this->count;
	}

	/* Methods for moving cargo around. First parameter is always maximum
	 * amount of cargo to be moved. Second parameter is destination (if
	 * applicable), return value is amount of cargo actually moved. */

	template<MoveToAction Tfrom, MoveToAction Tto>
	uint Reassign(uint max_move);
	uint Return(uint max_move, StationCargoList *dest, StationID next_station, TileIndex current_tile);
	uint Unload(uint max_move, StationCargoList *dest, CargoPayment *payment, TileIndex current_tile);
	uint Shift(uint max_move, VehicleCargoList *dest);
	uint Truncate(uint max_move = UINT_MAX);
	uint Reroute(uint max_move, VehicleCargoList *dest, StationID avoid, StationID avoid2, const GoodsEntry *ge);

	/**
	 * Are the two CargoPackets mergeable in the context of
	 * a list of CargoPackets for a Vehicle?
	 * @param cp1 First CargoPacket.
	 * @param cp2 Second CargoPacket.
	 * @return True if they are mergeable.
	 */
	static bool AreMergable(const CargoPacket *cp1, const CargoPacket *cp2)
	{
		return cp1->source_xy == cp2->source_xy &&
				cp1->periods_in_transit == cp2->periods_in_transit &&
				cp1->source_type == cp2->source_type &&
				cp1->first_station == cp2->first_station &&
				cp1->source_id == cp2->source_id;
	}
};

typedef MultiMap<StationID, CargoPacket *> StationCargoPacketMap;
typedef std::map<StationID, uint> StationCargoAmountMap;

/**
 * CargoList that is used for stations.
 */
class StationCargoList : public CargoList<StationCargoList, StationCargoPacketMap> {
protected:
	/** The (direct) parent of this class. */
	typedef CargoList<StationCargoList, StationCargoPacketMap> Parent;

	uint reserved_count; ///< Amount of cargo being reserved for loading.

public:
	/** The super class ought to know what it's doing. */
	friend class CargoList<StationCargoList, StationCargoPacketMap>;
	/* So we can use private/protected variables in the saveload code */
	friend class SlStationGoods;

	friend class CargoLoad;
	friend class CargoTransfer;
	template<class Tsource>
	friend class CargoRemoval;
	friend class CargoReservation;
	friend class CargoReturn;
	friend class StationCargoReroute;

	static void InvalidateAllFrom(SourceType src_type, SourceID src);

	template<class Taction>
	bool ShiftCargo(Taction &action, StationID next);

	template<class Taction>
	uint ShiftCargo(Taction action, StationIDStack next, bool include_invalid);

	void Append(CargoPacket *cp, StationID next);

	/**
	 * Check for cargo headed for a specific station.
	 * @param next Station the cargo is headed for.
	 * @return If there is any cargo for that station.
	 */
	inline bool HasCargoFor(StationIDStack next) const
	{
		while (!next.IsEmpty()) {
			if (this->packets.find(next.Pop()) != this->packets.end()) return true;
		}
		/* Packets for INVALID_STATION can go anywhere. */
		return this->packets.find(INVALID_STATION) != this->packets.end();
	}

	/**
	 * Returns first station of the first cargo packet in this list.
	 * @return The before mentioned station.
	 */
	inline StationID GetFirstStation() const
	{
		return this->count == 0 ? INVALID_STATION : this->packets.begin()->second.front()->first_station;
	}

	/**
	 * Returns sum of cargo still available for loading at the sation.
	 * (i.e. not counting cargo which is already reserved for loading)
	 * @return Cargo on board the vehicle.
	 */
	inline uint AvailableCount() const
	{
		return this->count;
	}

	/**
	 * Returns sum of cargo reserved for loading onto vehicles.
	 * @return Cargo reserved for loading.
	 */
	inline uint ReservedCount() const
	{
		return this->reserved_count;
	}

	/**
	 * Returns total count of cargo at the station, including
	 * cargo which is already reserved for loading.
	 * @return Total cargo count.
	 */
	inline uint TotalCount() const
	{
		return this->count + this->reserved_count;
	}

	/* Methods for moving cargo around. First parameter is always maximum
	 * amount of cargo to be moved. Second parameter is destination (if
	 * applicable), return value is amount of cargo actually moved. */

	uint Reserve(uint max_move, VehicleCargoList *dest, StationIDStack next, TileIndex current_tile);
	uint Load(uint max_move, VehicleCargoList *dest, StationIDStack next, TileIndex current_tile);
	uint Truncate(uint max_move = UINT_MAX, StationCargoAmountMap *cargo_per_source = nullptr);
	uint Reroute(uint max_move, StationCargoList *dest, StationID avoid, StationID avoid2, const GoodsEntry *ge);

	/**
	 * Are the two CargoPackets mergeable in the context of
	 * a list of CargoPackets for a Station?
	 * @param cp1 First CargoPacket.
	 * @param cp2 Second CargoPacket.
	 * @return True if they are mergeable.
	 */
	static bool AreMergable(const CargoPacket *cp1, const CargoPacket *cp2)
	{
		return cp1->source_xy == cp2->source_xy &&
				cp1->periods_in_transit == cp2->periods_in_transit &&
				cp1->source_type == cp2->source_type &&
				cp1->first_station == cp2->first_station &&
				cp1->source_id == cp2->source_id;
	}
};

#endif /* CARGOPACKET_H */
