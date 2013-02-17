/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargopacket.cpp Implementation of the cargo packets. */

#include "stdafx.h"
#include "core/pool_func.hpp"
#include "economy_base.h"
#include "cargoaction.h"
#include "order_type.h"

/* Initialize the cargopacket-pool */
CargoPacketPool _cargopacket_pool("CargoPacket");
INSTANTIATE_POOL_METHODS(CargoPacket)

/**
 * Create a new packet for savegame loading.
 */
CargoPacket::CargoPacket()
{
	this->source_type = ST_INDUSTRY;
	this->source_id   = INVALID_SOURCE;
}

/**
 * Creates a new cargo packet.
 * @param source      Source station of the packet.
 * @param source_xy   Source location of the packet.
 * @param count       Number of cargo entities to put in this packet.
 * @param source_type 'Type' of source the packet comes from (for subsidies).
 * @param source_id   Actual source of the packet (for subsidies).
 * @pre count != 0
 * @note We have to zero memory ourselves here because we are using a 'new'
 * that, in contrary to all other pools, does not memset to 0.
 */
CargoPacket::CargoPacket(StationID source, TileIndex source_xy, uint16 count, SourceType source_type, SourceID source_id) :
	feeder_share(0),
	count(count),
	days_in_transit(0),
	source_id(source_id),
	source(source),
	source_xy(source_xy),
	loaded_at_xy(0)
{
	assert(count != 0);
	this->source_type  = source_type;
}

/**
 * Creates a new cargo packet. Initializes the fields that cannot be changed later.
 * Used when loading or splitting packets.
 * @param count           Number of cargo entities to put in this packet.
 * @param days_in_transit Number of days the cargo has been in transit.
 * @param source          Station the cargo was initially loaded.
 * @param source_xy       Station location the cargo was initially loaded.
 * @param loaded_at_xy    Location the cargo was loaded last.
 * @param feeder_share    Feeder share the packet has already accumulated.
 * @param source_type     'Type' of source the packet comes from (for subsidies).
 * @param source_id       Actual source of the packet (for subsidies).
 * @note We have to zero memory ourselves here because we are using a 'new'
 * that, in contrary to all other pools, does not memset to 0.
 */
CargoPacket::CargoPacket(uint16 count, byte days_in_transit, StationID source, TileIndex source_xy, TileIndex loaded_at_xy, Money feeder_share, SourceType source_type, SourceID source_id) :
		feeder_share(feeder_share),
		count(count),
		days_in_transit(days_in_transit),
		source_id(source_id),
		source(source),
		source_xy(source_xy),
		loaded_at_xy(loaded_at_xy)
{
	assert(count != 0);
	this->source_type = source_type;
}

/**
 * Split this packet in two and return the split off part.
 * @param new_size Size of the split part.
 * @return Split off part, or NULL if no packet could be allocated!
 */
CargoPacket *CargoPacket::Split(uint new_size)
{
	if (!CargoPacket::CanAllocateItem()) return NULL;

	Money fs = this->FeederShare(new_size);
	CargoPacket *cp_new = new CargoPacket(new_size, this->days_in_transit, this->source, this->source_xy, this->loaded_at_xy, fs, this->source_type, this->source_id);
	this->feeder_share -= fs;
	this->count -= new_size;
	return cp_new;
}

/**
 * Merge another packet into this one.
 * @param cp Packet to be merged in.
 */
void CargoPacket::Merge(CargoPacket *cp)
{
	this->count += cp->count;
	this->feeder_share += cp->feeder_share;
	delete cp;
}

/**
 * Reduce the packet by the given amount and remove the feeder share.
 * @param count Amount to be removed.
 */
void CargoPacket::Reduce(uint count)
{
	assert(count < this->count);
	this->feeder_share -= this->FeederShare(count);
	this->count -= count;
}

/**
 * Invalidates (sets source_id to INVALID_SOURCE) all cargo packets from given source.
 * @param src_type Type of source.
 * @param src Index of source.
 */
