/* $Id$ */

/** @file subsidy_type.h Types related to subsidies. */

#ifndef SUBSIDY_TYPE_H
#define SUBSIDY_TYPE_H

#include "cargo_type.h"
#include "company_type.h"

/** Struct about subsidies, both offered and awarded */
struct Subsidy {
	CargoID cargo_type; ///< Cargo type involved in this subsidy, CT_INVALID for invalid subsidy
	byte age;           ///< Subsidy age; < 12 is unawarded, >= 12 is awarded
	uint16 from;        ///< Index of source. Either TownID, IndustryID or StationID, when awarded.
	uint16 to;          ///< Index of destination. Either TownID, IndustryID or StationID, when awarded.
};

extern Subsidy _subsidies[MAX_COMPANIES];

#define FOR_ALL_SUBSIDIES_FROM(var, start) for (var = &_subsidies[start]; var < endof(_subsidies); var++) \
		if (var->cargo_type != CT_INVALID)
#define FOR_ALL_SUBSIDIES(var) FOR_ALL_SUBSIDIES_FROM(var, 0)

#endif /* SUBSIDY_TYPE_H */
