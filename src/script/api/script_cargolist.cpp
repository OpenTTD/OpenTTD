/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_cargolist.cpp Implementation of ScriptCargoList and friends. */

#include "../../stdafx.h"
#include "script_cargolist.h"
#include "script_industry.h"
#include "script_station.h"
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

	Industry *ind = ::Industry::Get(industry_id);
	for (const auto &a : ind->accepted) {
		if (::IsValidCargoID(a.cargo)) {
			this->AddItem(a.cargo);
		}
	}
}

ScriptCargoList_IndustryProducing::ScriptCargoList_IndustryProducing(IndustryID industry_id)
{
	if (!ScriptIndustry::IsValidIndustry(industry_id)) return;

	Industry *ind = ::Industry::Get(industry_id);
	for (const auto &p : ind->produced) {
		if (::IsValidCargoID(p.cargo)) {
			this->AddItem(p.cargo);
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
