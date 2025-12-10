/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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
	VehicleOrderID first = v->orders->GetNextDecisionNode(v->cur_implicit_order_index, 0);
	if (first == INVALID_VEH_ORDER_ID) return;

	HopSet seen_hops;
	LinkRefresher refresher(v, &seen_hops, allow_merge, is_full_loading);

	refresher.RefreshLinks(first, first, v->last_loading_station != StationID::Invalid() ? RefreshFlags{RefreshFlag::HasCargo} : RefreshFlags{});
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
	vehicle(vehicle), seen_hops(seen_hops), cargo(INVALID_CARGO), allow_merge(allow_merge),
	is_full_loading(is_full_loading)
{
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
bool LinkRefresher::HandleRefit(CargoType refit_cargo)
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
		CargoType temp_cargo_type = v->cargo_type;
		uint8_t temp_subtype = v->cargo_subtype;
		v->cargo_type = this->cargo;
		v->cargo_subtype = GetBestFittingSubType(v, v, this->cargo);

		uint16_t mail_capacity = 0;
		uint amount = e->DetermineCapacity(v, &mail_capacity);

		/* Restore the original cargo type */
		v->cargo_type = temp_cargo_type;
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
VehicleOrderID LinkRefresher::PredictNextOrder(VehicleOrderID cur, VehicleOrderID next, RefreshFlags flags, uint num_hops)
{
	assert(this->vehicle->orders != nullptr);
	const OrderList &orderlist = *this->vehicle->orders;
	auto orders = orderlist.GetOrders();

	/* next is good if it's either nullptr (then the caller will stop the
	 * evaluation) or if it's not conditional and the caller allows it to be
	 * chosen (by setting RefreshFlag::UseNext). */
	while (next < orderlist.GetNumOrders() && (!flags.Test(RefreshFlag::UseNext) || orders[next].IsType(OT_CONDITIONAL))) {

		/* After the first step any further non-conditional order is good,
		 * regardless of previous RefreshFlag::UseNext settings. The case of cur and next or
		 * their respective stations being equal is handled elsewhere. */
		flags.Set(RefreshFlag::UseNext);

		if (orders[next].IsType(OT_CONDITIONAL)) {
			VehicleOrderID skip_to = orderlist.GetNextDecisionNode(orders[next].GetConditionSkipToOrder(), num_hops);
			if (skip_to != INVALID_VEH_ORDER_ID && num_hops < orderlist.GetNumOrders()) {
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
		next = orderlist.GetNextDecisionNode(orderlist.GetNext(next), num_hops++);
	}
	return next;
}

/**
 * Refresh link stats for the given pair of orders.
 * @param cur Last stop where the consist could interact with cargo.
 * @param next Next order to be processed.
 */
void LinkRefresher::RefreshStats(VehicleOrderID cur, VehicleOrderID next)
{
	assert(this->vehicle->orders != nullptr);
	const OrderList &orderlist = *this->vehicle->orders;
	auto orders = orderlist.GetOrders();

	StationID next_station = orders[next].GetDestination().ToStationID();
	Station *st = Station::GetIfValid(orders[cur].GetDestination().ToStationID());
	if (st != nullptr && next_station != StationID::Invalid() && next_station != st->index) {
		Station *st_to = Station::Get(next_station);
		for (CargoType cargo = 0; cargo < NUM_CARGO; ++cargo) {
			/* Refresh the link and give it a minimum capacity. */

			uint cargo_quantity = this->capacities[cargo];
			if (cargo_quantity == 0) continue;

			if (this->vehicle->GetDisplayMaxSpeed() == 0) continue;

			/* If not allowed to merge link graphs, make sure the stations are
			 * already in the same link graph. */
			if (!this->allow_merge && st->goods[cargo].link_graph != st_to->goods[cargo].link_graph) {
				continue;
			}

			/* A link is at least partly restricted if a vehicle can't load at its source. */
			EdgeUpdateMode restricted_mode = orders[cur].GetLoadType() != OrderLoadType::NoLoad ?
						EdgeUpdateMode::Unrestricted : EdgeUpdateMode::Restricted;
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
					IncreaseStats(st, cargo, next_station, effective_capacity /
							this->vehicle->orders->GetTotalDuration(), 0, 0,
							{EdgeUpdateMode::Increase, restricted_mode});
				} else if (RandomRange(this->vehicle->orders->GetTotalDuration()) < effective_capacity) {
					IncreaseStats(st, cargo, next_station, 1, 0, 0, {EdgeUpdateMode::Increase, restricted_mode});
				} else {
					IncreaseStats(st, cargo, next_station, cargo_quantity, 0, time_estimate, {EdgeUpdateMode::Refresh, restricted_mode});
				}
			} else {
				IncreaseStats(st, cargo, next_station, cargo_quantity, 0, time_estimate, {EdgeUpdateMode::Refresh, restricted_mode});
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
void LinkRefresher::RefreshLinks(VehicleOrderID cur, VehicleOrderID next, RefreshFlags flags, uint num_hops)
{
	assert(this->vehicle->orders != nullptr);
	const OrderList &orderlist = *this->vehicle->orders;
	while (next < orderlist.GetNumOrders()) {
		const Order *next_order = orderlist.GetOrderAt(next);

		if ((next_order->IsType(OT_GOTO_DEPOT) || next_order->IsType(OT_GOTO_STATION)) && next_order->IsRefit()) {
			flags.Set(RefreshFlag::WasRefit);
			if (!next_order->IsAutoRefit()) {
				this->HandleRefit(next_order->GetRefitCargo());
			} else if (!flags.Test(RefreshFlag::InAutorefit)) {
				flags.Set(RefreshFlag::InAutorefit);
				LinkRefresher backup(*this);
				for (CargoType cargo = 0; cargo != NUM_CARGO; ++cargo) {
					if (CargoSpec::Get(cargo)->IsValid() && this->HandleRefit(cargo)) {
						this->RefreshLinks(cur, next, flags, num_hops);
						*this = backup;
					}
				}
			}
		}

		/* Only reset the refit capacities if the "previous" next is a station,
		 * meaning that either the vehicle was refit at the previous station or
		 * it wasn't at all refit during the current hop. */
		if (flags.Test(RefreshFlag::WasRefit) && (next_order->IsType(OT_GOTO_STATION) || next_order->IsType(OT_IMPLICIT))) {
			flags.Set(RefreshFlag::ResetRefit);
		} else {
			flags.Reset(RefreshFlag::ResetRefit);
		}

		next = this->PredictNextOrder(cur, next, flags, num_hops);
		if (next == INVALID_VEH_ORDER_ID) break;
		Hop hop(cur, next, this->cargo);
		if (this->seen_hops->find(hop) != this->seen_hops->end()) {
			break;
		} else {
			this->seen_hops->insert(hop);
		}

		next_order = orderlist.GetOrderAt(next);

		/* Don't use the same order again, but choose a new one in the next round. */
		flags.Reset(RefreshFlag::UseNext);

		/* Skip resetting and link refreshing if next order won't do anything with cargo. */
		if (!next_order->IsType(OT_GOTO_STATION) && !next_order->IsType(OT_IMPLICIT)) continue;

		if (flags.Test(RefreshFlag::ResetRefit)) {
			this->ResetRefit();
			flags.Reset({RefreshFlag::ResetRefit, RefreshFlag::WasRefit});
		}

		const Order *cur_order = orderlist.GetOrderAt(cur);

		if (cur_order->IsType(OT_GOTO_STATION) || cur_order->IsType(OT_IMPLICIT)) {
			if (cur_order->CanLeaveWithCargo(flags.Test(RefreshFlag::HasCargo))) {
				flags.Set(RefreshFlag::HasCargo);
				this->RefreshStats(cur, next);
			} else {
				flags.Reset(RefreshFlag::HasCargo);
			}
		}

		/* "cur" is only assigned here if the stop is a station so that
		 * whenever stats are to be increased two stations can be found. */
		cur = next;
	}
}
