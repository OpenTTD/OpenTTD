/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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
	enum class RefreshFlag : uint8_t {
		UseNext,     ///< There was a conditional jump. Try to use the given next order when looking for a new one.
		HasCargo,    ///< Consist could leave the last stop where it could interact with cargo carrying cargo (i.e. not an "unload all" + "no loading" order).
		WasRefit,    ///< Consist was refit since the last stop where it could interact with cargo.
		ResetRefit,  ///< Consist had a chance to load since the last refit and the refit capacities can be reset.
		InAutorefit, ///< Currently doing an autorefit loop. Ignore the first autorefit order.
	};

	using RefreshFlags = EnumBitSet<RefreshFlag, uint8_t>;

	/**
	 * Simulated cargo type and capacity for prediction of future links.
	 */
	struct RefitDesc {
		CargoType cargo;    ///< Cargo type the vehicle will be carrying.
		uint16_t capacity;  ///< Capacity the vehicle will have.
		uint16_t remaining; ///< Capacity remaining from before the previous refit.
		RefitDesc(CargoType cargo, uint16_t capacity, uint16_t remaining) :
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
		VehicleOrderID from;  ///< Last order where vehicle could interact with cargo or absolute first order.
		VehicleOrderID to;    ///< Next order to be processed.
		CargoType cargo; ///< Cargo the consist is probably carrying or INVALID_CARGO if unknown.

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
		Hop(VehicleOrderID from, VehicleOrderID to, CargoType cargo) : from(from), to(to), cargo(cargo) {}

		constexpr auto operator<=>(const Hop &) const noexcept = default;
	};

	typedef std::vector<RefitDesc> RefitList;
	typedef std::set<Hop> HopSet;

	Vehicle *vehicle;           ///< Vehicle for which the links should be refreshed.
	CargoArray capacities{}; ///< Current added capacities per cargo type in the consist.
	RefitList refit_capacities; ///< Current state of capacity remaining from previous refits versus overall capacity per vehicle in the consist.
	HopSet *seen_hops;          ///< Hops already seen. If the same hop is seen twice we stop the algorithm. This is shared between all Refreshers of the same run.
	CargoType cargo;              ///< Cargo given in last refit order.
	bool allow_merge;           ///< If the refresher is allowed to merge or extend link graphs.
	bool is_full_loading;       ///< If the vehicle is full loading.

	LinkRefresher(Vehicle *v, HopSet *seen_hops, bool allow_merge, bool is_full_loading);

	bool HandleRefit(CargoType refit_cargo);
	void ResetRefit();
	void RefreshStats(VehicleOrderID cur, VehicleOrderID next);
	VehicleOrderID PredictNextOrder(VehicleOrderID cur, VehicleOrderID next, RefreshFlags flags, uint num_hops = 0);

	void RefreshLinks(VehicleOrderID cur, VehicleOrderID next, RefreshFlags flags, uint num_hops = 0);
};

#endif /* REFRESH_H */
