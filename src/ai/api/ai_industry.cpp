/* $Id$ */

/** @file ai_industry.cpp Implementation of AIIndustry. */

#include "ai_industry.hpp"
#include "ai_cargo.hpp"
#include "ai_map.hpp"
#include "../../openttd.h"
#include "../../tile_type.h"
#include "../../industry.h"
#include "../../strings_func.h"
#include "../../station_func.h"
#include "table/strings.h"

/* static */ IndustryID AIIndustry::GetMaxIndustryID()
{
	return ::GetMaxIndustryIndex();
}

/* static */ int32 AIIndustry::GetIndustryCount()
{
	return ::GetNumIndustries();
}

/* static */ bool AIIndustry::IsValidIndustry(IndustryID industry_id)
{
	return ::IsValidIndustryID(industry_id);
}

/* static */ const char *AIIndustry::GetName(IndustryID industry_id)
{
	if (!IsValidIndustry(industry_id)) return NULL;
	static const int len = 64;
	char *industry_name = MallocT<char>(len);

	::SetDParam(0, industry_id);
	::GetString(industry_name, STR_INDUSTRY, &industry_name[len - 1]);

	return industry_name;
}

/* static */ int32 AIIndustry::GetProduction(IndustryID industry_id, CargoID cargo_id)
{
	if (!IsValidIndustry(industry_id)) return -1;
	if (!AICargo::IsValidCargo(cargo_id)) return -1;

	const Industry *i = ::GetIndustry(industry_id);
	const IndustrySpec *indsp = ::GetIndustrySpec(i->type);

	for (byte j = 0; j < lengthof(indsp->produced_cargo); j++)
		if (indsp->produced_cargo[j] == cargo_id) return i->production_rate[j] * 8;

	return -1;
}

/* static */ bool AIIndustry::IsCargoAccepted(IndustryID industry_id, CargoID cargo_id)
{
	if (!IsValidIndustry(industry_id)) return false;
	if (!AICargo::IsValidCargo(cargo_id)) return false;

	const Industry *i = ::GetIndustry(industry_id);
	const IndustrySpec *indsp = ::GetIndustrySpec(i->type);

	for (byte j = 0; j < lengthof(indsp->accepts_cargo); j++)
		if (indsp->accepts_cargo[j] == cargo_id) return true;

	return false;
}

/* static */ int32 AIIndustry::GetStockpiledCargo(IndustryID industry_id, CargoID cargo_id)
{
	if (!IsValidIndustry(industry_id)) return -1;
	if (!AICargo::IsValidCargo(cargo_id)) return -1;

	Industry *ind = ::GetIndustry(industry_id);
	for (uint i = 0; i < lengthof(ind->accepts_cargo); i++) {
		CargoID cid = ind->accepts_cargo[i];
		if (cid == cargo_id) {
			return ind->incoming_cargo_waiting[i];
		}
	}

	return -1;
}

/* static */ int32 AIIndustry::GetLastMonthProduction(IndustryID industry_id, CargoID cargo_id)
{
	if (!IsValidIndustry(industry_id)) return -1;
	if (!AICargo::IsValidCargo(cargo_id)) return -1;

	const Industry *i = ::GetIndustry(industry_id);
	const IndustrySpec *indsp = ::GetIndustrySpec(i->type);

	for (byte j = 0; j < lengthof(indsp->produced_cargo); j++)
		if (indsp->produced_cargo[j] == cargo_id) return i->last_month_production[j];

	return -1;
}

/* static */ int32 AIIndustry::GetLastMonthTransported(IndustryID industry_id, CargoID cargo_id)
{
	if (!IsValidIndustry(industry_id)) return -1;
	if (!AICargo::IsValidCargo(cargo_id)) return -1;

	const Industry *i = ::GetIndustry(industry_id);
	const IndustrySpec *indsp = ::GetIndustrySpec(i->type);

	for (byte j = 0; j < lengthof(indsp->produced_cargo); j++)
		if (indsp->produced_cargo[j] == cargo_id) return i->last_month_transported[j];

	return -1;
}

/* static */ TileIndex AIIndustry::GetLocation(IndustryID industry_id)
{
	if (!IsValidIndustry(industry_id)) return INVALID_TILE;

	return ::GetIndustry(industry_id)->xy;
}

/* static */ int32 AIIndustry::GetAmountOfStationsAround(IndustryID industry_id)
{
	if (!IsValidIndustry(industry_id)) return -1;

	Industry *ind = ::GetIndustry(industry_id);
	return (int32)::FindStationsAroundTiles(ind->xy, ind->width, ind->height).Length();
}

/* static */ int32 AIIndustry::GetDistanceManhattanToTile(IndustryID industry_id, TileIndex tile)
{
	if (!IsValidIndustry(industry_id)) return -1;

	return AIMap::DistanceManhattan(tile, GetLocation(industry_id));
}

/* static */ int32 AIIndustry::GetDistanceSquareToTile(IndustryID industry_id, TileIndex tile)
{
	if (!IsValidIndustry(industry_id)) return -1;

	return AIMap::DistanceSquare(tile, GetLocation(industry_id));
}

/* static */ bool AIIndustry::IsBuiltOnWater(IndustryID industry_id)
{
	if (!IsValidIndustry(industry_id)) return false;

	return (::GetIndustrySpec(::GetIndustry(industry_id)->type)->behaviour & INDUSTRYBEH_BUILT_ONWATER) != 0;
}

/* static */ bool AIIndustry::HasHeliportAndDock(IndustryID industry_id)
{
	if (!IsValidIndustry(industry_id)) return false;

	return (::GetIndustrySpec(::GetIndustry(industry_id)->type)->behaviour & INDUSTRYBEH_AI_AIRSHIP_ROUTES) != 0;
}

/* static */ TileIndex AIIndustry::GetHeliportAndDockLocation(IndustryID industry_id)
{
	if (!IsValidIndustry(industry_id)) return INVALID_TILE;
	if (!HasHeliportAndDock(industry_id)) return INVALID_TILE;

	return ::GetIndustry(industry_id)->xy;
}

/* static */ IndustryType AIIndustry::GetIndustryType(IndustryID industry_id)
{
	if (!IsValidIndustry(industry_id)) return INVALID_INDUSTRYTYPE;

	return ::GetIndustry(industry_id)->type;
}
