/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file autoreplace_func.h Functions related to autoreplacing. */

#ifndef AUTOREPLACE_FUNC_H
#define AUTOREPLACE_FUNC_H

#include "autoreplace_type.h"
#include "command_type.h"

const EngineRenew *GetEngineRenew(const EngineRenewList &erl, EngineID engine);
const EngineRenew *GetEngineRenew(CompanyID company, GroupID group, EngineID engine, bool search);
CommandCost AddEngineReplacement(DoCommandFlags flags, CompanyID company, GroupID group, EngineID engine, EngineID new_engine, bool replace_when_old);
CommandCost RemoveEngineReplacement(DoCommandFlags flags, CompanyID company, GroupID group, EngineID engine);

void RemoveAllEngineReplacementForCompany(CompanyID company);

/**
 * Check if a company has a replacement set up for the given engine.
 * @param company The company.
 * @param group The group related to this replacement.
 * @param engine Engine type to be replaced.
 * @param search Whether to search groups and company for replacement.
 * @return true if a replacement was set up, false otherwise.
 */
inline bool EngineHasReplacement(CompanyID company, GroupID group, EngineID engine, bool search)
{
	const EngineRenew *er = GetEngineRenew(company, group, engine, search);
	return er != nullptr && engine != er->to;
}

/**
 * Check if a company has a replacement set up for the given engine when it gets old.
 * @param company The company.
 * @param group The group related to this replacement.
 * @param engine Engine type to be replaced.
 * @param search Whether to search groups and company for replacement.
 * @return True if a replacement when old was set up, false otherwise.
 */
inline bool EngineHasReplacementWhenOld(CompanyID company, GroupID group, EngineID engine, bool search)
{
	const EngineRenew *er = GetEngineRenew(company, group, engine, search);
	return er != nullptr && er->replace_when_old;
}

bool CheckAutoreplaceValidity(EngineID from, EngineID to, CompanyID company);

#endif /* AUTOREPLACE_FUNC_H */
