/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file subsidy_base.h %Subsidy base class. */

#ifndef SUBSIDY_BASE_H
#define SUBSIDY_BASE_H

#include "cargo_type.h"
#include "company_type.h"
#include "source_type.h"
#include "subsidy_type.h"
#include "core/pool_type.hpp"

using SubsidyPool = Pool<Subsidy, SubsidyID, 1>;
extern SubsidyPool _subsidy_pool;

/** Struct about subsidies, offered and awarded */
struct Subsidy : SubsidyPool::PoolItem<&_subsidy_pool> {
	CargoType cargo_type = INVALID_CARGO;  ///< Cargo type involved in this subsidy, INVALID_CARGO for invalid subsidy
	uint16_t remaining = 0;    ///< Remaining months when this subsidy is valid
	CompanyID awarded = CompanyID::Invalid();   ///< Subsidy is awarded to this company; CompanyID::Invalid() if it's not awarded to anyone
	Source src{}; ///< Source of subsidised path
	Source dst{}; ///< Destination of subsidised path

	/**
	 * We need an (empty) constructor so struct isn't zeroed (as C++ standard states)
	 */
	Subsidy() { }
	Subsidy(CargoType cargo_type, Source src, Source dst, uint16_t remaining) : cargo_type(cargo_type), remaining(remaining), src(src), dst(dst) {}

	/**
	 * (Empty) destructor has to be defined else operator delete might be called with nullptr parameter
	 */
	~Subsidy() { }

	/**
	 * Tests whether this subsidy has been awarded to someone
	 * @return is this subsidy awarded?
	 */
	inline bool IsAwarded() const
	{
		return this->awarded != CompanyID::Invalid();
	}

	void AwardTo(CompanyID company);
};

/** Constants related to subsidies */
static const uint SUBSIDY_OFFER_MONTHS         =  12; ///< Duration of subsidy offer
static const uint SUBSIDY_PAX_MIN_POPULATION   = 400; ///< Min. population of towns for subsidised pax route
static const uint SUBSIDY_CARGO_MIN_POPULATION = 900; ///< Min. population of destination town for cargo route
static const uint SUBSIDY_MAX_PCT_TRANSPORTED  =  42; ///< Subsidy will be created only for towns/industries with less % transported
static const uint SUBSIDY_MAX_DISTANCE         =  70; ///< Max. length of subsidised route (DistanceManhattan)
static const uint SUBSIDY_TOWN_CARGO_RADIUS    =   6; ///< Extent of a tile area around town center when scanning for town cargo acceptance and production (6 ~= min catchmement + min station / 2)

#endif /* SUBSIDY_BASE_H */
