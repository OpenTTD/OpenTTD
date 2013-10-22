/* $Id$ */

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
#include <list>
#include <map>

/**
 * Utility to refresh links a consist will visit.
 */
class LinkRefresher {
public:
	static void Run(Vehicle *v);

protected:
	/**
	 * Various flags about properties of the last examined link that might have
	 * an influence on the next one.
	 */
	enum RefreshFlags {
		USE_NEXT,   ///< There has been a previous link. Try to use the given next order.
		HAS_CARGO,  ///< Consist could leave the last stop where it could interact with cargo carrying cargo (i.e. not an "unload all" + "no loading" order).
		WAS_REFIT,  ///< Consist was refit since the last stop where it could interact with cargo.
		RESET_REFIT ///< Consist had a chance to load since the last refit and the refit capacities can be reset.
	};

	/**
	 * Simulated cargo type and capacity for prediction of future links.
	 */
	struct RefitDesc {
		CargoID cargo;    ///< Cargo type the vehicle will be carrying.
		uint16 capacity;  ///< Capacity the vehicle will have.
		uint16 remaining; ///< Capacity remaining from before the previous refit.
		RefitDesc(CargoID cargo, uint16 capacity, uint16 remaining) :
				cargo(cargo), capacity(capacity), remaining(remaining) {}
	};

	typedef std::list<RefitDesc> RefitList;
	typedef std::map<CargoID, uint> CapacitiesMap;

	Vehicle *vehicle;           ///< Vehicle for which the links should be refreshed.
	const Order *first;         ///< Order to be checked first. If this is encountered again the refreshing is considered finished.
	CapacitiesMap capacities;   ///< Current added capacities per cargo ID in the consist.
	RefitList refit_capacities; ///< Current state of capacity remaining from previous refits versus overall capacity per vehicle in the consist.
	uint hops;                  ///< Number of hops already used up. If more than two times the number of orders in the list have been checked refreshing is stopped.
	LinkRefresher(Vehicle *v);

	void HandleRefit(const Order *next);
	void ResetRefit();
	void RefreshStats(const Order *cur, const Order *next);
	const Order *PredictNextOrder(const Order *cur, const Order *next, uint8 flags);

	void RefreshLinks(const Order *cur, const Order *next, uint8 flags);
};

#endif // REFRESH_H
