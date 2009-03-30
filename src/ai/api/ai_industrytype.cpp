/* $Id$ */

/** @file ai_industrytype.cpp Implementation of AIIndustryType. */

#include "ai_industrytype.hpp"
#include "ai_map.hpp"
#include "../../command_type.h"
#include "../../settings_type.h"
#include "../../strings_func.h"
#include "../../industry.h"

/* static */ bool AIIndustryType::IsValidIndustryType(IndustryType industry_type)
{
	if (industry_type >= NUM_INDUSTRYTYPES) return false;

	return ::GetIndustrySpec(industry_type)->enabled;
}

/* static */ bool AIIndustryType::IsRawIndustry(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return false;

	return ::GetIndustrySpec(industry_type)->IsRawIndustry();
}

/* static */ bool AIIndustryType::ProductionCanIncrease(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return false;

	if (_settings_game.game_creation.landscape != LT_TEMPERATE) return true;
	return (::GetIndustrySpec(industry_type)->behaviour & INDUSTRYBEH_DONT_INCR_PROD) == 0;
}

/* static */ Money AIIndustryType::GetConstructionCost(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return false;

	return ::GetIndustrySpec(industry_type)->GetConstructionCost();
}

/* static */ char *AIIndustryType::GetName(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return NULL;
	static const int len = 64;
	char *industrytype_name = MallocT<char>(len);

	::GetString(industrytype_name, ::GetIndustrySpec(industry_type)->name, &industrytype_name[len - 1]);

	return industrytype_name;
}

/* static */ AIList *AIIndustryType::GetProducedCargo(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return NULL;

	const IndustrySpec *ins = ::GetIndustrySpec(industry_type);

	AIList *list = new AIList();
	for (size_t i = 0; i < lengthof(ins->produced_cargo); i++) {
		if (ins->produced_cargo[i] != CT_INVALID) list->AddItem(ins->produced_cargo[i], 0);
	}

	return list;
}

/* static */ AIList *AIIndustryType::GetAcceptedCargo(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return NULL;

	const IndustrySpec *ins = ::GetIndustrySpec(industry_type);

	AIList *list = new AIList();
	for (size_t i = 0; i < lengthof(ins->accepts_cargo); i++) {
		if (ins->accepts_cargo[i] != CT_INVALID) list->AddItem(ins->accepts_cargo[i], 0);
	}

	return list;
}

/* static */ bool AIIndustryType::CanBuildIndustry(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return false;
	if (!::GetIndustrySpec(industry_type)->IsRawIndustry()) return true;

	/* raw_industry_construction == 1 means "Build as other industries" */
	return _settings_game.construction.raw_industry_construction == 1;
}

/* static */ bool AIIndustryType::CanProspectIndustry(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return false;
	if (!::GetIndustrySpec(industry_type)->IsRawIndustry()) return false;

	/* raw_industry_construction == 2 means "prospect" */
	return _settings_game.construction.raw_industry_construction == 2;
}

/* static */ bool AIIndustryType::BuildIndustry(IndustryType industry_type, TileIndex tile)
{
	EnforcePrecondition(false, CanBuildIndustry(industry_type));
	EnforcePrecondition(false, AIMap::IsValidTile(tile));

	uint32 seed = ::InteractiveRandom();
	return AIObject::DoCommand(tile, (::InteractiveRandomRange(::GetIndustrySpec(industry_type)->num_table) << 16) | industry_type, seed, CMD_BUILD_INDUSTRY);
}

/* static */ bool AIIndustryType::ProspectIndustry(IndustryType industry_type)
{
	EnforcePrecondition(false, CanProspectIndustry(industry_type));

	uint32 seed = ::InteractiveRandom();
	return AIObject::DoCommand(0, industry_type, seed, CMD_BUILD_INDUSTRY);
}

/* static */ bool AIIndustryType::IsBuiltOnWater(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return false;

	return (::GetIndustrySpec(industry_type)->behaviour & INDUSTRYBEH_BUILT_ONWATER) != 0;
}

/* static */ bool AIIndustryType::HasHeliport(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return false;

	return (::GetIndustrySpec(industry_type)->behaviour & INDUSTRYBEH_AI_AIRSHIP_ROUTES) != 0;
}

/* static */ bool AIIndustryType::HasDock(IndustryType industry_type)
{
	if (!IsValidIndustryType(industry_type)) return false;

	return (::GetIndustrySpec(industry_type)->behaviour & INDUSTRYBEH_AI_AIRSHIP_ROUTES) != 0;
}
