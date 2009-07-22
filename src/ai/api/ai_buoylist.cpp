/* $Id$ */

/** @file ai_buoylist.cpp Implementation of AIBuoyList and friends. */

#include "ai_buoylist.hpp"
#include "../../waypoint_base.h"

AIBuoyList::AIBuoyList()
{
	Waypoint *wp;
	FOR_ALL_WAYPOINTS(wp) {
		if (wp->facilities & FACIL_DOCK) this->AddItem(wp->xy);
	}
}
