/* $Id$ */

/** @file ai_townlist.cpp Implementation of AITownList and friends. */

#include "ai_townlist.hpp"
#include "../../town.h"

AITownList::AITownList()
{
	Town *t;
	FOR_ALL_TOWNS(t) {
		this->AddItem(t->index);
	}
}
