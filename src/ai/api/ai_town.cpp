/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_town.cpp Implementation of AITown. */

#include "../../stdafx.h"
#include "ai_town.hpp"
#include "ai_map.hpp"
#include "ai_cargo.hpp"
#include "ai_error.hpp"
#include "../../town.h"
#include "../../strings_func.h"
#include "../../company_func.h"
#include "../../station_base.h"
#include "table/strings.h"

/* static */ int32 AITown::GetTownCount()
{
	return (int32)::Town::GetNumItems();
}

/* static */ bool AITown::IsValidTown(TownID town_id)
{
	return ::Town::IsValidID(town_id);
}

/* static */ char *AITown::GetName(TownID town_id)
{
	if (!IsValidTown(town_id)) return NULL;
	static const int len = 64;
	char *town_name = MallocT<char>(len);

	::SetDParam(0, town_id);
	::GetString(town_name, STR_TOWN_NAME, &town_name[len - 1]);

	return town_name;
}

/* static */ int32 AITown::GetPopulation(TownID town_id)
{
	if (!IsValidTown(town_id)) return -1;
	const Town *t = ::Town::Get(town_id);
	return t->population;
}

/* static */ int32 AITown::GetHouseCount(TownID town_id)
{
	if (!IsValidTown(town_id)) return -1;
	const Town *t = ::Town::Get(town_id);
	return t->num_houses;
}

/* static */ TileIndex AITown::GetLocation(TownID town_id)
{
	if (!IsValidTown(town_id)) return INVALID_TILE;
	const Town *t = ::Town::Get(town_id);
	return t->xy;
}

/* static */ int32 AITown::GetLastMonthProduction(TownID town_id, CargoID cargo_id)
{
	if (!IsValidTown(town_id)) return -1;
	if (!AICargo::IsValidCargo(cargo_id)) return -1;

	const Town *t = ::Town::Get(town_id);

	switch (AICargo::GetTownEffect(cargo_id)) {
		case AICargo::TE_PASSENGERS: return t->max_pass;
		case AICargo::TE_MAIL:       return t->max_mail;
		default: return -1;
	}
}

/* static */ int32 AITown::GetLastMonthTransported(TownID town_id, CargoID cargo_id)
{
	if (!IsValidTown(town_id)) return -1;
	if (!AICargo::IsValidCargo(cargo_id)) return -1;

	const Town *t = ::Town::Get(town_id);

	switch (AICargo::GetTownEffect(cargo_id)) {
		case AICargo::TE_PASSENGERS: return t->act_pass;
		case AICargo::TE_MAIL:       return t->act_mail;
		default: return -1;
	}
}

/* static */ int32 AITown::GetLastMonthTransportedPercentage(TownID town_id, CargoID cargo_id)
{
	if (!IsValidTown(town_id)) return -1;
	if (!AICargo::IsValidCargo(cargo_id)) return -1;

	const Town *t = ::Town::Get(town_id);

	switch (AICargo::GetTownEffect(cargo_id)) {
		case AICargo::TE_PASSENGERS: return ::ToPercent8(t->pct_pass_transported);
		case AICargo::TE_MAIL:       return ::ToPercent8(t->pct_mail_transported);
		default: return -1;
	}
}

/* static */ int32 AITown::GetDistanceManhattanToTile(TownID town_id, TileIndex tile)
{
	return AIMap::DistanceManhattan(tile, GetLocation(town_id));
}

/* static */ int32 AITown::GetDistanceSquareToTile(TownID town_id, TileIndex tile)
{
	return AIMap::DistanceSquare(tile, GetLocation(town_id));
}

/* static */ bool AITown::IsWithinTownInfluence(TownID town_id, TileIndex tile)
{
	if (!IsValidTown(town_id)) return false;

	const Town *t = ::Town::Get(town_id);
	return ((uint32)GetDistanceSquareToTile(town_id, tile) <= t->squared_town_zone_radius[0]);
}

