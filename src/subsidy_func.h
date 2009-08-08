/* $Id$ */

/** @file subsidy_func.h Functions related to subsidies. */

#ifndef SUBSIDY_FUNC_H
#define SUBSIDY_FUNC_H

#include "core/geometry_type.hpp"
#include "station_type.h"
#include "town_type.h"
#include "industry_type.h"
#include "company_type.h"

Pair SetupSubsidyDecodeParam(const struct Subsidy *s, bool mode);
void DeleteSubsidyWith(SourceType type, SourceID index);
bool CheckSubsidised(CargoID cargo_type, CompanyID company, SourceType src_type, SourceID src, const Station *st);
void SubsidyMonthlyHandler();
void RebuildSubsidisedSourceAndDestinationCache();
void DeleteSubsidy(struct Subsidy *s);

#endif /* SUBSIDY_FUNC_H */
