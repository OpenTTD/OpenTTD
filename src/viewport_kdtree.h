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
	enum ItemType : uint16_t {
		VKI_STATION,
		VKI_WAYPOINT,
		VKI_TOWN,
		VKI_SIGN,
	};
	ItemType type;
	std::variant<StationID, TownID, SignID> id;
	int32_t center;
	int32_t top;

	bool operator== (const ViewportSignKdtreeItem &other) const
	{
		if (this->type != other.type) return false;
		return this->id == other.id;
	}

	bool operator< (const ViewportSignKdtreeItem &other) const
	{
		if (this->type != other.type) return this->type < other.type;
		return this->id < other.id;
	}

	static ViewportSignKdtreeItem MakeStation(StationID id);
	static ViewportSignKdtreeItem MakeWaypoint(StationID id);
	static ViewportSignKdtreeItem MakeTown(TownID id);
	static ViewportSignKdtreeItem MakeSign(SignID id);
};

struct Kdtree_ViewportSignXYFunc {
	inline int32_t operator()(const ViewportSignKdtreeItem &item, int dim)
	{
		return (dim == 0) ? item.center : item.top;
	}
};

using ViewportSignKdtree = Kdtree<ViewportSignKdtreeItem, Kdtree_ViewportSignXYFunc, int32_t, int32_t>;
extern ViewportSignKdtree _viewport_sign_kdtree;

void RebuildViewportKdtree();

#endif
