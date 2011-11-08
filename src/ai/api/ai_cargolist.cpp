/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_cargolist.cpp Implementation of AICargoList and friends. */

#include "../../stdafx.h"
#include "ai_cargolist.hpp"
#include "ai_industry.hpp"
#include "ai_station.hpp"
#include "../../cargotype.h"
#include "../../industry.h"
#include "../../station_base.h"

AICargoList::AICargoList()
{
	const CargoSpec *cs;
	FOR_ALL_CARGOSPECS(cs) {
		this->AddItem(cs->Index());
	}
}

AICargoList_IndustryAccepting::AICargoList_IndustryAccepting(IndustryID industry_id)
{
	if (!AIIndustry::IsValidIndustry(industry_id)) return;

	Industry *ind = ::Industry::Get(industry_id);
	for (uint i = 0; i < lengthof(ind->accepts_cargo); i++) {
		CargoID cargo_id = ind->accepts_cargo[i];
		if (cargo_id != CT_INVALID) {
			this->AddItem(cargo_id);
		}
	}
}

AICargoList_IndustryProducing::AICargoList_IndustryProducing(IndustryID industry_id)
{
	if (!AIIndustry::IsValidIndustry(industry_id)) return;

	Industry *ind = ::Industry::Get(industry_id);
	for (uint i = 0; i < lengthof(ind->produced_cargo); i++) {
		CargoID cargo_id = ind->produced_cargo[i];
		if (cargo_id != CT_INVALID) {
			this->AddItem(cargo_id);
		}
	}
}

AICargoList_StationAccepting::AICargoList_StationAccepting(StationID station_id)
{
	if (!AIStation::IsValidStation(station_id)) return;

	Station *st = ::Station::Get(station_id);
	for (CargoID i = 0; i < NUM_CARGO; i++) {
		if (HasBit(st->goods[i].acceptance_pickup, GoodsEntry::GES_ACCEPTANCE)) this->AddItem(i);
	}
}
