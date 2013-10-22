/* $Id$ */

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

/**
 * Refresh all links the given vehicle will visit.
 * @param v Vehicle to refresh links for.
 */
/* static */ void LinkRefresher::Run(Vehicle *v)
{
	/* If there are no orders we can't predict anything.*/
	if (v->orders.list == NULL) return;

	LinkRefresher refresher(v);

	if (refresher.first == NULL) return;
	refresher.RefreshLinks(refresher.first, refresher.first, v->last_loading_station != INVALID_STATION ? 1 << HAS_CARGO : 0);
}

/**
 * Constructor for link refreshing algorithm.
 * @param vehicle Vehicle to refresh links for.
 */
LinkRefresher::LinkRefresher(Vehicle *vehicle) : vehicle(vehicle), hops(0)
{
	/* Assemble list of capacities and set last loading stations to 0. */
	for (Vehicle *v = this->vehicle; v != NULL; v = v->Next()) {
		this->refit_capacities.push_back(RefitDesc(v->cargo_type, v->cargo_cap, v->refit_cap));
		if (v->refit_cap > 0) this->capacities[v->cargo_type] += v->refit_cap;
	}

	this->first = this->vehicle->GetOrder(this->vehicle->cur_implicit_order_index);

	/* Make sure the first order is a useful order. */
	this->first = this->vehicle->orders.list->GetNextDecisionNode(this->first, 0);
}

/**
 * Handle refit orders by updating capacities and refit_capacities.
 * @param next Order to be processed.
 */
