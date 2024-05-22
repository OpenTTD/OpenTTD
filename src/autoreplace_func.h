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

void RemoveAllEngineReplacement(EngineRenewList *erl);
EngineID EngineReplacement(EngineRenewList erl, EngineID engine, GroupID group, bool *replace_when_old = nullptr);
CommandCost AddEngineReplacement(EngineRenewList *erl, EngineID old_engine, EngineID new_engine, GroupID group, bool replace_when_old, DoCommandFlag flags);
CommandCost RemoveEngineReplacement(EngineRenewList *erl, EngineID engine, GroupID group, DoCommandFlag flags);

/**
 * Remove all engine replacement settings for the given company.
 * @param c the company.
 */
inline void RemoveAllEngineReplacementForCompany(Company *c)
{
	RemoveAllEngineReplacement(&c->engine_renew_list);
}

/**
 * Retrieve the engine replacement for the given company and original engine type.
 * @param c company.
 * @param engine Engine type.
 * @param group The group related to this replacement.
 * @param[out] replace_when_old Set to true if the replacement should be done when old.
 * @return The engine type to replace with, or INVALID_ENGINE if no
 * replacement is in the list.
 */
inline EngineID EngineReplacementForCompany(const Company *c, EngineID engine, GroupID group, bool *replace_when_old = nullptr)
{
	return EngineReplacement(c->engine_renew_list, engine, group, replace_when_old);
}

/**
 * Check if a company has a replacement set up for the given engine.
 * @param c Company.
 * @param engine Engine type to be replaced.
 * @param group The group related to this replacement.
 * @return true if a replacement was set up, false otherwise.
 */
inline bool EngineHasReplacementForCompany(const Company *c, EngineID engine, GroupID group)
{
	return EngineReplacementForCompany(c, engine, group) != INVALID_ENGINE;
}

/**
 * Check if a company has a replacement set up for the given engine when it gets old.
 * @param c Company.
 * @param engine Engine type to be replaced.
 * @param group The group related to this replacement.
 * @return True if a replacement when old was set up, false otherwise.
 */
inline bool EngineHasReplacementWhenOldForCompany(const Company *c, EngineID engine, GroupID group)
{
	bool replace_when_old;
	EngineReplacement(c->engine_renew_list, engine, group, &replace_when_old);
	return replace_when_old;
}

/**
 * Add an engine replacement for the company.
 * @param c Company.
 * @param old_engine The original engine type.
 * @param new_engine The replacement engine type.
 * @param group The group related to this replacement.
 * @param replace_when_old Replace when old or always?
 * @param flags The calling command flags.
 * @return 0 on success, CMD_ERROR on failure.
 */
inline CommandCost AddEngineReplacementForCompany(Company *c, EngineID old_engine, EngineID new_engine, GroupID group, bool replace_when_old, DoCommandFlag flags)
{
	return AddEngineReplacement(&c->engine_renew_list, old_engine, new_engine, group, replace_when_old, flags);
}

/**
 * Remove an engine replacement for the company.
 * @param c Company.
 * @param engine The original engine type.
 * @param group The group related to this replacement.
 * @param flags The calling command flags.
 * @return 0 on success, CMD_ERROR on failure.
 */
inline CommandCost RemoveEngineReplacementForCompany(Company *c, EngineID engine, GroupID group, DoCommandFlag flags)
{
	return RemoveEngineReplacement(&c->engine_renew_list, engine, group, flags);
}

bool CheckAutoreplaceValidity(EngineID from, EngineID to, CompanyID company);

#endif /* AUTOREPLACE_FUNC_H */
