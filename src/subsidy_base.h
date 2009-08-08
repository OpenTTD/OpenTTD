/* $Id$ */

/** @file subsidy_base.h Subsidy base class. */

#ifndef SUBSIDY_BASE_H
#define SUBSIDY_BASE_H

#include "cargo_type.h"
#include "company_type.h"
#include "subsidy_type.h"
#include "core/pool_type.hpp"

typedef Pool<Subsidy, SubsidyID, 1, MAX_COMPANIES> SubsidyPool;
extern SubsidyPool _subsidy_pool;

/** Struct about subsidies, offered and awarded */
struct Subsidy : SubsidyPool::PoolItem<&_subsidy_pool> {
	CargoID cargo_type;      ///< Cargo type involved in this subsidy, CT_INVALID for invalid subsidy
	byte remaining;          ///< Remaining months when this subsidy is valid
	CompanyByte awarded;     ///< Subsidy is awarded to this company; INVALID_COMPANY if it's not awarded to anyone
	SourceTypeByte src_type; ///< Source of subsidised path (ST_INDUSTRY or ST_TOWN)
	SourceTypeByte dst_type; ///< Destination of subsidised path (ST_INDUSTRY or ST_TOWN)
	SourceID src;            ///< Index of source. Either TownID or IndustryID
	SourceID dst;            ///< Index of destination. Either TownID or IndustryID

	/**
	 * We need an (empty) constructor so struct isn't zeroed (as C++ standard states)
	 */
	FORCEINLINE Subsidy() { }

	/**
	 * Tests whether this subsidy has been awarded to someone
	 * @return is this subsidy awarded?
	 */
	FORCEINLINE bool IsAwarded() const
	{
		return this->awarded != INVALID_COMPANY;
	}

	void AwardTo(CompanyID company);
};

/** Constants related to subsidies */
enum {
	SUBSIDY_OFFER_MONTHS         =  12, ///< Duration of subsidy offer
	SUBSIDY_CONTRACT_MONTHS      =  12, ///< Duration of subsidy after awarding
	SUBSIDY_PAX_MIN_POPULATION   = 400, ///< Min. population of towns for subsidised pax route
	SUBSIDY_CARGO_MIN_POPULATION = 900, ///< Min. population of destination town for cargo route
	SUBSIDY_MAX_PCT_TRANSPORTED  =  42, ///< Subsidy will be created only for towns/industries with less % transported
	SUBSIDY_MAX_DISTANCE         =  70, ///< Max. length of subsidised route (DistanceManhattan)
};

#define FOR_ALL_SUBSIDIES_FROM(var, start) FOR_ALL_ITEMS_FROM(Subsidy, subsidy_index, var, start)
#define FOR_ALL_SUBSIDIES(var) FOR_ALL_SUBSIDIES_FROM(var, 0)

#endif /* SUBSIDY_BASE_H */
