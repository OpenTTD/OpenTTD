/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file refresh.h Definition of link refreshing utility. */

#include "../stdafx.h"
#include "../core/bitmath_func.hpp"
#include "../station_func.h"
#include "../engine_base.h"
#include "../vehicle_func.h"
#include "refresh.h"
#include "linkgraph.h"

#include "../safeguards.h"

/**
 * Refresh all links the given vehicle will visit.
 * @param v Vehicle to refresh links for.
 * @param allow_merge If the refresher is allowed to merge or extend link graphs.
 * @param is_full_loading If the vehicle is full loading.
 */
/* static */ void LinkRefresher::Run(Vehicle *v, bool allow_merge, bool is_full_loading)
{
	/* If there are no orders we can't predict anything.*/
	if (v->orders == nullptr) return;

	/* Make sure the first order is a useful order. */
	const Order *first = v->orders->GetNextDecisionNode(v->GetOrder(v->cur_implicit_order_index), 0);
	if (first == nullptr) return;

	HopSet seen_hops;
	LinkRefresher refresher(v, &seen_hops, allow_merge, is_full_loading);

	refresher.RefreshLinks(first, first, v->last_loading_station != INVALID_STATION ? 1 << HAS_CARGO : 0);
}

/**
 * Comparison operator to allow hops to be used in a std::set.
 * @param other Other hop to be compared with.
 * @return If this hop is "smaller" than the other (defined by from, to and cargo in this order).
 */
bool LinkRefresher::Hop::operator<(const Hop &other) const
{
	if (this->from < other.from) {
		return true;
	} else if (this->from > other.from) {
		return false;
	}
	if (this->to < other.to) {
		return true;
	} else if (this->to > other.to) {
		return false;
	}
	return this->cargo < other.cargo;
}

/**
 * Constructor for link refreshing algorithm.
 * @param vehicle Vehicle to refresh links for.
 * @param seen_hops Set of hops already seen. This is shared between this
 *                  refresher and all its children.
 * @param allow_merge If the refresher is allowed to merge or extend link graphs.
 * @param is_full_loading If the vehicle is full loading.
 */
LinkRefresher::LinkRefresher(Vehicle *vehicle, HopSet *seen_hops, bool allow_merge, bool is_full_loading) :
	vehicle(vehicle), seen_hops(seen_hops), cargo(CT_INVALID), allow_merge(allow_merge),
	is_full_loading(is_full_loading)
{
	memset(this->capacities, 0, sizeof(this->capacities));

	/* Assemble list of capacities and set last loading stations to 0. */
	for (Vehicle *v = this->vehicle; v != nullptr; v = v->Next()) {
		this->refit_capacities.push_back(RefitDesc(v->cargo_type, v->cargo_cap, v->refit_cap));
		if (v->refit_cap > 0) {
			assert(v->cargo_type < NUM_CARGO);
			this->capacities[v->cargo_type] += v->refit_cap;
		}
	}
}

/**
 * Handle refit orders by updating capacities and refit_capacities.
 * @param refit_cargo Cargo to refit to.
 * @return True if any vehicle was refit; false if none was.
 */
bool LinkRefresher::HandleRefit(CargoID refit_cargo)
{
	this->cargo = refit_cargo;
	RefitList::iterator refit_it = this->refit_capacities.begin();
	bool any_refit = false;
	for (Vehicle *v = this->vehicle; v != nullptr; v = v->Next()) {
		const Engine *e = Engine::Get(v->engine_type);
		if (!HasBit(e->info.refit_mask, this->cargo)) {
			++refit_it;
			continue;
		}
		any_refit = true;

		/* Back up the vehicle's cargo type */
		CargoID temp_cid = v->cargo_type;
		byte temp_subtype = v->cargo_subtype;
		v->cargo_type = this->cargo;
		v->cargo_subtype = GetBestFittingSubType(v, v, this->cargo);

		uint16_t mail_capacity = 0;
		uint amount = e->DetermineCapacity(v, &mail_capacity);

		/* Restore the original cargo type */
		v->cargo_type = temp_cid;
		v->cargo_subtype = temp_subtype;

		/* Skip on next refit. */
		if (this->cargo != refit_it->cargo && refit_it->remaining > 0) {
			this->capacities[refit_it->cargo] -= refit_it->remaining;
			refit_it->remaining = 0;
		} else if (amount < refit_it->remaining) {
			this->capacities[refit_it->cargo] -= refit_it->remaining - amount;
			refit_it->remaining = amount;
		}
		refit_it->capacity = amount;
		refit_it->cargo = this->cargo;

		++refit_it;

		/* Special case for aircraft with mail. */
		if (v->type == VEH_AIRCRAFT) {
			if (mail_capacity < refit_it->remaining) {
				this->capacities[refit_it->cargo] -= refit_it->remaining - mail_capacity;
				refit_it->remaining = mail_capacity;
			}
			refit_it->capacity = mail_capacity;
			break; // aircraft have only one vehicle
		}
	}
	return any_refit;
}

