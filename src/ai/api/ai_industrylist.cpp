/* $Id$ */

/** @file ai_industrylist.cpp Implementation of AIIndustryList and friends. */

#include "ai_industrylist.hpp"
#include "../../openttd.h"
#include "../../tile_type.h"
#include "../../industry.h"

AIIndustryList::AIIndustryList()
{
	Industry *i;
	FOR_ALL_INDUSTRIES(i) {
		this->AddItem(i->index);
	}
}

AIIndustryList_CargoAccepting::AIIndustryList_CargoAccepting(CargoID cargo_id)
{
	const Industry *i;
	const IndustrySpec *indsp;

	FOR_ALL_INDUSTRIES(i) {
		indsp = ::GetIndustrySpec(i->type);

		for (byte j = 0; j < lengthof(indsp->accepts_cargo); j++)
			if (indsp->accepts_cargo[j] == cargo_id) this->AddItem(i->index);
	}
}

AIIndustryList_CargoProducing::AIIndustryList_CargoProducing(CargoID cargo_id)
{
	const Industry *i;
	const IndustrySpec *indsp;

	FOR_ALL_INDUSTRIES(i) {
		indsp = ::GetIndustrySpec(i->type);

		for (byte j = 0; j < lengthof(indsp->produced_cargo); j++)
			if (indsp->produced_cargo[j] == cargo_id) this->AddItem(i->index);
	}
}
