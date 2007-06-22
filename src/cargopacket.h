/* $Id$ */

/** @file cargotype.h */

#ifndef CARGOPACKET_H
#define CARGOPACKET_H

#include <list>

/**
 * Container for cargo from the same location and time
 */
struct CargoPacket {
	bool touched;

	typedef uint32 ID;      ///< Type for cargopacket identifiers

	ID index;               ///< The unique index of this packet

	StationID source;       ///< The station where the cargo came from first
	TileIndex source_xy;    ///< The origin of the cargo (first station in feeder chain)
	TileIndex loaded_at_xy; ///< Location where this cargo has been loaded into the vehicle

	uint16 count;           ///< The amount of cargo in this packet
	byte days_in_transit;   ///< Amount of days this packet has been in transit
	Money feeder_share;     ///< Value of feeder pickup to be paid for on delivery of cargo
	bool paid_for;          ///< Have we been paid for this cargo packet?

	/**
	 * Creates a new cargo packet
	 * @param source the source of the packet
	 * @param count  the number of cargo entities to put in this packet
	 * @pre count != 0 || source == INVALID_STATION
	 */
	CargoPacket(StationID source = INVALID_STATION, uint16 count = 0);

	/** Destroy the packet */
	~CargoPacket();


	/**
	 * Is this a valid cargo packet ?
	 * @return true if and only it is valid
	 */
	bool IsValid() const { return this->count != 0; }

	/**
	 * Checks whether the cargo packet is from (exactly) the same source
	 * in time and location.
	 * @param cp the cargo packet to compare to
	 * @return true if and only if days_in_transit and source_xy are equal
	 */
	bool SameSource(CargoPacket *cp);


	/* normal new/delete operators. Used when building/removing station */
	void* operator new (size_t size);
	void operator delete(void *p);

	/* new/delete operators accepting station index. Used when loading station from savegame. */
	void* operator new (size_t size, CargoPacket::ID cp_idx);
	void operator delete(void *p, CargoPacket::ID cp_idx);

private:
	/**
	 * Allocate the raw memory for this cargo packet
	 * @return the allocated memory
	 */
	static CargoPacket *AllocateRaw();
};

/** We want to use a pool */
DECLARE_OLD_POOL(CargoPacket, CargoPacket, 10, 1000)

/**
 * Iterate over all _valid_ cargo packets from the given start
 * @param cp    the variable used as "iterator"
 * @param start the cargo packet ID of the first packet to iterate over
 */
#define FOR_ALL_CARGOPACKETS_FROM(cp, start) for (cp = GetCargoPacket(start); cp != NULL; cp = (cp->index + 1U < GetCargoPacketPoolSize()) ? GetCargoPacket(cp->index + 1U) : NULL) if (cp->IsValid())

/**
 * Iterate over all _valid_ cargo packets from the begin of the pool
 * @param cp    the variable used as "iterator"
 */
#define FOR_ALL_CARGOPACKETS(cp) FOR_ALL_CARGOPACKETS_FROM(cp, 0)

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
	/**
	 * Needed for an ugly hack:
	 *  - vehicles and stations need to store cargo lists, so they use CargoList as container
	 *  - this internals of the container should be protected, e.g. private (or protected) by C++
	 *  - for saving/loading we need to pass pointer to objects
	 *  -> so *if* the pointer to the cargo list is the same as the pointer to the packet list
	 *     encapsulated in the CargoList, we can just pass the CargoList as "offset".
	 *     Normally we would then just add the offset of the packets variable within the cargo list
	 *     but that is not possible because the variable is private. Furthermore we are not sure
	 *     that this works on all platforms, we need to check whether the offset is actually 0.
	 *     This cannot be done compile time, because the variable is private. So we need to write
	 *     a function that does actually check the offset runtime and call it somewhere where it
	 *     is always called but it should not be called often.
	 */
	static void AssertOnWrongPacketOffset();

	/** Create the cargo list */
	CargoList() { this->InvalidateCache(); }
	/** And destroy it ("frees" all cargo packets) */
	~CargoList();

	/**
	 * Returns a pointer to the cargo packet list (so you can iterate over it etc).
	 * @return pointer to the packet list
	 */
	const CargoList::List *Packets() const;

	/**
	 * Ages the all cargo in this list
	 */
	void AgeCargo();

	/**
	 * Checks whether this list is empty
	 * @return true if and only if the list is empty
	 */
	bool Empty() const;

	/**
	 * Returns the number of cargo entities in this list
	 * @return the before mentioned number
	 */
	uint Count() const;

	/**
	 * Is there some cargo that has not been paid for?
	 * @return true if and only if there is such a cargo
	 */
	bool UnpaidCargo() const;

	/**
	 * Returns total sum of the feeder share for all packets
	 * @return the before mentioned number
	 */
	Money FeederShare() const;

	/**
	 * Returns source of the first cargo packet in this list
	 * @return the before mentioned source
	 */
	StationID Source() const;

	/**
	 * Returns average number of days in transit for a cargo entity
	 * @return the before mentioned number
	 */
	uint DaysInTransit() const;


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