/**
 * Restore capacities and refit_capacities as vehicle might have been able to load now.
 */
void LinkRefresher::ResetRefit()
{
	for (auto &it : this->refit_capacities) {
		if (it.remaining == it.capacity) continue;
		this->capacities[it.cargo] += it.capacity - it.remaining;
		it.remaining = it.capacity;
	}
}

/**
 * Predict the next order the vehicle will execute and resolve conditionals by
 * recursion and return next non-conditional order in list.
 * @param cur Current order being evaluated.
 * @param next Next order to be evaluated.
 * @param flags RefreshFlags to give hints about the previous link and state carried over from that.
 * @param num_hops Number of hops already taken by recursive calls to this method.
 * @return new next Order.
 */
const Order *LinkRefresher::PredictNextOrder(const Order *cur, const Order *next, uint8_t flags, uint num_hops)
{
	/* next is good if it's either nullptr (then the caller will stop the
	 * evaluation) or if it's not conditional and the caller allows it to be
	 * chosen (by setting USE_NEXT). */
	while (next != nullptr && (!HasBit(flags, USE_NEXT) || next->IsType(OT_CONDITIONAL))) {

		/* After the first step any further non-conditional order is good,
		 * regardless of previous USE_NEXT settings. The case of cur and next or
		 * their respective stations being equal is handled elsewhere. */
		SetBit(flags, USE_NEXT);

		if (next->IsType(OT_CONDITIONAL)) {
			const Order *skip_to = this->vehicle->orders->GetNextDecisionNode(
					this->vehicle->orders->GetOrderAt(next->GetConditionSkipToOrder()), num_hops);
			if (skip_to != nullptr && num_hops < this->vehicle->orders->GetNumOrders()) {
				/* Make copies of capacity tracking lists. There is potential
				 * for optimization here: If the vehicle never refits we don't
				 * need to copy anything. Also, if we've seen the branched link
				 * before we don't need to branch at all. */
				LinkRefresher branch(*this);
				branch.RefreshLinks(cur, skip_to, flags, num_hops + 1);
			}
		}

		/* Reassign next with the following stop. This can be a station or a
		 * depot.*/
		next = this->vehicle->orders->GetNextDecisionNode(
				this->vehicle->orders->GetNext(next), num_hops++);
	}
	return next;
}

/**
 * Refresh link stats for the given pair of orders.
 * @param cur Last stop where the consist could interact with cargo.
 * @param next Next order to be processed.
 */
void LinkRefresher::RefreshStats(const Order *cur, const Order *next)
{
	StationID next_station = next->GetDestination();
	Station *st = Station::GetIfValid(cur->GetDestination());
	if (st != nullptr && next_station != INVALID_STATION && next_station != st->index) {
		Station *st_to = Station::Get(next_station);
		for (CargoID c = 0; c < NUM_CARGO; c++) {
			/* Refresh the link and give it a minimum capacity. */

			uint cargo_quantity = this->capacities[c];
			if (cargo_quantity == 0) continue;

			if (this->vehicle->GetDisplayMaxSpeed() == 0) continue;

			/* If not allowed to merge link graphs, make sure the stations are
			 * already in the same link graph. */
			if (!this->allow_merge && st->goods[c].link_graph != st_to->goods[c].link_graph) {
				continue;
			}

			/* A link is at least partly restricted if a vehicle can't load at its source. */
			EdgeUpdateMode restricted_mode = (cur->GetLoadType() & OLFB_NO_LOAD) == 0 ?
						EUM_UNRESTRICTED : EUM_RESTRICTED;
			/* This estimates the travel time of the link as the time needed
			 * to travel between the stations at half the max speed of the consist.
			 * The result is in tiles/tick (= 2048 km-ish/h). */
			uint32_t time_estimate = DistanceManhattan(st->xy, st_to->xy) * 4096U / this->vehicle->GetDisplayMaxSpeed();

			/* If the vehicle is currently full loading, increase the capacities at the station
			 * where it is loading by an estimate of what it would have transported if it wasn't
			 * loading. Don't do that if the vehicle has been waiting for longer than the entire
			 * order list is supposed to take, though. If that is the case the total duration is
			 * probably far off and we'd greatly overestimate the capacity by increasing.*/
			if (this->is_full_loading && this->vehicle->orders != nullptr &&
					st->index == vehicle->last_station_visited &&
					this->vehicle->orders->GetTotalDuration() > this->vehicle->current_order_time) {
				uint effective_capacity = cargo_quantity * this->vehicle->load_unload_ticks;
				if (effective_capacity > (uint)this->vehicle->orders->GetTotalDuration()) {
					IncreaseStats(st, c, next_station, effective_capacity /
							this->vehicle->orders->GetTotalDuration(), 0, 0,
							EUM_INCREASE | restricted_mode);
				} else if (RandomRange(this->vehicle->orders->GetTotalDuration()) < effective_capacity) {
					IncreaseStats(st, c, next_station, 1, 0, 0, EUM_INCREASE | restricted_mode);
				} else {
					IncreaseStats(st, c, next_station, cargo_quantity, 0, time_estimate, EUM_REFRESH | restricted_mode);
				}
			} else {
				IncreaseStats(st, c, next_station, cargo_quantity, 0, time_estimate, EUM_REFRESH | restricted_mode);
			}
		}
	}
}