/* static */ void CargoPacket::InvalidateAllFrom(SourceType src_type, SourceID src)
{
	CargoPacket *cp;
	FOR_ALL_CARGOPACKETS(cp) {
		if (cp->source_type == src_type && cp->source_id == src) cp->source_id = INVALID_SOURCE;
	}
}

/**
 * Invalidates (sets source to INVALID_STATION) all cargo packets from given station.
 * @param sid Station that gets removed.
 */
/* static */ void CargoPacket::InvalidateAllFrom(StationID sid)
{
	CargoPacket *cp;
	FOR_ALL_CARGOPACKETS(cp) {
		if (cp->source == sid) cp->source = INVALID_STATION;
	}
}

/*
 *
 * Cargo list implementation
 *
 */

/**
 * Destroy the cargolist ("frees" all cargo packets).
 */
template <class Tinst>
CargoList<Tinst>::~CargoList()
{
	for (Iterator it(this->packets.begin()); it != this->packets.end(); ++it) {
		delete *it;
	}
}

/**
 * Empty the cargo list, but don't free the cargo packets;
 * the cargo packets are cleaned by CargoPacket's CleanPool.
 */
template <class Tinst>
void CargoList<Tinst>::OnCleanPool()
{
	this->packets.clear();
}

/**
 * Update the cached values to reflect the removal of this packet or part of it.
 * Decreases count and days_in_transit.
 * @param cp Packet to be removed from cache.
 * @param count Amount of cargo from the given packet to be removed.
 */
template <class Tinst>
void CargoList<Tinst>::RemoveFromCache(const CargoPacket *cp, uint count)
{
	assert(count <= cp->count);
	this->count                 -= count;
	this->cargo_days_in_transit -= cp->days_in_transit * count;
}

/**
 * Update the cache to reflect adding of this packet.
 * Increases count and days_in_transit.
 * @param cp New packet to be inserted.
 */
template <class Tinst>
void CargoList<Tinst>::AddToCache(const CargoPacket *cp)
{
	this->count                 += cp->count;
	this->cargo_days_in_transit += cp->days_in_transit * cp->count;
}

/**
 * Truncates the cargo in this list to the given amount. It leaves the
 * first cargo entities and removes max_move from the back of the list.
 * @param max_move Maximum amount of entities to be removed from the list.
 * @return Amount of entities actually moved.
 */
template <class Tinst>
uint CargoList<Tinst>::Truncate(uint max_move)
{
	max_move = min(this->count, max_move);
	this->PopCargo(CargoRemoval<Tinst>(static_cast<Tinst *>(this), max_move));
	return max_move;
}

/**
 * Shifts cargo from the front of the packet list and applies some action to it.
 * @tparam Taction Action class or function to be used. It should define
 *                 "bool operator()(CargoPacket *)". If true is returned the
 *                 cargo packet will be removed from the list. Otherwise it
 *                 will be kept and the loop will be aborted.
 * @param action Action instance to be applied.
 */
template <class Tinst>
template <class Taction>
void CargoList<Tinst>::ShiftCargo(Taction action)
{
	Iterator it(this->packets.begin());
	while (it != this->packets.end() && action.MaxMove() > 0) {
		CargoPacket *cp = *it;
		if (action(cp)) {
			it = this->packets.erase(it);
		} else {
			break;
		}
	}
}

/**
 * Pops cargo from the back of the packet list and applies some action to it.
 * @tparam Taction Action class or function to be used. It should define
 *                 "bool operator()(CargoPacket *)". If true is returned the
 *                 cargo packet will be removed from the list. Otherwise it
 *                 will be kept and the loop will be aborted.
 * @param action Action instance to be applied.
 */
