/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_industrylist.cpp Implementation of ScriptIndustryList and friends. */

#include "../../stdafx.h"
#include "script_industrylist.hpp"
#include "../../industry.h"

#include "../../safeguards.h"

ScriptIndustryList::ScriptIndustryList()
{
	for (const Industry *i : Industry::Iterate()) {
		this->AddItem(i->index);
	}
}

ScriptIndustryList_CargoAccepting::ScriptIndustryList_CargoAccepting(CargoID cargo_id)
{
	for (const Industry *i : Industry::Iterate()) {
		if (i->IsCargoAccepted(cargo_id)) this->AddItem(i->index);
	}
}

ScriptIndustryList_CargoProducing::ScriptIndustryList_CargoProducing(CargoID cargo_id)
{
	for (const Industry *i : Industry::Iterate()) {
		if (i->IsCargoProduced(cargo_id)) this->AddItem(i->index);
	}
}
