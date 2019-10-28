/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_industry.cpp Implementation of ScriptIndustry. */

#include "../../stdafx.h"
#include "script_industry.hpp"
#include "script_cargo.hpp"
#include "script_map.hpp"
#include "../../industry.h"
#include "../../strings_func.h"
#include "../../station_base.h"
#include "../../newgrf_industries.h"
#include "table/strings.h"

#include "../../safeguards.h"

/* static */ int32 ScriptIndustry::GetIndustryCount()
{
	return (int32)::Industry::GetNumItems();
}

/* static */ bool ScriptIndustry::IsValidIndustry(IndustryID industry_id)
{
	return ::Industry::IsValidID(industry_id);
}

/* static */ IndustryID ScriptIndustry::GetIndustryID(TileIndex tile)
{
	if (!::IsValidTile(tile) || !::IsTileType(tile, MP_INDUSTRY)) return INVALID_INDUSTRY;
	return ::GetIndustryIndex(tile);
}

/* static */ char *ScriptIndustry::GetName(IndustryID industry_id)
{
	if (!IsValidIndustry(industry_id)) return nullptr;

	::SetDParam(0, industry_id);
	return GetString(STR_INDUSTRY_NAME);
}

/* static */ ScriptIndustry::CargoAcceptState ScriptIndustry::IsCargoAccepted(IndustryID industry_id, CargoID cargo_id)
{
	if (!IsValidIndustry(industry_id)) return CAS_NOT_ACCEPTED;
	if (!ScriptCargo::IsValidCargo(cargo_id)) return CAS_NOT_ACCEPTED;

	Industry *i = ::Industry::Get(industry_id);

	for (byte j = 0; j < lengthof(i->accepts_cargo); j++) {
		if (i->accepts_cargo[j] == cargo_id) {
			if (IndustryTemporarilyRefusesCargo(i, cargo_id)) return CAS_TEMP_REFUSED;
			return CAS_ACCEPTED;
		}
	}

	return CAS_NOT_ACCEPTED;
}

/* static */ int32 ScriptIndustry::GetStockpiledCargo(IndustryID industry_id, CargoID cargo_id)
{
	if (!IsValidIndustry(industry_id)) return -1;
	if (!ScriptCargo::IsValidCargo(cargo_id)) return -1;

	Industry *ind = ::Industry::Get(industry_id);
	for (uint i = 0; i < lengthof(ind->accepts_cargo); i++) {
		CargoID cid = ind->accepts_cargo[i];
		if (cid == cargo_id) {
			return ind->incoming_cargo_waiting[i];
		}
	}

	return -1;
}

/* static */ int32 ScriptIndustry::GetLastMonthProduction(IndustryID industry_id, CargoID cargo_id)
{
	if (!IsValidIndustry(industry_id)) return -1;
	if (!ScriptCargo::IsValidCargo(cargo_id)) return -1;

	const Industry *i = ::Industry::Get(industry_id);

	for (byte j = 0; j < lengthof(i->produced_cargo); j++) {
		if (i->produced_cargo[j] == cargo_id) return i->last_month_production[j];
	}

	return -1;
}

/* static */ int32 ScriptIndustry::GetLastMonthTransported(IndustryID industry_id, CargoID cargo_id)
{
	if (!IsValidIndustry(industry_id)) return -1;
	if (!ScriptCargo::IsValidCargo(cargo_id)) return -1;

	const Industry *i = ::Industry::Get(industry_id);

	for (byte j = 0; j < lengthof(i->produced_cargo); j++) {
		if (i->produced_cargo[j] == cargo_id) return i->last_month_transported[j];
	}

	return -1;
}

/* static */ int32 ScriptIndustry::GetLastMonthTransportedPercentage(IndustryID industry_id, CargoID cargo_id)
{
	if (!IsValidIndustry(industry_id)) return -1;
	if (!ScriptCargo::IsValidCargo(cargo_id)) return -1;

	const Industry *i = ::Industry::Get(industry_id);

	for (byte j = 0; j < lengthof(i->produced_cargo); j++) {
		if (i->produced_cargo[j] == cargo_id) return ::ToPercent8(i->last_month_pct_transported[j]);
	}

	return -1;
}

/* static */ TileIndex ScriptIndustry::GetLocation(IndustryID industry_id)
{
	if (!IsValidIndustry(industry_id)) return INVALID_TILE;

	return ::Industry::Get(industry_id)->location.tile;
}

/* static */ int32 ScriptIndustry::GetAmountOfStationsAround(IndustryID industry_id)
{
	if (!IsValidIndustry(industry_id)) return -1;

	Industry *ind = ::Industry::Get(industry_id);
	return (int32)ind->stations_near.size();
}

/* static */ int32 ScriptIndustry::GetDistanceManhattanToTile(IndustryID industry_id, TileIndex tile)
{
	if (!IsValidIndustry(industry_id)) return -1;

	return ScriptMap::DistanceManhattan(tile, GetLocation(industry_id));
}

/* static */ int32 ScriptIndustry::GetDistanceSquareToTile(IndustryID industry_id, TileIndex tile)
{
	if (!IsValidIndustry(industry_id)) return -1;

	return ScriptMap::DistanceSquare(tile, GetLocation(industry_id));
}

/* static */ bool ScriptIndustry::IsBuiltOnWater(IndustryID industry_id)
{
	if (!IsValidIndustry(industry_id)) return false;

	return (::GetIndustrySpec(::Industry::Get(industry_id)->type)->behaviour & INDUSTRYBEH_BUILT_ONWATER) != 0;
}

/* static */ bool ScriptIndustry::HasHeliport(IndustryID industry_id)
{
	if (!IsValidIndustry(industry_id)) return false;

	return (::GetIndustrySpec(::Industry::Get(industry_id)->type)->behaviour & INDUSTRYBEH_AI_AIRSHIP_ROUTES) != 0;
}

/* static */ TileIndex ScriptIndustry::GetHeliportLocation(IndustryID industry_id)
{
	if (!IsValidIndustry(industry_id)) return INVALID_TILE;
	if (!HasHeliport(industry_id)) return INVALID_TILE;

	const Industry *ind = ::Industry::Get(industry_id);
	TILE_AREA_LOOP(tile_cur, ind->location) {
		if (IsTileType(tile_cur, MP_STATION) && IsOilRig(tile_cur)) {
			return tile_cur;
		}
	}

	return INVALID_TILE;
}

/* static */ bool ScriptIndustry::HasDock(IndustryID industry_id)
{
	if (!IsValidIndustry(industry_id)) return false;

	return (::GetIndustrySpec(::Industry::Get(industry_id)->type)->behaviour & INDUSTRYBEH_AI_AIRSHIP_ROUTES) != 0;
}

/* static */ TileIndex ScriptIndustry::GetDockLocation(IndustryID industry_id)
{
	if (!IsValidIndustry(industry_id)) return INVALID_TILE;
	if (!HasDock(industry_id)) return INVALID_TILE;

	const Industry *ind = ::Industry::Get(industry_id);
	TILE_AREA_LOOP(tile_cur, ind->location) {
		if (IsTileType(tile_cur, MP_STATION) && IsOilRig(tile_cur)) {
			return tile_cur;
		}
	}

	return INVALID_TILE;
}

/* static */ IndustryType ScriptIndustry::GetIndustryType(IndustryID industry_id)
{
	if (!IsValidIndustry(industry_id)) return INVALID_INDUSTRYTYPE;

	return ::Industry::Get(industry_id)->type;
}
