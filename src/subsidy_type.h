/* $Id$ */

/** @file subsidy_type.h Types related to subsidies. */

#ifndef SUBSIDY_TYPE_H
#define SUBSIDY_TYPE_H

#include "cargo_type.h"
#include "company_type.h"

struct Subsidy {
	CargoID cargo_type;
	byte age;
	/* from and to can either be TownID, StationID or IndustryID */
	uint16 from;
	uint16 to;
};

extern Subsidy _subsidies[MAX_COMPANIES];

#endif /* SUBSIDY_TYPE_H */
