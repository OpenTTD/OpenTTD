/* $Id$ */

/** @file ai_grouplist.cpp Implementation of AIGroupList and friends. */

#include "ai_grouplist.hpp"
#include "../../company_func.h"
#include "../../group.h"

AIGroupList::AIGroupList()
{
	Group *g;
	FOR_ALL_GROUPS(g) {
		if (g->owner == _current_company) this->AddItem(g->index);
	}
}
