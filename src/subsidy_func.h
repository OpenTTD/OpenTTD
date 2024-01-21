/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file subsidy_func.h Functions related to subsidies. */

#ifndef SUBSIDY_FUNC_H
#define SUBSIDY_FUNC_H

#include "core/geometry_type.h"
#include "station_type.h"
#include "company_type.h"
#include "cargo_type.h"
#include "news_type.h"
#include "subsidy_base.h"

std::pair<NewsReferenceType, NewsReferenceType> SetupSubsidyDecodeParam(const struct Subsidy *s, SubsidyDecodeParamType mode, uint parameter_offset = 0);
void DeleteSubsidyWith(SourceType type, SourceID index);
bool CheckSubsidised(CargoID cargo_type, CompanyID company, SourceType src_type, SourceID src, const Station *st);
void RebuildSubsidisedSourceAndDestinationCache();

#endif /* SUBSIDY_FUNC_H */
