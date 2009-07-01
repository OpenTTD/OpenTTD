/* $Id$ */

/** @file ai_subsidylist.cpp Implementation of AISubsidyList. */

#include "ai_subsidylist.hpp"
#include "ai_subsidy.hpp"
#include "../../subsidy_base.h"

AISubsidyList::AISubsidyList()
{
	const Subsidy *s;
	FOR_ALL_SUBSIDIES(s) {
		this->AddItem(s->Index());
	}
}