template <class Tinst>
template <class Taction>
void CargoList<Tinst>::PopCargo(Taction action)
{
	if (this->packets.empty()) return;
	Iterator it(--(this->packets.end()));
	Iterator begin(this->packets.begin());
	while (action.MaxMove() > 0) {
		CargoPacket *cp = *it;
		if (action(cp)) {
			if (it != begin) {
				this->packets.erase(it--);
			} else {
				this->packets.erase(it);
				break;
			}
		} else {
			break;
		}
	}
}

/** Invalidates the cached data and rebuilds it. */
template <class Tinst>
void CargoList<Tinst>::InvalidateCache()
{
	this->count = 0;
	this->cargo_days_in_transit = 0;

	for (ConstIterator it(this->packets.begin()); it != this->packets.end(); it++) {
		static_cast<Tinst *>(this)->AddToCache(*it);
	}
}

/**
 * Tries to merge the second packet into the first and return if that was
 * successful.
 * @param icp Packet to be merged into.
 * @param cp Packet to be eliminated.
 * @return If the packets could be merged.
 */
template <class Tinst>
/* static */ bool CargoList<Tinst>::TryMerge(CargoPacket *icp, CargoPacket *cp)
{
	if (Tinst::AreMergable(icp, cp) &&
				icp->count + cp->count <= CargoPacket::MAX_COUNT) {
		icp->Merge(cp);
		return true;
	} else {
		return false;
	}
}

/*
 *
 * Vehicle cargo list implementation.
 *
 */

/**
 * Appends the given cargo packet. Tries to merge it with another one in the
 * packets list. If no fitting packet is found, appends it. You can only append
 * packets to the ranges of packets designated for keeping or loading.
 * Furthermore if there are already packets reserved for loading you cannot
 * directly add packets to the "keep" list. You first have to load the reserved
 * ones.
 * @warning After appending this packet may not exist anymore!
 * @note Do not use the cargo packet anymore after it has been appended to this CargoList!
 * @param cp Cargo packet to add.
 * @param action Either MTA_KEEP if you want to add the packet directly or MTA_LOAD
 * if you want to reserve it first.
 * @pre cp != NULL
 * @pre action == MTA_LOAD || (action == MTA_KEEP && this->designation_counts[MTA_LOAD] == 0)
 */
void VehicleCargoList::Append(CargoPacket *cp, MoveToAction action)
{
	assert(cp != NULL);
	assert(action == MTA_LOAD ||
			(action == MTA_KEEP && this->action_counts[MTA_LOAD] == 0));
	this->AddToMeta(cp, action);

	if (this->count == cp->count) {
		this->packets.push_back(cp);
		return;
	}

	uint sum = cp->count;
	for (ReverseIterator it(this->packets.rbegin()); it != this->packets.rend(); it++) {
		CargoPacket *icp = *it;
		if (VehicleCargoList::TryMerge(icp, cp)) return;
		sum += icp->count;
		if (sum >= this->action_counts[action]) {
			this->packets.push_back(cp);
			return;
		}
	}

	NOT_REACHED();
}

/**
 * Update the cached values to reflect the removal of this packet or part of it.
 * Decreases count, feeder share and days_in_transit.
 * @param cp Packet to be removed from cache.
 * @param count Amount of cargo from the given packet to be removed.
 */
void VehicleCargoList::RemoveFromCache(const CargoPacket *cp, uint count)
{
	this->feeder_share -= cp->FeederShare(count);
	this->Parent::RemoveFromCache(cp, count);
}

/**
 * Update the cache to reflect adding of this packet.
 * Increases count, feeder share and days_in_transit.
 * @param cp New packet to be inserted.
 */
void VehicleCargoList::AddToCache(const CargoPacket *cp)
{
	this->feeder_share += cp->feeder_share;
	this->Parent::AddToCache(cp);
}

/**
 * Removes a packet or part of it from the metadata.
 * @param cp Packet to be removed.
 * @param action MoveToAction of the packet (for updating the counts).
 * @param count Amount of cargo to be removed.
 */
