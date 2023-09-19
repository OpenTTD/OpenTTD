/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargoaction.cpp Implementation of cargo actions. */

#include "stdafx.h"
#include "economy_base.h"
#include "cargoaction.h"
#include "station_base.h"

#include "safeguards.h"

/**
 * Decides if a packet needs to be split.
 * @param cp Packet to be either split or moved in one piece.
 * @return Either new packet if splitting was necessary or the given one
 *         otherwise.
 */
template<class Tsource, class Tdest>
CargoPacket *CargoMovement<Tsource, Tdest>::Preprocess(CargoPacket *cp)
{
	if (this->max_move < cp->Count()) {
		cp = cp->Split(this->max_move);
		this->max_move = 0;
	} else {
		this->max_move -= cp->Count();
	}
	return cp;
}

/**
 * Determines the amount of cargo to be removed from a packet and removes that
 * from the metadata of the list.
 * @param cp Packet to be removed completely or partially.
 * @return Amount of cargo to be removed.
 */
template<class Tsource>
uint CargoRemoval<Tsource>::Preprocess(CargoPacket *cp)
{
	if (this->max_move >= cp->Count()) {
		this->max_move -= cp->Count();
		return cp->Count();
	} else {
		uint ret = this->max_move;
		this->max_move = 0;
		return ret;
	}
}

/**
 * Finalize cargo removal. Either delete the packet or reduce it.
 * @param cp Packet to be removed or reduced.
 * @param remove Amount of cargo to be removed.
 * @return True if the packet was deleted, False if it was reduced.
 */
template<class Tsource>
bool CargoRemoval<Tsource>::Postprocess(CargoPacket *cp, uint remove)
{
	if (remove == cp->Count()) {
		delete cp;
		return true;
	} else {
		cp->Reduce(remove);
		return false;
	}
}

/**
 * Removes some cargo from a StationCargoList.
 * @param cp Packet to be removed.
 * @return True if the packet was completely delivered, false if only part of
 *         it was.
 */
template<>
bool CargoRemoval<StationCargoList>::operator()(CargoPacket *cp)
{
	uint remove = this->Preprocess(cp);
	this->source->RemoveFromCache(cp, remove);
	return this->Postprocess(cp, remove);
}

/**
 * Removes some cargo from a VehicleCargoList.
 * @param cp Packet to be removed.
 * @return True if the packet was completely delivered, false if only part of
 *         it was.
 */
template<>
bool CargoRemoval<VehicleCargoList>::operator()(CargoPacket *cp)
{
	uint remove = this->Preprocess(cp);
	this->source->RemoveFromMeta(cp, VehicleCargoList::MTA_KEEP, remove);
	return this->Postprocess(cp, remove);
}

/**
 * Delivers some cargo.
 * @param cp Packet to be delivered.
 * @return True if the packet was completely delivered, false if only part of
 *         it was.
 */
bool CargoDelivery::operator()(CargoPacket *cp)
{
	uint remove = this->Preprocess(cp);
	this->source->RemoveFromMeta(cp, VehicleCargoList::MTA_DELIVER, remove);
	this->payment->PayFinalDelivery(cp, remove, this->current_tile);
	return this->Postprocess(cp, remove);
}

/**
 * Loads some cargo onto a vehicle.
 * @param cp Packet to be loaded.
 * @return True if the packet was completely loaded, false if part of it was.
 */
bool CargoLoad::operator()(CargoPacket *cp)
{
	CargoPacket *cp_new = this->Preprocess(cp);
	if (cp_new == nullptr) return false;
	cp_new->UpdateLoadingTile(this->current_tile);
	this->source->RemoveFromCache(cp_new, cp_new->Count());
	this->destination->Append(cp_new, VehicleCargoList::MTA_KEEP);
	return cp_new == cp;
}

/**
 * Reserves some cargo for loading.
 * @param cp Packet to be reserved.
 * @return True if the packet was completely reserved, false if part of it was.
 */
bool CargoReservation::operator()(CargoPacket *cp)
{
	CargoPacket *cp_new = this->Preprocess(cp);
	if (cp_new == nullptr) return false;
	cp_new->UpdateLoadingTile(this->current_tile);
	this->source->reserved_count += cp_new->Count();
	this->source->RemoveFromCache(cp_new, cp_new->Count());
	this->destination->Append(cp_new, VehicleCargoList::MTA_LOAD);
	return cp_new == cp;
}

