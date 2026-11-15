/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file autoreplace.cpp Management of replacement lists. */

#include "stdafx.h"

#include "autoreplace_func.h"
#include "autoreplace_type.h"
#include "command_func.h"
#include "company_base.h"
#include "group.h"

#include "safeguards.h"

/**
 * Search within a single engine renew list for an engine to renew.
 * @param erl The engine renew list.
 * @param engine The engine to search for.
 * @return Engine Renew data if present.
 */
const EngineRenew *GetEngineRenew(const EngineRenewList &erl, EngineID engine)
{
	/* Linear scan until it reaches or passes engine, instead of binary search (lower_bound) */
	auto it = std::ranges::find_if(erl, [engine](const auto &er) { return er.from >= engine; });
	if (it == erl.end() || it->from != engine) return nullptr;
	return &*it;
}

/**
 * Retrieves the EngineRenew that specifies the replacement of the given engine type from the given renewlist
 * @param company The company.
 * @param group The group to look for.
 * @param engine The engine to look for.
 * @param search Whether to search groups and company for replacement.
 * @return The engine renew configuration for the given engine and group.
 */
const EngineRenew *GetEngineRenew(CompanyID company, GroupID group, EngineID engine, bool search)
{
	for (const Group *g = Group::GetIfValid(group); g != nullptr; g = Group::GetIfValid(g->parent)) {
		if (g->flags.Test(GroupFlag::ReplaceProtection)) return nullptr;
		const EngineRenew *er = GetEngineRenew(g->engine_renew_list, engine);
		if (er != nullptr || !search) return er;
	}

	const Company *c = Company::Get(company);
	if (group == DEFAULT_GROUP) {
		const EngineRenew *er = GetEngineRenew(c->ungrouped_engine_renew_list, engine);
		if (er != nullptr || !search) return er;
	}

	return GetEngineRenew(c->all_engine_renew_list, engine);
}

/**
 * Remove all engine replacements for a company.
 * @param company The company.
 */
void RemoveAllEngineReplacementForCompany(CompanyID company)
{
	Company *c = Company::GetIfValid(company);
	if (c == nullptr) return;

	c->all_engine_renew_list.clear();
	c->ungrouped_engine_renew_list.clear();

	for (Group *g : Group::Iterate()) {
		if (g->owner != company) continue;
		g->engine_renew_list.clear();
	}
}

/**
 * Get the Engine Renew List for a company/group.
 * @param company The company.
 * @param group The group.
 * @return The EngineRenewList for the company/group.
 */
EngineRenewList &GetEngineRenewList(CompanyID company, GroupID group)
{
	if (group == ALL_GROUP) return Company::Get(company)->all_engine_renew_list;
	if (group == DEFAULT_GROUP) return Company::Get(company)->ungrouped_engine_renew_list;

	Group *g = Group::GetIfValid(group);
	assert(g != nullptr);
	return g->engine_renew_list;
}

/**
 * Add an engine replacement to the given renewlist.
 * @param flags The calling command flags.
 * @param company The company.
 * @param group The group related to this replacement.
 * @param engine The original engine type.
 * @param new_engine The replacement engine type.
 * @param replace_when_old Replace when old or always?
 * @return 0 on success, CMD_ERROR on failure.
 */
CommandCost AddEngineReplacement(DoCommandFlags flags, CompanyID company, GroupID group, EngineID engine, EngineID new_engine, bool replace_when_old)
{
	if (flags.Test(DoCommandFlag::Execute)) {
		/* Like-for-like replacements are only replaced when old. */
		if (engine == new_engine) replace_when_old = true;

		/* Check if the old vehicle is already in the list. */
		EngineRenewList &erl = GetEngineRenewList(company, group);
		auto it = std::ranges::lower_bound(erl, engine, std::less{}, &EngineRenew::from);
		if (it == erl.end() || it->from != engine) {
			/* Not present, create new entry. */
			erl.emplace(it, engine, new_engine, replace_when_old);
		} else {
			/* Update existing entry. */
			it->to = new_engine;
			it->replace_when_old = replace_when_old;
		}
	}

	return CommandCost();
}

/**
 * Remove an engine replacement from a given renewlist.
 * @param flags The calling command flags.
 * @param company The company.
 * @param group The group related to this replacement.
 * @param engine The original engine type.
 * @return 0 on success, CMD_ERROR on failure.
 */
CommandCost RemoveEngineReplacement(DoCommandFlags flags, CompanyID company, GroupID group, EngineID engine)
{
	EngineRenewList &erl = GetEngineRenewList(company, group);
	auto it = std::ranges::lower_bound(erl, engine, std::less{}, &EngineRenew::from);
	if (it == erl.end() || it->from != engine) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		erl.erase(it);
	}

	return CommandCost();
}