void VehicleCargoList::RemoveFromMeta(const CargoPacket *cp, MoveToAction action, uint count)
{
	this->AssertCountConsistency();
	this->RemoveFromCache(cp, count);
	this->action_counts[action] -= count;
	this->AssertCountConsistency();
}

/**
 * Adds a packet to the metadata.
 * @param cp Packet to be added.
 * @param action MoveToAction of the packet.
 */
void VehicleCargoList::AddToMeta(const CargoPacket *cp, MoveToAction action)
{
	this->AssertCountConsistency();
	this->AddToCache(cp);
	this->action_counts[action] += cp->count;
	this->AssertCountConsistency();
}

/**
 * Ages the all cargo in this list.
 */
void VehicleCargoList::AgeCargo()
{
	for (ConstIterator it(this->packets.begin()); it != this->packets.end(); it++) {
		CargoPacket *cp = *it;
		/* If we're at the maximum, then we can't increase no more. */
		if (cp->days_in_transit == 0xFF) continue;

		cp->days_in_transit++;
		this->cargo_days_in_transit += cp->count;
	}
}

/**
 * Stages cargo for unloading. The cargo is sorted so that packets to be
 * transferred, delivered or kept are in consecutive chunks in the list. At the
 * same time the designation_counts are updated to reflect the size of those
 * chunks.
 * @param accepted If the cargo will be accepted at the station.
 * @param current_station ID of the station.
 * @param order_flags OrderUnloadFlags that will apply to the unload operation.
 * return If any cargo will be unloaded.
 */
bool VehicleCargoList::Stage(bool accepted, StationID current_station, uint8 order_flags)
{
	this->AssertCountConsistency();
	assert(this->action_counts[MTA_LOAD] == 0);
	this->action_counts[MTA_TRANSFER] = this->action_counts[MTA_DELIVER] = this->action_counts[MTA_KEEP] = 0;
	Iterator deliver = this->packets.end();
	Iterator it = this->packets.begin();
	uint sum = 0;
	while (sum < this->count) {
		CargoPacket *cp = *it;
		this->packets.erase(it++);
		if ((order_flags & OUFB_TRANSFER) != 0 || (!accepted && (order_flags & OUFB_UNLOAD) != 0)) {
			this->packets.push_front(cp);
			this->action_counts[MTA_TRANSFER] += cp->count;
		} else if (accepted && current_station != cp->source && (order_flags & OUFB_NO_UNLOAD) == 0) {
			this->packets.insert(deliver, cp);
			this->action_counts[MTA_DELIVER] += cp->count;
		} else {
			this->packets.push_back(cp);
			if (deliver == this->packets.end()) --deliver;
			this->action_counts[MTA_KEEP] += cp->count;
		}
		sum += cp->count;
	}
	this->AssertCountConsistency();
	return this->action_counts[MTA_DELIVER] > 0 || this->action_counts[MTA_TRANSFER] > 0;
}

/** Invalidates the cached data and rebuild it. */
void VehicleCargoList::InvalidateCache()
{
	this->feeder_share = 0;
	this->Parent::InvalidateCache();
}

/**
 * Moves some cargo from one designation to another. You can only move
 * between adjacent designations. E.g. you can keep cargo that was
 * previously reserved (MTA_LOAD) or you can mark cargo to be transferred
 * that was previously marked as to be delivered, but you can't reserve
 * cargo that's marked as to be delivered.
 */
uint VehicleCargoList::Reassign(uint max_move, MoveToAction from, MoveToAction to)
{
	max_move = min(this->action_counts[from], max_move);
	assert(Delta((int)from, (int)to) == 1);
	this->action_counts[from] -= max_move;
	this->action_counts[to] += max_move;
	return max_move;
}

/**
 * Returns reserved cargo to the station and removes it from the cache.
 * @param dest Station the cargo is returned to.
 * @param max_move Maximum amount of cargo to move.
 * @return Amount of cargo actually returned.
 */
