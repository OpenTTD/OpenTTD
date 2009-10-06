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

void InitializeCargoPackets()
{
	_cargopacket_pool.CleanPool();
}

CargoPacket::CargoPacket(StationID source, uint16 count, SourceType source_type, SourceID source_id) :
	count(count),
	source(source),
	source_id(source_id)
{
	this->source_type     = source_type;

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
	while (!packets.empty()) {
		delete packets.front();
		packets.pop_front();
	}
}

void CargoList::AgeCargo()
{
	if (empty) return;

	uint dit = 0;
	for (List::const_iterator it = packets.begin(); it != packets.end(); it++) {
		if ((*it)->days_in_transit != 0xFF) (*it)->days_in_transit++;
		dit += (*it)->days_in_transit * (*it)->count;
	}
	days_in_transit = dit / count;
}

void CargoList::Append(CargoPacket *cp)
{
	assert(cp != NULL);

	for (List::iterator it = packets.begin(); it != packets.end(); it++) {
		if ((*it)->SameSource(cp) && (*it)->count + cp->count <= CargoPacket::MAX_COUNT) {
			(*it)->count        += cp->count;
			(*it)->feeder_share += cp->feeder_share;
			delete cp;

			InvalidateCache();
			return;
		}
	}

	/* The packet could not be merged with another one */
	packets.push_back(cp);
	InvalidateCache();
}


void CargoList::Truncate(uint count)
{
	for (List::iterator it = packets.begin(); it != packets.end(); it++) {
		uint local_count = (*it)->count;
		if (local_count <= count) {
			count -= local_count;
			continue;
		}

		(*it)->count = count;
		count = 0;
	}

	while (!packets.empty()) {
		CargoPacket *cp = packets.back();
		if (cp->count != 0) break;
		delete cp;
		packets.pop_back();
	}

	InvalidateCache();
}

bool CargoList::MoveTo(CargoList *dest, uint count, CargoList::MoveToAction mta, CargoPayment *payment, uint data)
{
	assert(mta == MTA_FINAL_DELIVERY || dest != NULL);
	assert(mta == MTA_UNLOAD || mta == MTA_CARGO_LOAD || payment != NULL);
	CargoList tmp;

	while (!packets.empty() && count > 0) {
		CargoPacket *cp = *packets.begin();
		if (cp->count <= count) {
			/* Can move the complete packet */
			packets.remove(cp);
			switch (mta) {
				case MTA_FINAL_DELIVERY:
					if (cp->source == data) {
						tmp.Append(cp);
					} else {
						payment->PayFinalDelivery(cp, cp->count);
						count -= cp->count;
						delete cp;
					}
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
			count -= cp->count;
			dest->packets.push_back(cp);
		} else {
			/* Can move only part of the packet, so split it into two pieces */
			if (mta != MTA_FINAL_DELIVERY) {
				CargoPacket *cp_new = new CargoPacket();

				Money fs = cp->feeder_share * count / static_cast<uint>(cp->count);
				cp->feeder_share -= fs;

				cp_new->source          = cp->source;
				cp_new->source_xy       = cp->source_xy;
				cp_new->loaded_at_xy    = (mta == MTA_CARGO_LOAD) ? data : cp->loaded_at_xy;

				cp_new->days_in_transit = cp->days_in_transit;
				cp_new->feeder_share    = fs;

				cp_new->source_type     = cp->source_type;
				cp_new->source_id       = cp->source_id;

				cp_new->count = count;
				dest->packets.push_back(cp_new);

				if (mta == MTA_TRANSFER) cp_new->feeder_share += payment->PayTransfer(cp_new, count);
			} else {
				payment->PayFinalDelivery(cp, count);
			}
			cp->count -= count;

			count = 0;
		}
	}

	bool remaining = !packets.empty();

	if (mta == MTA_FINAL_DELIVERY && !tmp.Empty()) {
		/* There are some packets that could not be delivered at the station, put them back */
		tmp.MoveTo(this, UINT_MAX, MTA_UNLOAD, NULL);
		tmp.packets.clear();
	}

	if (dest != NULL) dest->InvalidateCache();
	InvalidateCache();

	return remaining;
}

void CargoList::InvalidateCache()
{
	empty = packets.empty();
	count = 0;
	feeder_share = 0;
	source = INVALID_STATION;
	days_in_transit = 0;

	if (empty) return;

	uint dit = 0;
	for (List::const_iterator it = packets.begin(); it != packets.end(); it++) {
		count        += (*it)->count;
		dit          += (*it)->days_in_transit * (*it)->count;
		feeder_share += (*it)->feeder_share;
	}
	days_in_transit = dit / count;
	source = (*packets.begin())->source;
}

