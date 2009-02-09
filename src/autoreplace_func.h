/* $Id$ */

/** @file autoreplace_func.h Functions related to autoreplacing. */

#ifndef AUTOREPLACE_FUNC_H
#define AUTOREPLACE_FUNC_H

#include "autoreplace_type.h"
#include "company_base.h"

/**
 * Remove all engine replacement settings for the company.
 * @param  erl The renewlist for a given company.
 * @return The new renewlist for the company.
 */
void RemoveAllEngineReplacement(EngineRenewList *erl);

/**
 * Retrieve the engine replacement in a given renewlist for an original engine type.
 * @param  erl The renewlist to search in.
 * @param  engine Engine type to be replaced.
 * @return The engine type to replace with, or INVALID_ENGINE if no
 * replacement is in the list.
 */
EngineID EngineReplacement(EngineRenewList erl, EngineID engine, GroupID group);

/**
 * Add an engine replacement to the given renewlist.
 * @param erl The renewlist to add to.
 * @param old_engine The original engine type.
 * @param new_engine The replacement engine type.
 * @param flags The calling command flags.
 * @return 0 on success, CMD_ERROR on failure.
 */
CommandCost AddEngineReplacement(EngineRenewList *erl, EngineID old_engine, EngineID new_engine, GroupID group, DoCommandFlag flags);

/**
 * Remove an engine replacement from a given renewlist.
 * @param erl The renewlist from which to remove the replacement
 * @param engine The original engine type.
 * @param flags The calling command flags.
 * @return 0 on success, CMD_ERROR on failure.
 */
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
 * @param  engine Engine type to be replaced.
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
 * @param flags The calling command flags.
 * @return 0 on success, CMD_ERROR on failure.
 */
static inline CommandCost RemoveEngineReplacementForCompany(Company *c, EngineID engine, GroupID group, DoCommandFlag flags)
{
	return RemoveEngineReplacement(&c->engine_renew_list, engine, group, flags);
}

bool CheckAutoreplaceValidity(EngineID from, EngineID to, CompanyID company);

#endif /* AUTOREPLACE_FUNC_H */
