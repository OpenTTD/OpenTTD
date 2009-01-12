/* $Id$ */

/** @file ai_railtypelist.cpp Implementation of AIRailTypeList and friends. */

#include "ai_railtypelist.hpp"
#include "../../rail.h"
#include "../../company_func.h"

AIRailTypeList::AIRailTypeList()
{
	for (RailType rt = RAILTYPE_BEGIN; rt != RAILTYPE_END; rt++) {
		if (::HasRailtypeAvail(_current_company, rt)) this->AddItem(rt);
	}
}
