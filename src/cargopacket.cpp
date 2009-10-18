/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargopacket.cpp Implementation of the cargo packets */

#include "stdafx.h"
#include "core/pool_func.hpp"
#include "economy_base.h"
#include "station_base.h"

/* Initialize the cargopacket-pool */
CargoPacketPool _cargopacket_pool("CargoPacket");
INSTANTIATE_POOL_METHODS(CargoPacket)

/**
 * Initialize, i.e. clean, the pool with cargo packets.
 */
void InitializeCargoPackets()
{
	_cargopacket_pool.CleanPool();
}

CargoPacket::CargoPacket(StationID source, uint16 count, SourceType source_type, SourceID source_id) :
	count(count),
	source_id(source_id),
	source(source)
{
	this->source_type = source_type;

	if (source != INVALID_STATION) {
		assert(count != 0);
		this->source_xy    = Station::Get(source)->xy;
		this->loaded_at_xy = this->source_xy;
	}
}

CargoPacket::CargoPacket(uint16 count, byte days_in_transit, Money feeder_share, SourceType source_type, SourceID source_id) :
		feeder_share(feeder_share),
		count(count),
		days_in_transit(days_in_transit),
		source_id(source_id)
{
	this->source_type = source_type;
}

/**
 * Invalidates (sets source_id to INVALID_SOURCE) all cargo packets from given source
 * @param src_type type of source
 * @param src index of source
 */
/* static */ void CargoPacket::InvalidateAllFrom(SourceType src_type, SourceID src)
{
	CargoPacket *cp;
	FOR_ALL_CARGOPACKETS(cp) {
		if (cp->source_type == src_type && cp->source_id == src) cp->source_id = INVALID_SOURCE;
	}
}

/*
 *
 * Cargo list implementation
 *
 */

CargoList::~CargoList()
{
	while (!this->packets.empty()) {
		delete this->packets.front();
		this->packets.pop_front();
	}
}

void CargoList::RemoveFromCache(const CargoPacket *cp)
{
	this->count                 -= cp->count;
	this->feeder_share          -= cp->feeder_share;
	this->cargo_days_in_transit -= cp->days_in_transit * cp->count;
}

void CargoList::AddToCache(const CargoPacket *cp)
{
	this->count                 += cp->count;
	this->feeder_share          += cp->feeder_share;
	this->cargo_days_in_transit += cp->days_in_transit * cp->count;
}

void VehicleCargoList::AgeCargo()
{
	for (List::const_iterator it = this->packets.begin(); it != this->packets.end(); it++) {
		/* If we're at the maximum, then we can't increase no more. */
		if ((*it)->days_in_transit == 0xFF) continue;

		(*it)->days_in_transit++;
		this->cargo_days_in_transit += (*it)->count;
	}
}

void CargoList::Append(CargoPacket *cp)
{
	assert(cp != NULL);

	for (List::iterator it = this->packets.begin(); it != this->packets.end(); it++) {
		CargoPacket *icp = *it;
		if (icp->SameSource(cp) && icp->count + cp->count <= CargoPacket::MAX_COUNT) {
			icp->count        += cp->count;
			icp->feeder_share += cp->feeder_share;

			this->AddToCache(cp);
			delete cp;
			return;
		}
	}

	/* The packet could not be merged with another one */
	this->packets.push_back(cp);
	this->AddToCache(cp);
}


void CargoList::Truncate(uint max_remaining)
{
	for (List::iterator it = packets.begin(); it != packets.end(); /* done during loop*/) {
		CargoPacket *cp = *it;
		if (max_remaining == 0) {
			/* Nothing should remain, just remove the packets. */
			packets.erase(it++);
			this->RemoveFromCache(cp);
			delete cp;
			continue;
		}

		uint local_count = cp->count;
		if (local_count > max_remaining) {
			uint diff = local_count - max_remaining;
			this->count -= diff;
			this->cargo_days_in_transit -= cp->days_in_transit * diff;
			cp->count = max_remaining;
			max_remaining = 0;
		} else {
			max_remaining -= local_count;
		}
		++it;
	}
}

bool CargoList::MoveTo(CargoList *dest, uint max_move, CargoList::MoveToAction mta, CargoPayment *payment, uint data)
{
	assert(mta == MTA_FINAL_DELIVERY || dest != NULL);
	assert(mta == MTA_UNLOAD || mta == MTA_CARGO_LOAD || payment != NULL);

	List::iterator it = packets.begin();
	while (it != packets.end() && max_move > 0) {
		CargoPacket *cp = *it;
		if (cp->source == data && mta == MTA_FINAL_DELIVERY) {
			/* Skip cargo that originated from this station. */
			++it;
			continue;
		}

		if (cp->count <= max_move) {
			/* Can move the complete packet */
			max_move -= cp->count;
			this->packets.erase(it++);
			this->RemoveFromCache(cp);
			switch(mta) {
				case MTA_FINAL_DELIVERY:
					payment->PayFinalDelivery(cp, cp->count);
					delete cp;
					continue; // of the loop

				case MTA_CARGO_LOAD:
					cp->loaded_at_xy = data;
					break;

				case MTA_TRANSFER:
					cp->feeder_share += payment->PayTransfer(cp, cp->count);
					break;

				case MTA_UNLOAD:
					break;
			}
			dest->Append(cp);
			continue;
		}

		/* Can move only part of the packet */
		if (mta == MTA_FINAL_DELIVERY) {
			/* Final delivery doesn't need package splitting. */
			payment->PayFinalDelivery(cp, max_move);
			this->count -= max_move;
			this->cargo_days_in_transit -= max_move * cp->days_in_transit;

			/* Final delivery payment pays the feeder share, so we have to
			 * reset that so it is not 'shown' twice for partial unloads. */
			this->feeder_share -= cp->feeder_share;
			cp->feeder_share = 0;
		} else {
			/* But... the rest needs package splitting. */
			Money fs = cp->feeder_share * max_move / static_cast<uint>(cp->count);
			cp->feeder_share -= fs;

			CargoPacket *cp_new = new CargoPacket(max_move, cp->days_in_transit, fs, cp->source_type, cp->source_id);

			cp_new->source          = cp->source;
			cp_new->source_xy       = cp->source_xy;
			cp_new->loaded_at_xy    = (mta == MTA_CARGO_LOAD) ? data : cp->loaded_at_xy;

			this->RemoveFromCache(cp_new); // this reflects the changes in cp.

			if (mta == MTA_TRANSFER) {
				/* Add the feeder share before inserting in dest. */
				cp_new->feeder_share += payment->PayTransfer(cp_new, max_move);
			}

			dest->Append(cp_new);
		}
		cp->count -= max_move;

		max_move = 0;
	}

	return it != packets.end();
}

void CargoList::InvalidateCache()
{
	this->count = 0;
	this->feeder_share = 0;
	this->cargo_days_in_transit = 0;

	for (List::const_iterator it = this->packets.begin(); it != this->packets.end(); it++) {
		this->AddToCache(*it);
	}
}