/**
 * Returns some reserved cargo.
 * @param cp Packet to be returned.
 * @return True if the packet was completely returned, false if part of it was.
 */
bool CargoReturn::operator()(CargoPacket *cp)
{
	CargoPacket *cp_new = this->Preprocess(cp);
	if (cp_new == nullptr) cp_new = cp;
	assert(cp_new->Count() <= this->destination->reserved_count);
	cp_new->UpdateUnloadingTile(this->current_tile);
	this->source->RemoveFromMeta(cp_new, VehicleCargoList::MTA_LOAD, cp_new->Count());
	this->destination->reserved_count -= cp_new->Count();
	this->destination->Append(cp_new, this->next);
	return cp_new == cp;
}

/**
 * Transfers some cargo from a vehicle to a station.
 * @param cp Packet to be transferred.
 * @return True if the packet was completely reserved, false if part of it was.
 */
bool CargoTransfer::operator()(CargoPacket *cp)
{
	CargoPacket *cp_new = this->Preprocess(cp);
	if (cp_new == nullptr) return false;
	cp_new->UpdateUnloadingTile(this->current_tile);
	this->source->RemoveFromMeta(cp_new, VehicleCargoList::MTA_TRANSFER, cp_new->Count());
	/* No transfer credits here as they were already granted during Stage(). */
	this->destination->Append(cp_new, cp_new->GetNextHop());
	return cp_new == cp;
}

/**
 * Shifts some cargo from a vehicle to another one.
 * @param cp Packet to be shifted.
 * @return True if the packet was completely shifted, false if part of it was.
 */
bool CargoShift::operator()(CargoPacket *cp)
{
	CargoPacket *cp_new = this->Preprocess(cp);
	if (cp_new == nullptr) cp_new = cp;
	this->source->RemoveFromMeta(cp_new, VehicleCargoList::MTA_KEEP, cp_new->Count());
	this->destination->Append(cp_new, VehicleCargoList::MTA_KEEP);
	return cp_new == cp;
}

/**
 * Reroutes some cargo from one Station sublist to another.
 * @param cp Packet to be rerouted.
 * @return True if the packet was completely rerouted, false if part of it was.
 */
bool StationCargoReroute::operator()(CargoPacket *cp)
{
	CargoPacket *cp_new = this->Preprocess(cp);
	if (cp_new == nullptr) cp_new = cp;
	StationID next = this->ge->GetVia(cp_new->GetFirstStation(), this->avoid, this->avoid2);
	assert(next != this->avoid && next != this->avoid2);
	if (this->source != this->destination) {
		this->source->RemoveFromCache(cp_new, cp_new->Count());
		this->destination->AddToCache(cp_new);
	}

	/* Legal, as insert doesn't invalidate iterators in the MultiMap, however
	 * this might insert the packet between range.first and range.second (which might be end())
	 * This is why we check for GetKey above to avoid infinite loops. */
	this->destination->packets.Insert(next, cp_new);
	return cp_new == cp;
}

/**
 * Reroutes some cargo in a VehicleCargoList.
 * @param cp Packet to be rerouted.
 * @return True if the packet was completely rerouted, false if part of it was.
 */
bool VehicleCargoReroute::operator()(CargoPacket *cp)
{
	CargoPacket *cp_new = this->Preprocess(cp);
	if (cp_new == nullptr) cp_new = cp;
	if (cp_new->GetNextHop() == this->avoid || cp_new->GetNextHop() == this->avoid2) {
		cp->SetNextHop(this->ge->GetVia(cp_new->GetFirstStation(), this->avoid, this->avoid2));
	}
	if (this->source != this->destination) {
		this->source->RemoveFromMeta(cp_new, VehicleCargoList::MTA_TRANSFER, cp_new->Count());
		this->destination->AddToMeta(cp_new, VehicleCargoList::MTA_TRANSFER);
	}

	/* Legal, as front pushing doesn't invalidate iterators in std::list. */
	this->destination->packets.push_front(cp_new);
	return cp_new == cp;
}

template uint CargoRemoval<VehicleCargoList>::Preprocess(CargoPacket *cp);
template uint CargoRemoval<StationCargoList>::Preprocess(CargoPacket *cp);
template bool CargoRemoval<VehicleCargoList>::Postprocess(CargoPacket *cp, uint remove);
template bool CargoRemoval<StationCargoList>::Postprocess(CargoPacket *cp, uint remove);
