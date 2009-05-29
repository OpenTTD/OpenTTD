/* $Id$ */

/** @file cargopacket.h Base class for cargo packets. */

#ifndef CARGOPACKET_H
#define CARGOPACKET_H

#include "core/pool_type.hpp"
#include "economy_type.h"
#include "tile_type.h"
#include "station_type.h"
#include <list>

typedef uint32 CargoPacketID;
struct CargoPacket;

/** We want to use a pool */
typedef Pool<CargoPacket, CargoPacketID, 1024, 1048576> CargoPacketPool;
extern CargoPacketPool _cargopacket_pool;

/**
 * Container for cargo from the same location and time
 */
struct CargoPacket : CargoPacketPool::PoolItem<&_cargopacket_pool> {
	Money feeder_share;     ///< Value of feeder pickup to be paid for on delivery of cargo
	TileIndex source_xy;    ///< The origin of the cargo (first station in feeder chain)
	TileIndex loaded_at_xy; ///< Location where this cargo has been loaded into the vehicle
	StationID source;       ///< The station where the cargo came from first

	uint16 count;           ///< The amount of cargo in this packet
	byte days_in_transit;   ///< Amount of days this packet has been in transit
	bool paid_for;          ///< Have we been paid for this cargo packet?

	/**
	 * Creates a new cargo packet
	 * @param source the source of the packet
	 * @param count  the number of cargo entities to put in this packet
	 * @pre count != 0 || source == INVALID_STATION
	 */
	CargoPacket(StationID source = INVALID_STATION, uint16 count = 0);

	/** Destroy the packet */
	~CargoPacket() { }

	/**
	 * Checks whether the cargo packet is from (exactly) the same source
	 * in time and location.
	 * @param cp the cargo packet to compare to
	 * @return true if and only if days_in_transit and source_xy are equal
	 */
	FORCEINLINE bool SameSource(const CargoPacket *cp) const
	{
		return this->source_xy == cp->source_xy && this->days_in_transit == cp->days_in_transit && this->paid_for == cp->paid_for;
	}
};

/**
 * Iterate over all _valid_ cargo packets from the given start
 * @param var   the variable used as "iterator"
 * @param start the cargo packet ID of the first packet to iterate over
 */
#define FOR_ALL_CARGOPACKETS_FROM(var, start) FOR_ALL_ITEMS_FROM(CargoPacket, cargopacket_index, var, start)

/**
 * Iterate over all _valid_ cargo packets from the begin of the pool
 * @param var   the variable used as "iterator"
 */
#define FOR_ALL_CARGOPACKETS(var) FOR_ALL_CARGOPACKETS_FROM(var, 0)

extern const struct SaveLoad *GetGoodsDesc();

/**
 * Simple collection class for a list of cargo packets
 */
class CargoList {
public:
	/** List of cargo packets */
	typedef std::list<CargoPacket *> List;

	/** Kind of actions that could be done with packets on move */
	enum MoveToAction {
		MTA_FINAL_DELIVERY, ///< "Deliver" the packet to the final destination, i.e. destroy the packet
		MTA_CARGO_LOAD,     ///< Load the packet onto a vehicle, i.e. set the last loaded station ID
		MTA_OTHER           ///< "Just" move the packet to another cargo list
	};

private:
	List packets;         ///< The cargo packets in this list

	bool empty;           ///< Cache for whether this list is empty or not
	uint count;           ///< Cache for the number of cargo entities
	bool unpaid_cargo;    ///< Cache for the unpaid cargo
	Money feeder_share;   ///< Cache for the feeder share
	StationID source;     ///< Cache for the source of the packet
	uint days_in_transit; ///< Cache for the number of days in transit

public:
	friend const struct SaveLoad *GetGoodsDesc();

	/** Create the cargo list */
	FORCEINLINE CargoList() { this->InvalidateCache(); }
	/** And destroy it ("frees" all cargo packets) */
	~CargoList();

	/**
	 * Returns a pointer to the cargo packet list (so you can iterate over it etc).
	 * @return pointer to the packet list
	 */
	FORCEINLINE const CargoList::List *Packets() const { return &this->packets; }

	/**
	 * Ages the all cargo in this list
	 */
	void AgeCargo();

	/**
	 * Checks whether this list is empty
	 * @return true if and only if the list is empty
	 */
	FORCEINLINE bool Empty() const { return this->empty; }

	/**
	 * Returns the number of cargo entities in this list
	 * @return the before mentioned number
	 */
	FORCEINLINE uint Count() const { return this->count; }

	/**
	 * Is there some cargo that has not been paid for?
	 * @return true if and only if there is such a cargo
	 */
	FORCEINLINE bool UnpaidCargo() const { return this->unpaid_cargo; }

	/**
	 * Returns total sum of the feeder share for all packets
	 * @return the before mentioned number
	 */
	FORCEINLINE Money FeederShare() const { return this->feeder_share; }

	/**
	 * Returns source of the first cargo packet in this list
	 * @return the before mentioned source
	 */
	FORCEINLINE StationID Source() const { return this->source; }

	/**
	 * Returns average number of days in transit for a cargo entity
	 * @return the before mentioned number
	 */
	FORCEINLINE uint DaysInTransit() const { return this->days_in_transit; }


	/**
	 * Appends the given cargo packet
	 * @warning After appending this packet may not exist anymore!
	 * @note Do not use the cargo packet anymore after it has been appended to this CargoList!
	 * @param cp the cargo packet to add
	 * @pre cp != NULL
	 */
	void Append(CargoPacket *cp);

	/**
	 * Truncates the cargo in this list to the given amount. It leaves the
	 * first count cargo entities and removes the rest.
	 * @param count the maximum amount of entities to be in the list after the command
	 */
	void Truncate(uint count);

	/**
	 * Moves the given amount of cargo to another list.
	 * Depending on the value of mta the side effects of this function differ:
	 *  - MTA_FINAL_DELIVERY: destroys the packets that do not originate from a specific station
	 *  - MTA_CARGO_LOAD:     sets the loaded_at_xy value of the moved packets
	 *  - MTA_OTHER:          just move without side effects
	 * @param dest  the destination to move the cargo to
	 * @param count the amount of cargo entities to move
	 * @param mta   how to handle the moving (side effects)
	 * @param data  Depending on mta the data of this variable differs:
	 *              - MTA_FINAL_DELIVERY - station ID of packet's origin not to remove
	 *              - MTA_CARGO_LOAD     - station's tile index of load
	 *              - MTA_OTHER          - unused
	 * @param mta == MTA_FINAL_DELIVERY || dest != NULL
	 * @return true if there are still packets that might be moved from this cargo list
	 */
	bool MoveTo(CargoList *dest, uint count, CargoList::MoveToAction mta = MTA_OTHER, uint data = 0);

	/** Invalidates the cached data and rebuild it */
	void InvalidateCache();
};

#endif /* CARGOPACKET_H */
