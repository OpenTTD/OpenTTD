/* $Id$ */

/** @file ai_buoylist.cpp Implementation of AIBuoyList and friends. */

#include "ai_buoylist.hpp"
#include "../../station_base.h"

AIBuoyList::AIBuoyList()
{
	Station *st;
	FOR_ALL_STATIONS(st) {
		if (st->IsBuoy()) this->AddItem(st->xy);
	}
}
