/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_cargolist.cpp Implementation of ScriptCargoList and friends. */

#include "../../stdafx.h"
#include "script_cargolist.hpp"
#include "script_industry.hpp"
#include "script_station.hpp"
#include "../../cargotype.h"
#include "../../industry.h"
#include "../../station_base.h"

#include "../../safeguards.h"

ScriptCargoList::ScriptCargoList()
{
	const CargoSpec *cs;
	FOR_ALL_CARGOSPECS(cs) {
		this->AddItem(cs->Index());
	}
}

ScriptCargoList_IndustryAccepting::ScriptCargoList_IndustryAccepting(IndustryID industry_id)
{
	if (!ScriptIndustry::IsValidIndustry(industry_id)) return;

	Industry *ind = ::Industry::Get(industry_id);
	for (uint i = 0; i < lengthof(ind->accepts_cargo); i++) {
		CargoID cargo_id = ind->accepts_cargo[i];
		if (cargo_id != CT_INVALID) {
			this->AddItem(cargo_id);
		}
	}
}

ScriptCargoList_IndustryProducing::ScriptCargoList_IndustryProducing(IndustryID industry_id)
{
	if (!ScriptIndustry::IsValidIndustry(industry_id)) return;

	Industry *ind = ::Industry::Get(industry_id);
	for (uint i = 0; i < lengthof(ind->produced_cargo); i++) {
		CargoID cargo_id = ind->produced_cargo[i];
		if (cargo_id != CT_INVALID) {
			this->AddItem(cargo_id);
		}
	}
}

ScriptCargoList_StationAccepting::ScriptCargoList_StationAccepting(StationID station_id)
{
	if (!ScriptStation::IsValidStation(station_id)) return;

	Station *st = ::Station::Get(station_id);
	for (CargoID i = 0; i < NUM_CARGO; i++) {
		if (HasBit(st->goods[i].status, GoodsEntry::GES_ACCEPTANCE)) this->AddItem(i);
	}
}
