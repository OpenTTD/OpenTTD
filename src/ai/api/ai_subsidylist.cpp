/* $Id$ */

/** @file ai_subsidylist.cpp Implementation of AISubsidyList. */

#include "ai_subsidylist.hpp"
#include "ai_subsidy.hpp"
#include "../../economy_func.h"

AISubsidyList::AISubsidyList()
{
	for (uint i = 0; i < lengthof(_subsidies); i++) {
		if (AISubsidy::IsValidSubsidy(i)) this->AddItem(i);
	}
}
