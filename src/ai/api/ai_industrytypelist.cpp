/* $Id$ */

/** @file ai_industrytypelist.cpp Implementation of AIIndustryTypeList. */

#include "ai_industrytypelist.hpp"
#include "../../tile_type.h"
#include "../../industry.h"

AIIndustryTypeList::AIIndustryTypeList()
{
	for (int i = 0; i < NUM_INDUSTRYTYPES; i++) {
		if (AIIndustryType::IsValidIndustryType(i)) this->AddItem(i);
	}
}
