/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

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