uint VehicleCargoList::Return(uint max_move, StationCargoList *dest)
{
	max_move = min(this->action_counts[MTA_LOAD], max_move);
	this->PopCargo(CargoReturn(this, dest, max_move));
	return max_move;
}

/**
 * Shifts cargo between two vehicles.
 * @param dest Other vehicle's cargo list.
 * @param max_move Maximum amount of cargo to be moved.
 * @return Amount of cargo actually moved.
 */
uint VehicleCargoList::Shift(uint max_move, VehicleCargoList *dest)
{
	max_move = min(this->count, max_move);
	this->PopCargo(CargoShift(this, dest, max_move));
	return max_move;
}

/**
 * Unloads cargo at the given station. Deliver or transfer, depending on the
 * ranges defined by designation_counts.
 * @param dest StationCargoList to add transferred cargo to.
 * @param max_move Maximum amount of cargo to move.
 * @param payment Payment object to register payments in.
 * @return Amount of cargo actually unloaded.
 */
uint VehicleCargoList::Unload(uint max_move, StationCargoList *dest, CargoPayment *payment)
{
	uint moved = 0;
	if (this->action_counts[MTA_TRANSFER] > 0) {
		uint move = min(this->action_counts[MTA_TRANSFER], max_move);
		this->ShiftCargo(CargoTransfer(this, dest, move, payment));
		moved += move;
	}
	if (this->action_counts[MTA_TRANSFER] == 0 && this->action_counts[MTA_DELIVER] > 0 && moved < max_move) {
		uint move = min(this->action_counts[MTA_DELIVER], max_move - moved);
		this->ShiftCargo(CargoDelivery(this, move, payment));
		moved += move;
	}
	return moved;
}

/*
 *
 * Station cargo list implementation.
 *
 */

/**
 * Appends the given cargo packet. Tries to merge it with another one in the
 * packets list. If no fitting packet is found, appends it.
 * @warning After appending this packet may not exist anymore!
 * @note Do not use the cargo packet anymore after it has been appended to this CargoList!
 * @param cp Cargo packet to add.
 * @pre cp != NULL
 */
void StationCargoList::Append(CargoPacket *cp)
{
	assert(cp != NULL);
	this->AddToCache(cp);

	for (List::reverse_iterator it(this->packets.rbegin()); it != this->packets.rend(); it++) {
		if (StationCargoList::TryMerge(*it, cp)) return;
	}

	/* The packet could not be merged with another one */
	this->packets.push_back(cp);
}

/**
 * Reserves cargo for loading onto the vehicle.
 * @param dest VehicleCargoList to reserve for.
 * @param max_move Maximum amount of cargo to reserve.
 * @param load_place Tile index of the current station.
 * @return Amount of cargo actually reserved.
 */
uint StationCargoList::Reserve(uint max_move, VehicleCargoList *dest, TileIndex load_place)
{
	max_move = min(this->count, max_move);
	this->ShiftCargo(CargoReservation(this, dest, max_move, load_place));
	return max_move;
}

/**
 * Loads cargo onto a vehicle. If the vehicle has reserved cargo load that.
 * Otherwise load cargo from the station.
 * @param dest Vehicle cargo list where the cargo resides.
 * @param max_move Amount of cargo to load.
 * @return Amount of cargo actually loaded.
 */
uint StationCargoList::Load(uint max_move, VehicleCargoList *dest, TileIndex load_place)
{
	uint move = min(dest->ActionCount(VehicleCargoList::MTA_LOAD), max_move);
	if (move > 0) {
		this->reserved_count -= move;
		dest->Reassign(move, VehicleCargoList::MTA_LOAD, VehicleCargoList::MTA_KEEP);
	} else {
		move = min(this->count, max_move);
		this->ShiftCargo(CargoLoad(this, dest, move, load_place));
	}
	return move;
}

/*
 * We have to instantiate everything we want to be usable.
 */
template class CargoList<VehicleCargoList>;
template class CargoList<StationCargoList>;
