/* $Id$ */

/** @file ai_cargolist.cpp Implementation of AICargoList and friends. */

#include "ai_cargolist.hpp"
#include "ai_industry.hpp"
#include "../../cargotype.h"
#include "../../tile_type.h"
#include "../../industry.h"

AICargoList::AICargoList()
{
	for (byte i = 0; i < NUM_CARGO; i++) {
		const CargoSpec *c = ::GetCargo(i);
		if (c->IsValid()) {
			this->AddItem(i);
		}
	}
}

AICargoList_IndustryAccepting::AICargoList_IndustryAccepting(IndustryID industry_id)
{
	if (!AIIndustry::IsValidIndustry(industry_id)) return;

	Industry *ind = ::GetIndustry(industry_id);
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

	Industry *ind = ::GetIndustry(industry_id);
	for (uint i = 0; i < lengthof(ind->produced_cargo); i++) {
		CargoID cargo_id = ind->produced_cargo[i];
		if (cargo_id != CT_INVALID) {
			this->AddItem(cargo_id);
		}
	}
}
