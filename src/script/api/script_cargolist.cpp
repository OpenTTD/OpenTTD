/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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
	for (const CargoSpec *cs : CargoSpec::Iterate()) {
		this->AddItem(cs->Index());
	}
}

ScriptCargoList_IndustryAccepting::ScriptCargoList_IndustryAccepting(IndustryID industry_id)
{
	if (!ScriptIndustry::IsValidIndustry(industry_id)) return;

	const Industry *ind = ::Industry::Get(industry_id);
	for (const auto &a : ind->accepted) {
		if (::IsValidCargoType(a.cargo)) {
			this->AddItem(a.cargo);
		}
	}
}

ScriptCargoList_IndustryProducing::ScriptCargoList_IndustryProducing(IndustryID industry_id)
{
	if (!ScriptIndustry::IsValidIndustry(industry_id)) return;

	const Industry *ind = ::Industry::Get(industry_id);
	for (const auto &p : ind->produced) {
		if (::IsValidCargoType(p.cargo)) {
			this->AddItem(p.cargo);
		}
	}
}

ScriptCargoList_StationAccepting::ScriptCargoList_StationAccepting(StationID station_id)
{
	if (!ScriptStation::IsValidStation(station_id)) return;

	const Station *st = ::Station::Get(station_id);
	for (CargoType cargo = 0; cargo < NUM_CARGO; ++cargo) {
		if (st->goods[cargo].status.Test(GoodsEntry::State::Acceptance)) this->AddItem(cargo);
	}
}
