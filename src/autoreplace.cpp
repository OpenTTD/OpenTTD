/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file autoreplace.cpp Management of replacement lists. */

#include "stdafx.h"
#include "command_func.h"
#include "group.h"
#include "autoreplace_base.h"
#include "core/pool_func.hpp"

EngineRenewPool _enginerenew_pool("EngineRenew");
INSTANTIATE_POOL_METHODS(EngineRenew)

/**
 * Retrieves the EngineRenew that specifies the replacement of the given
 * engine type from the given renewlist
 */
static EngineRenew *GetEngineReplacement(EngineRenewList erl, EngineID engine, GroupID group)
{
	EngineRenew *er = (EngineRenew *)erl;

	while (er) {
		if (er->from == engine && er->group_id == group) return er;
		er = er->next;
	}
	return NULL;
}

/**
 * Remove all engine replacement settings for the company.
 * @param  erl The renewlist for a given company.
 * @return The new renewlist for the company.
 */
void RemoveAllEngineReplacement(EngineRenewList *erl)
{
	EngineRenew *er = (EngineRenew *)(*erl);
	EngineRenew *next;

	while (er != NULL) {
		next = er->next;
		delete er;
		er = next;
	}
	*erl = NULL; // Empty list
}

/**
 * Retrieve the engine replacement in a given renewlist for an original engine type.
 * @param erl The renewlist to search in.
 * @param engine Engine type to be replaced.
 * @param group The group related to this replacement.
 * @return The engine type to replace with, or INVALID_ENGINE if no
 * replacement is in the list.
 */
EngineID EngineReplacement(EngineRenewList erl, EngineID engine, GroupID group)
{
	const EngineRenew *er = GetEngineReplacement(erl, engine, group);
	if (er == NULL && (group == DEFAULT_GROUP || (Group::IsValidID(group) && !Group::Get(group)->replace_protection))) {
		/* We didn't find anything useful in the vehicle's own group so we will try ALL_GROUP */
		er = GetEngineReplacement(erl, engine, ALL_GROUP);
	}
	return er == NULL ? INVALID_ENGINE : er->to;
}

/**
 * Add an engine replacement to the given renewlist.
 * @param erl The renewlist to add to.
 * @param old_engine The original engine type.
 * @param new_engine The replacement engine type.
 * @param group The group related to this replacement.
 * @param flags The calling command flags.
 * @return 0 on success, CMD_ERROR on failure.
 */
CommandCost AddEngineReplacement(EngineRenewList *erl, EngineID old_engine, EngineID new_engine, GroupID group, DoCommandFlag flags)
{
	/* Check if the old vehicle is already in the list */
	EngineRenew *er = GetEngineReplacement(*erl, old_engine, group);
	if (er != NULL) {
		if (flags & DC_EXEC) er->to = new_engine;
		return CommandCost();
	}

	if (!EngineRenew::CanAllocateItem()) return CMD_ERROR;

	if (flags & DC_EXEC) {
		er = new EngineRenew(old_engine, new_engine);
		er->group_id = group;

		/* Insert before the first element */
		er->next = (EngineRenew *)(*erl);
		*erl = (EngineRenewList)er;
	}

	return CommandCost();
}

/**
 * Remove an engine replacement from a given renewlist.
 * @param erl The renewlist from which to remove the replacement
 * @param engine The original engine type.
 * @param group The group related to this replacement.
 * @param flags The calling command flags.
 * @return 0 on success, CMD_ERROR on failure.
 */
CommandCost RemoveEngineReplacement(EngineRenewList *erl, EngineID engine, GroupID group, DoCommandFlag flags)
{
	EngineRenew *er = (EngineRenew *)(*erl);
	EngineRenew *prev = NULL;

	while (er) {
		if (er->from == engine && er->group_id == group) {
			if (flags & DC_EXEC) {
				if (prev == NULL) { // First element
					/* The second becomes the new first element */
					*erl = (EngineRenewList)er->next;
				} else {
					/* Cut this element out */
					prev->next = er->next;
				}
				delete er;
			}
			return CommandCost();
		}
		prev = er;
		er = er->next;
	}

	return CMD_ERROR;
}

void InitializeEngineRenews()
{
	_enginerenew_pool.CleanPool();
}