void LinkRefresher::HandleRefit(const Order *next)
{
	CargoID new_cid = next->GetRefitCargo();
	RefitList::iterator refit_it = this->refit_capacities.begin();
	for (Vehicle *v = this->vehicle; v != NULL; v = v->Next()) {
		const Engine *e = Engine::Get(v->engine_type);
		if (!HasBit(e->info.refit_mask, new_cid)) {
			++refit_it;
			continue;
		}

		/* Back up the vehicle's cargo type */
		CargoID temp_cid = v->cargo_type;
		byte temp_subtype = v->cargo_subtype;
		v->cargo_type = new_cid;
		v->cargo_subtype = GetBestFittingSubType(v, v, new_cid);

		uint16 mail_capacity = 0;
		uint amount = e->DetermineCapacity(v, &mail_capacity);

		/* Restore the original cargo type */
		v->cargo_type = temp_cid;
		v->cargo_subtype = temp_subtype;

		/* Skip on next refit. */
		if (new_cid != refit_it->cargo && refit_it->remaining > 0) {
			this->capacities[refit_it->cargo] -= refit_it->remaining;
			refit_it->remaining = 0;
		} else if (amount < refit_it->remaining) {
			this->capacities[refit_it->cargo] -= refit_it->remaining - amount;
			refit_it->remaining = amount;
		}
		refit_it->capacity = amount;
		refit_it->cargo = new_cid;

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
}

/**
 * Restore capacities and refit_capacities as vehicle might have been able to load now.
 */
void LinkRefresher::ResetRefit()
{
	for (RefitList::iterator it(this->refit_capacities.begin()); it != this->refit_capacities.end(); ++it) {
		if (it->remaining == it->capacity) continue;
		this->capacities[it->cargo] += it->capacity - it->remaining;
		it->remaining = it->capacity;
	}
}

/**
 * Predict the next order the vehicle will execute and resolve conditionals by
 * recursion and return next non-conditional order in list.
 * @param cur Current order being evaluated.
 * @param next Next order to be evaluated.
 * @param flags RefreshFlags to give hints about the previous link and state carried over from that.
 * @return new next Order.
 */
const Order *LinkRefresher::PredictNextOrder(const Order *cur, const Order *next, uint8 flags)
{
	do {
		if (HasBit(flags, USE_NEXT)) {
			/* First incrementation has to be skipped if a "real" next hop,
			 * different from cur, was given. */
			ClrBit(flags, USE_NEXT);
		} else {
			const Order *skip_to = NULL;
			if (next->IsType(OT_CONDITIONAL)) {
				skip_to = this->vehicle->orders.list->GetNextDecisionNode(
						this->vehicle->orders.list->GetOrderAt(next->GetConditionSkipToOrder()),
						this->hops / 2);
			}

			/* Reassign next with the following stop. This can be a station or a
			 * depot. Allow the order list to be walked twice so that we can
			 * reassign "first" below without afterwards terminating early here. */
			next = this->vehicle->orders.list->GetNextDecisionNode(
					this->vehicle->orders.list->GetNext(next), ++this->hops / 2);

			if (skip_to != NULL) {
				/* Make copies of capacity tracking lists. There is potential
				 * for optimization here. If the vehicle never refits we don't
				 * need to copy anything. Also, if we've seen the branched link
				 * before we don't need to branch at all. */
				++this->hops;
				LinkRefresher branch(*this);
				branch.RefreshLinks(cur, skip_to, flags | (cur != skip_to ? 1 << USE_NEXT : 0));
				this->hops = branch.hops;
			}
		}
	} while (next != NULL && next->IsType(OT_CONDITIONAL));
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
	if (st != NULL && next_station != INVALID_STATION && next_station != st->index) {
		for (CapacitiesMap::const_iterator i = this->capacities.begin(); i != this->capacities.end(); ++i) {
			/* Refresh the link and give it a minimum capacity. */
			if (i->second == 0) continue;
			/* A link is at least partly restricted if a
			 * vehicle can't load at its source. */
			IncreaseStats(st, i->first, next_station, i->second,
					(cur->GetLoadType() & OLFB_NO_LOAD) == 0 ? LinkGraph::REFRESH_UNRESTRICTED : LinkGraph::REFRESH_RESTRICTED);
		}
	}
}

/**
 * Iterate over orders starting at \a cur and \a next and refresh links
 * associated with them. \a cur and \a next can be equal. If they're not they
 * must be "neigbours" in their order list, which means \a next must be directly
 * reachable from \a cur without passing any further OT_GOTO_STATION or
 * OT_IMPLICIT orders in between.
 * @param cur Current order being evaluated.
 * @param next Next order to be checked.
 * @param flags RefreshFlags to give hints about the previous link and state carried over from that.
 */
void LinkRefresher::RefreshLinks(const Order *cur, const Order *next, uint8 flags)
{
	while (next != NULL) {

		/* If the refit cargo is CT_AUTO_REFIT, we're optimistic and assume the
		 * cargo will stay the same. The point of this method is to avoid
		 * deadlocks due to vehicles waiting for cargo that isn't being routed,
		 * yet. That situation will not occur if the vehicle is actually
		 * carrying a different cargo in the end. */
		if ((next->IsType(OT_GOTO_DEPOT) || next->IsType(OT_GOTO_STATION)) &&
				next->IsRefit() && !next->IsAutoRefit()) {
			SetBit(flags, WAS_REFIT);
			this->HandleRefit(next);
		}

		/* Only reset the refit capacities if the "previous" next is a station,
		 * meaning that either the vehicle was refit at the previous station or
		 * it wasn't at all refit during the current hop. */
		if (HasBit(flags, WAS_REFIT) && (next->IsType(OT_GOTO_STATION) || next->IsType(OT_IMPLICIT))) {
			SetBit(flags, RESET_REFIT);
		} else {
			ClrBit(flags, RESET_REFIT);
		}

		next = this->PredictNextOrder(cur, next, flags);
		if (next == NULL) break;

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
		 * whenever stats are to be increased two stations can be found.
		 * However, "first" can be a depot stop. If that is the case
		 * reassign it to make sure we end up with a station for the last
		 * link. */
		cur = next;
		if (cur == this->first) break;
		if (!this->first->IsType(OT_GOTO_STATION) && !this->first->IsType(OT_IMPLICIT)) {
			this->first = cur;
		}
	}
}
