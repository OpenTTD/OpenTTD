/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file autoreplace_func.h Functions related to autoreplacing. */

#ifndef AUTOREPLACE_FUNC_H
#define AUTOREPLACE_FUNC_H

#include "command_type.h"
#include "company_base.h"
#include "engine_type.h"
#include "group_type.h"

void RemoveAllEngineReplacement(EngineRenewList *erl);
EngineID EngineReplacement(EngineRenewList erl, EngineID engine, GroupID group);
CommandCost AddEngineReplacement(EngineRenewList *erl, EngineID old_engine, EngineID new_engine, GroupID group, DoCommandFlag flags);
CommandCost RemoveEngineReplacement(EngineRenewList *erl, EngineID engine, GroupID group, DoCommandFlag flags);

/**
 * Remove all engine replacement settings for the given company.
 * @param c the company.
 */
static inline void RemoveAllEngineReplacementForCompany(Company *c)
{
	RemoveAllEngineReplacement(&c->engine_renew_list);
}

/**
 * Retrieve the engine replacement for the given company and original engine type.
 * @param c company.
 * @param engine Engine type.
 * @param group The group related to this replacement.
 * @return The engine type to replace with, or INVALID_ENGINE if no
 * replacement is in the list.
 */
static inline EngineID EngineReplacementForCompany(const Company *c, EngineID engine, GroupID group)
{
	return EngineReplacement(c->engine_renew_list, engine, group);
}

/**
 * Check if a company has a replacement set up for the given engine.
 * @param c Company.
 * @param engine Engine type to be replaced.
 * @param group The group related to this replacement.
 * @return true if a replacement was set up, false otherwise.
 */
static inline bool EngineHasReplacementForCompany(const Company *c, EngineID engine, GroupID group)
{
	return EngineReplacementForCompany(c, engine, group) != INVALID_ENGINE;
}

/**
 * Add an engine replacement for the company.
 * @param c Company.
 * @param old_engine The original engine type.
 * @param new_engine The replacement engine type.
 * @param group The group related to this replacement.
 * @param flags The calling command flags.
 * @return 0 on success, CMD_ERROR on failure.
 */
static inline CommandCost AddEngineReplacementForCompany(Company *c, EngineID old_engine, EngineID new_engine, GroupID group, DoCommandFlag flags)
{
	return AddEngineReplacement(&c->engine_renew_list, old_engine, new_engine, group, flags);
}

/**
 * Remove an engine replacement for the company.
 * @param c Company.
 * @param engine The original engine type.
 * @param group The group related to this replacement.
 * @param flags The calling command flags.
 * @return 0 on success, CMD_ERROR on failure.
 */
static inline CommandCost RemoveEngineReplacementForCompany(Company *c, EngineID engine, GroupID group, DoCommandFlag flags)
{
	return RemoveEngineReplacement(&c->engine_renew_list, engine, group, flags);
}

bool CheckAutoreplaceValidity(EngineID from, EngineID to, CompanyID company);

#endif /* AUTOREPLACE_FUNC_H */
