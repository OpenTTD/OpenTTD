/*
* This file is part of OpenTTD.
* OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
* OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
*/

/** @file town_kdtree.h Declarations for accessing the k-d tree of towns */

#ifndef VIEWPORT_KDTREE_H
#define VIEWPORT_KDTREE_H

#include "core/kdtree.hpp"
#include "viewport_type.h"
#include "station_base.h"
#include "town_type.h"
#include "signs_base.h"

struct ViewportSignKdtreeItem {
	enum ItemType : uint16 {
		VKI_STATION,
		VKI_WAYPOINT,
		VKI_TOWN,
		VKI_SIGN,
		VKI_DEPOT,
	};
	ItemType type;
	union {
		StationID station;
		TownID town;
		SignID sign;
		DepotID depot;
	} id;
	int32 center;
	int32 top;

	bool operator== (const ViewportSignKdtreeItem &other) const
	{
		if (this->type != other.type) return false;
		switch (this->type) {
			case VKI_STATION:
			case VKI_WAYPOINT:
				return this->id.station == other.id.station;
			case VKI_TOWN:
				return this->id.town == other.id.town;
			case VKI_SIGN:
				return this->id.sign == other.id.sign;
			case VKI_DEPOT:
				return this->id.depot == other.id.depot;
			default:
				NOT_REACHED();
		}
	}

	bool operator< (const ViewportSignKdtreeItem &other) const
	{
		if (this->type != other.type) return this->type < other.type;
		switch (this->type) {
			case VKI_STATION:
			case VKI_WAYPOINT:
				return this->id.station < other.id.station;
			case VKI_TOWN:
				return this->id.town < other.id.town;
			case VKI_SIGN:
				return this->id.sign < other.id.sign;
			case VKI_DEPOT:
				return this->id.depot < other.id.depot;
			default:
				NOT_REACHED();
		}
	}

	static ViewportSignKdtreeItem MakeStation(StationID id);
	static ViewportSignKdtreeItem MakeWaypoint(StationID id);
	static ViewportSignKdtreeItem MakeTown(TownID id);
	static ViewportSignKdtreeItem MakeSign(SignID id);
	static ViewportSignKdtreeItem MakeDepot(DepotID id);
};

inline int32 Kdtree_ViewportSignXYFunc(const ViewportSignKdtreeItem &item, int dim)
{
	return (dim == 0) ? item.center : item.top;
}

typedef Kdtree<ViewportSignKdtreeItem, decltype(&Kdtree_ViewportSignXYFunc), int32, int32> ViewportSignKdtree;
extern ViewportSignKdtree _viewport_sign_kdtree;

void RebuildViewportKdtree();

#endif