/**
 * Iterate over orders starting at \a cur and \a next and refresh links
 * associated with them. \a cur and \a next can be equal. If they're not they
 * must be "neighbours" in their order list, which means \a next must be directly
 * reachable from \a cur without passing any further OT_GOTO_STATION or
 * OT_IMPLICIT orders in between.
 * @param cur Current order being evaluated.
 * @param next Next order to be checked.
 * @param flags RefreshFlags to give hints about the previous link and state carried over from that.
 * @param num_hops Number of hops already taken by recursive calls to this method.
 */
void LinkRefresher::RefreshLinks(const Order *cur, const Order *next, uint8_t flags, uint num_hops)
{
	while (next != nullptr) {

		if ((next->IsType(OT_GOTO_DEPOT) || next->IsType(OT_GOTO_STATION)) && next->IsRefit()) {
			SetBit(flags, WAS_REFIT);
			if (!next->IsAutoRefit()) {
				this->HandleRefit(next->GetRefitCargo());
			} else if (!HasBit(flags, IN_AUTOREFIT)) {
				SetBit(flags, IN_AUTOREFIT);
				LinkRefresher backup(*this);
				for (CargoID c = 0; c != NUM_CARGO; ++c) {
					if (CargoSpec::Get(c)->IsValid() && this->HandleRefit(c)) {
						this->RefreshLinks(cur, next, flags, num_hops);
						*this = backup;
					}
				}
			}
		}

		/* Only reset the refit capacities if the "previous" next is a station,
		 * meaning that either the vehicle was refit at the previous station or
		 * it wasn't at all refit during the current hop. */
		if (HasBit(flags, WAS_REFIT) && (next->IsType(OT_GOTO_STATION) || next->IsType(OT_IMPLICIT))) {
			SetBit(flags, RESET_REFIT);
		} else {
			ClrBit(flags, RESET_REFIT);
		}

		next = this->PredictNextOrder(cur, next, flags, num_hops);
		if (next == nullptr) break;
		Hop hop(cur->index, next->index, this->cargo);
		if (this->seen_hops->find(hop) != this->seen_hops->end()) {
			break;
		} else {
			this->seen_hops->insert(hop);
		}

		/* Don't use the same order again, but choose a new one in the next round. */
		ClrBit(flags, USE_NEXT);

		/* Skip resetting and link refreshing if next order won't do anything with cargo. */
		if (!next->IsType(OT_GOTO_STATION) && !next->IsType(OT_IMPLICIT)) continue;

		if (HasBit(flags, RESET_REFIT)) {
			this->ResetRefit();
			ClrBit(flags, RESET_REFIT);
			ClrBit(flags, WAS_REFIT);
		}

		if (cur->IsType(OT_GOTO_STATION) || cur->IsType(OT_IMPLICIT)) {
			if (cur->CanLeaveWithCargo(HasBit(flags, HAS_CARGO))) {
				SetBit(flags, HAS_CARGO);
				this->RefreshStats(cur, next);
			} else {
				ClrBit(flags, HAS_CARGO);
			}
		}

		/* "cur" is only assigned here if the stop is a station so that
		 * whenever stats are to be increased two stations can be found. */
		cur = next;
	}
}
