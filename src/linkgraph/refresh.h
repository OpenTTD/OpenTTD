/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file refresh.h Declaration of link refreshing utility. */

#ifndef REFRESH_H
#define REFRESH_H

#include "../cargo_type.h"
#include "../vehicle_base.h"

/**
 * Utility to refresh links a consist will visit.
 */
class LinkRefresher {
public:
	static void Run(Vehicle *v, bool allow_merge = true, bool is_full_loading = false);

protected:
	/**
	 * Various flags about properties of the last examined link that might have
	 * an influence on the next one.
	 */
	enum RefreshFlags {
		USE_NEXT,     ///< There was a conditional jump. Try to use the given next order when looking for a new one.
		HAS_CARGO,    ///< Consist could leave the last stop where it could interact with cargo carrying cargo (i.e. not an "unload all" + "no loading" order).
		WAS_REFIT,    ///< Consist was refit since the last stop where it could interact with cargo.
		RESET_REFIT,  ///< Consist had a chance to load since the last refit and the refit capacities can be reset.
		IN_AUTOREFIT, ///< Currently doing an autorefit loop. Ignore the first autorefit order.
	};

	/**
	 * Simulated cargo type and capacity for prediction of future links.
	 */
	struct RefitDesc {
		CargoID cargo;    ///< Cargo type the vehicle will be carrying.
		uint16_t capacity;  ///< Capacity the vehicle will have.
		uint16_t remaining; ///< Capacity remaining from before the previous refit.
		RefitDesc(CargoID cargo, uint16_t capacity, uint16_t remaining) :
				cargo(cargo), capacity(capacity), remaining(remaining) {}
	};

	/**
	 * A hop the refresh algorithm might evaluate. If the same hop is seen again
	 * the evaluation is stopped. This of course is a fairly simple heuristic.
	 * Sequences of refit orders can produce vehicles with all kinds of
	 * different cargoes and remembering only one can lead to early termination
	 * of the algorithm. However, as the order language is Turing complete, we
	 * are facing the halting problem here. At some point we have to draw the
	 * line.
	 */
	struct Hop {
		OrderID from;  ///< Last order where vehicle could interact with cargo or absolute first order.
		OrderID to;    ///< Next order to be processed.
		CargoID cargo; ///< Cargo the consist is probably carrying or CT_INVALID if unknown.

		/**
		 * Default constructor should not be called but has to be visible for
		 * usage in std::set.
		 */
		Hop() {NOT_REACHED();}

		/**
		 * Real constructor, only use this one.
		 * @param from First order of the hop.
		 * @param to Second order of the hop.
		 * @param cargo Cargo the consist is probably carrying when passing the hop.
		 */
		Hop(OrderID from, OrderID to, CargoID cargo) : from(from), to(to), cargo(cargo) {}
		bool operator<(const Hop &other) const;
	};

	typedef std::vector<RefitDesc> RefitList;
	typedef std::set<Hop> HopSet;

	Vehicle *vehicle;           ///< Vehicle for which the links should be refreshed.
	uint capacities[NUM_CARGO]; ///< Current added capacities per cargo ID in the consist.
	RefitList refit_capacities; ///< Current state of capacity remaining from previous refits versus overall capacity per vehicle in the consist.
	HopSet *seen_hops;          ///< Hops already seen. If the same hop is seen twice we stop the algorithm. This is shared between all Refreshers of the same run.
	CargoID cargo;              ///< Cargo given in last refit order.
	bool allow_merge;           ///< If the refresher is allowed to merge or extend link graphs.
	bool is_full_loading;       ///< If the vehicle is full loading.

	LinkRefresher(Vehicle *v, HopSet *seen_hops, bool allow_merge, bool is_full_loading);

	bool HandleRefit(CargoID refit_cargo);
	void ResetRefit();
	void RefreshStats(const Order *cur, const Order *next);
	const Order *PredictNextOrder(const Order *cur, const Order *next, uint8_t flags, uint num_hops = 0);

	void RefreshLinks(const Order *cur, const Order *next, uint8_t flags, uint num_hops = 0);
};

#endif /* REFRESH_H */
