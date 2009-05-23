/* $Id$ */

/** @file subsidy_func.h Functions related to subsidies. */

#ifndef SUBSIDY_FUNC_H
#define SUBSIDY_FUNC_H

#include "core/geometry_type.hpp"
#include "station_type.h"
#include "town_type.h"
#include "industry_type.h"
#include "company_type.h"
#include "subsidy_type.h"

Pair SetupSubsidyDecodeParam(const Subsidy *s, bool mode);
void DeleteSubsidyWithTown(TownID index);
void DeleteSubsidyWithIndustry(IndustryID index);
void DeleteSubsidyWithStation(StationID index);
bool CheckSubsidised(const Station *from, const Station *to, CargoID cargo_type);
void SubsidyMonthlyHandler();

#endif /* SUBSIDY_FUNC_H */
