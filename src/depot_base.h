/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file depot_base.h Base for all depots (except hangars) */

#ifndef DEPOT_BASE_H
#define DEPOT_BASE_H

#include "depot_map.h"
#include "core/pool_type.hpp"
#include "timer/timer_game_calendar.h"

typedef Pool<Depot, DepotID, 64, 64000> DepotPool;
extern DepotPool _depot_pool;

struct Depot : DepotPool::PoolItem<&_depot_pool> {
	/* DepotID index member of DepotPool is 2 bytes. */
	uint16_t town_cn; ///< The N-1th depot for this town (consecutive number)
	TileIndex xy;
	Town *town;
	std::string name;
	TimerGameCalendar::Date build_date; ///< Date of construction

	Depot(TileIndex xy = INVALID_TILE) : xy(xy) {}
	~Depot();

	static inline Depot *GetByTile(TileIndex tile)
	{
		return Depot::Get(GetDepotIndex(tile));
	}

	/**
	 * Is the "type" of depot the same as the given depot,
	 * i.e. are both a rail, road or ship depots?
	 * @param d The depot to compare to.
	 * @return true iff their types are equal.
	 */
	inline bool IsOfType(const Depot *d) const
	{
		return GetTileType(d->xy) == GetTileType(this->xy);
	}
};

#endif /* DEPOT_BASE_H */
