/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_industrytype.cpp Implementation of ScriptIndustryType. */

#include "../../stdafx.h"
#include "script_industrytype.hpp"
#include "script_base.hpp"
#include "script_map.hpp"
#include "script_error.hpp"
#include "../../strings_func.h"
#include "../../industry.h"
#include "../../newgrf_industries.h"
#include "../../core/random_func.hpp"
#include "../../industry_cmd.h"

#include "../../safeguards.h"

/* static */ bool ScriptIndustryType::IsValidIndustryType(IndustryType industry_type)
{
	if (industry_type >= NUM_INDUSTRYTYPES) return false;

	return ::GetIndustrySpec(industry_type)->enabled;
}

/* static */ bool ScriptIndustryType::IsRawIndustry(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return false;

	return ::GetIndustrySpec(industry_type)->IsRawIndustry();
}

/* static */ bool ScriptIndustryType::IsProcessingIndustry(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return false;

	return ::GetIndustrySpec(industry_type)->IsProcessingIndustry();
}

/* static */ bool ScriptIndustryType::ProductionCanIncrease(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return false;

	if (_settings_game.game_creation.landscape != LT_TEMPERATE) return true;
	return (::GetIndustrySpec(industry_type)->behaviour & INDUSTRYBEH_DONT_INCR_PROD) == 0;
}

/* static */ Money ScriptIndustryType::GetConstructionCost(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return -1;
	if (::GetIndustrySpec(industry_type)->IsRawIndustry() && _settings_game.construction.raw_industry_construction == 0) return -1;

	return ::GetIndustrySpec(industry_type)->GetConstructionCost();
}

/* static */ std::optional<std::string> ScriptIndustryType::GetName(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return std::nullopt;

	return GetString(::GetIndustrySpec(industry_type)->name);
}

/* static */ ScriptList *ScriptIndustryType::GetProducedCargo(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return nullptr;

	const IndustrySpec *ins = ::GetIndustrySpec(industry_type);

	ScriptList *list = new ScriptList();
	for (const CargoID &c : ins->produced_cargo) {
		if (::IsValidCargoID(c)) list->AddItem(c);
	}

	return list;
}

/* static */ ScriptList *ScriptIndustryType::GetAcceptedCargo(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return nullptr;

	const IndustrySpec *ins = ::GetIndustrySpec(industry_type);

	ScriptList *list = new ScriptList();
	for (const CargoID &c : ins->accepts_cargo) {
		if (::IsValidCargoID(c)) list->AddItem(c);
	}

	return list;
}

/* static */ bool ScriptIndustryType::CanBuildIndustry(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return false;

	const bool deity = ScriptCompanyMode::IsDeity();
	if (::GetIndustryProbabilityCallback(industry_type, deity ? IACT_RANDOMCREATION : IACT_USERCREATION, 1) == 0) return false;
	if (deity) return true;
	if (!::GetIndustrySpec(industry_type)->IsRawIndustry()) return true;

	/* raw_industry_construction == 1 means "Build as other industries" */
	return _settings_game.construction.raw_industry_construction == 1;
}

/* static */ bool ScriptIndustryType::CanProspectIndustry(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return false;

	const bool deity = ScriptCompanyMode::IsDeity();
	if (!deity && !::GetIndustrySpec(industry_type)->IsRawIndustry()) return false;
	if (::GetIndustryProbabilityCallback(industry_type, deity ? IACT_RANDOMCREATION : IACT_USERCREATION, 1) == 0) return false;

	/* raw_industry_construction == 2 means "prospect" */
	return deity || _settings_game.construction.raw_industry_construction == 2;
}

/* static */ bool ScriptIndustryType::BuildIndustry(IndustryType industry_type, TileIndex tile)
{
	EnforceDeityOrCompanyModeValid(false);
	EnforcePrecondition(false, CanBuildIndustry(industry_type));
	EnforcePrecondition(false, ScriptMap::IsValidTile(tile));

	uint32_t seed = ScriptBase::Rand();
	uint32_t layout_index = ScriptBase::RandRange((uint32_t)::GetIndustrySpec(industry_type)->layouts.size());
	return ScriptObject::Command<CMD_BUILD_INDUSTRY>::Do(tile, industry_type, layout_index, true, seed);
}

/* static */ bool ScriptIndustryType::ProspectIndustry(IndustryType industry_type)
{
	EnforceDeityOrCompanyModeValid(false);
	EnforcePrecondition(false, CanProspectIndustry(industry_type));

	uint32_t seed = ScriptBase::Rand();
	return ScriptObject::Command<CMD_BUILD_INDUSTRY>::Do(0, industry_type, 0, false, seed);
}

/* static */ bool ScriptIndustryType::IsBuiltOnWater(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return false;

	return (::GetIndustrySpec(industry_type)->behaviour & INDUSTRYBEH_BUILT_ONWATER) != 0;
}

/* static */ bool ScriptIndustryType::HasHeliport(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return false;

	return (::GetIndustrySpec(industry_type)->behaviour & INDUSTRYBEH_AI_AIRSHIP_ROUTES) != 0;
}

/* static */ bool ScriptIndustryType::HasDock(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return false;

	return (::GetIndustrySpec(industry_type)->behaviour & INDUSTRYBEH_AI_AIRSHIP_ROUTES) != 0;
}

/* static */ IndustryType ScriptIndustryType::ResolveNewGRFID(SQInteger grfid, SQInteger grf_local_id)
{
	EnforcePrecondition(INVALID_INDUSTRYTYPE, IsInsideBS(grf_local_id, 0x00, NUM_INDUSTRYTYPES_PER_GRF));

	grfid = BSWAP32(GB(grfid, 0, 32)); // Match people's expectations.
	return _industry_mngr.GetID(grf_local_id, grfid);
}
