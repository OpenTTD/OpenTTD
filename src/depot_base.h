/* $Id$ */

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

typedef Pool<Depot, DepotID, 64, 64000> DepotPool;
extern DepotPool _depot_pool;

struct Depot : DepotPool::PoolItem<&_depot_pool> {
	Town *town;
	char *name;

	TileIndex xy;
	uint16 town_cn;       ///< The N-1th depot for this town (consecutive number)
	Date build_date;      ///< Date of construction
	VehicleTypeByte type; ///< Type of the depot.
	OwnerByte owner;      ///< Owner of the depot.
	byte delete_ctr;      ///< Delete counter. If greater than 0 then it is decremented until it reaches 0; the depot is then deleted.

	Depot(TileIndex xy = INVALID_TILE, VehicleType type = VEH_INVALID, Owner owner = INVALID_OWNER)
	{
		 this->xy = xy;
		 this->type = type;
		 this->owner = owner;
	}

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
		return d->type == this->type;
	}

	/**
	 * Check whether the depot currently is in use; in use means
	 * that it is not scheduled for deletion and that it still has
	 * a building on the map. Otherwise the building is demolished
	 * and the depot awaits to be deleted.
	 * @return true iff still in use
	 * @see Depot::Disuse
	 * @see Depot::Reuse
	 */
	inline bool IsInUse() const
	{
		return this->delete_ctr == 0;
	}

	/**
	 * Cancel deletion of this depot (reuse it).
	 * @param xy New location of the depot.
	 * @see Depot::IsInUse
	 * @see Depot::Disuse
	 */

	void Reuse(TileIndex xy);
	void Disuse();
};

#define FOR_ALL_DEPOTS_FROM(var, start) FOR_ALL_ITEMS_FROM(Depot, depot_index, var, start)
#define FOR_ALL_DEPOTS(var) FOR_ALL_DEPOTS_FROM(var, 0)

#endif /* DEPOT_BASE_H */
