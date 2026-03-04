/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file demands.h Declaration of demand calculating link graph handler. */

#ifndef DEMANDS_H
#define DEMANDS_H

#include "linkgraphjob_base.h"

/**
 * Calculate the demands. This class has a state, but is recreated for each
 * call to of DemandHandler::Run.
 */
class DemandCalculator {
public:
	DemandCalculator(LinkGraphJob &job);

private:
	int32_t base_distance; ///< Base distance for scaling purposes.
	int32_t mod_dist;      ///< Distance modifier, determines how much demands decrease with distance.
	int32_t accuracy;      ///< Accuracy of the calculation.

	template <class Tscaler>
	void CalcDemand(LinkGraphJob &job, Tscaler scaler);
};

/**
 * Stateless, thread safe demand handler. Doesn't do anything but call DemandCalculator.
 */
class DemandHandler : public ComponentHandler {
public:

	/**
	 * Call the demand calculator on the given component.
	 * @param job Component to calculate the demands for.
	 */
	void Run(LinkGraphJob &job) const override { DemandCalculator c(job); }

	/**
	 * Virtual destructor has to be defined because of virtual Run().
	 */
	~DemandHandler() override = default;
};

#endif /* DEMANDS_H */