/* static */ bool AITown::HasStatue(TownID town_id)
{
	if (!IsValidTown(town_id)) return false;

	return ::HasBit(::Town::Get(town_id)->statues, _current_company);
}

/* static */ bool AITown::IsCity(TownID town_id)
{
	if (!IsValidTown(town_id)) return false;

	return ::Town::Get(town_id)->larger_town;
}

/* static */ int AITown::GetRoadReworkDuration(TownID town_id)
{
	if (!IsValidTown(town_id)) return -1;

	return ::Town::Get(town_id)->road_build_months;
}

/* static */ AICompany::CompanyID AITown::GetExclusiveRightsCompany(TownID town_id)
{
	if (!IsValidTown(town_id)) return AICompany::COMPANY_INVALID;

	return (AICompany::CompanyID)(int8)::Town::Get(town_id)->exclusivity;
}

/* static */ int32 AITown::GetExclusiveRightsDuration(TownID town_id)
{
	if (!IsValidTown(town_id)) return -1;

	return ::Town::Get(town_id)->exclusive_counter;
}

/* static */ bool AITown::IsActionAvailable(TownID town_id, TownAction town_action)
{
	if (!IsValidTown(town_id)) return false;

	return HasBit(::GetMaskOfTownActions(NULL, _current_company, ::Town::Get(town_id)), town_action);
}

/* static */ bool AITown::PerformTownAction(TownID town_id, TownAction town_action)
{
	EnforcePrecondition(false, IsValidTown(town_id));
	EnforcePrecondition(false, IsActionAvailable(town_id, town_action));

	return AIObject::DoCommand(::Town::Get(town_id)->xy, town_id, town_action, CMD_DO_TOWN_ACTION);
}

/* static */ AITown::TownRating AITown::GetRating(TownID town_id, AICompany::CompanyID company_id)
{
	if (!IsValidTown(town_id)) return TOWN_RATING_INVALID;
	AICompany::CompanyID company = AICompany::ResolveCompanyID(company_id);
	if (company == AICompany::COMPANY_INVALID) return TOWN_RATING_INVALID;

	const Town *t = ::Town::Get(town_id);
	if (!HasBit(t->have_ratings, company)) {
		return TOWN_RATING_NONE;
	} else if (t->ratings[company] <= RATING_APPALLING) {
		return TOWN_RATING_APPALLING;
	} else if (t->ratings[company] <= RATING_VERYPOOR) {
		return TOWN_RATING_VERY_POOR;
	} else if (t->ratings[company] <= RATING_POOR) {
		return TOWN_RATING_POOR;
	} else if (t->ratings[company] <= RATING_MEDIOCRE) {
		return TOWN_RATING_MEDIOCRE;
	} else if (t->ratings[company] <= RATING_GOOD) {
		return TOWN_RATING_GOOD;
	} else if (t->ratings[company] <= RATING_VERYGOOD) {
		return TOWN_RATING_VERY_GOOD;
	} else if (t->ratings[company] <= RATING_EXCELLENT) {
		return TOWN_RATING_EXCELLENT;
	} else {
		return TOWN_RATING_OUTSTANDING;
	}
}

/* static */ int AITown::GetAllowedNoise(TownID town_id)
{
	if (!IsValidTown(town_id)) return -1;

	const Town *t = ::Town::Get(town_id);
	if (_settings_game.economy.station_noise_level) {
		return t->MaxTownNoise() - t->noise_reached;
	}

	int num = 0;
	const Station *st;
	FOR_ALL_STATIONS(st) {
		if (st->town == t && (st->facilities & FACIL_AIRPORT) && st->airport.type != AT_OILRIG) num++;
	}
	return max(0, 2 - num);
}

/* static */ AITown::RoadLayout AITown::GetRoadLayout(TownID town_id)
{
	if (!IsValidTown(town_id)) return ROAD_LAYOUT_INVALID;

	return (AITown::RoadLayout)((TownLayout)::Town::Get(town_id)->layout);
}
