/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file roadstop_base.h Base class for roadstops. */

#ifndef ROADSTOP_BASE_H
#define ROADSTOP_BASE_H

#include "station_type.h"
#include "core/pool_type.hpp"
#include "vehicle_type.h"

using RoadStopPool = Pool<RoadStop, RoadStopID, 32>;
extern RoadStopPool _roadstop_pool;

/** A Stop for a Road Vehicle */
struct RoadStop : RoadStopPool::PoolItem<&_roadstop_pool> {
	enum class RoadStopStatusFlag : uint8_t {
		Bay0Free  = 0, ///< Non-zero when bay 0 is free
		Bay1Free  = 1, ///< Non-zero when bay 1 is free
		BaseEntry = 6, ///< Non-zero when the entries on this road stop are the primary, i.e. the ones to delete
		EntryBusy = 7, ///< Non-zero when roadstop entry is busy
	};
	using RoadStopStatusFlags = EnumBitSet<RoadStopStatusFlag, uint8_t>;

	/** Container for each entry point of a drive through road stop */
	struct Entry {
	private:
		uint16_t length = 0; ///< The length of the stop in tile 'units'
		uint16_t occupied = 0; ///< The amount of occupied stop in tile 'units'

	public:
		friend struct RoadStop; ///< Oh yeah, the road stop may play with me.

		/**
		 * Get the length of this drive through stop.
		 * @return the length in tile units.
		 */
		inline int GetLength() const
		{
			return this->length;
		}

		/**
		 * Get the amount of occupied space in this drive through stop.
		 * @return the occupied space in tile units.
		 */
		inline int GetOccupied() const
		{
			return this->occupied;
		}

		void Leave(const RoadVehicle *rv);
		void Enter(const RoadVehicle *rv);
		void CheckIntegrity(const RoadStop *rs) const;
		void Rebuild(const RoadStop *rs, int side = -1);
	};

	/** Container for both east and west entry points. */
	struct Entries {
		Entry east{}; ///< Information for vehicles that entered from the east
		Entry west{}; ///< Information for vehicles that entered from the west
	};

	RoadStopStatusFlags status{RoadStopStatusFlag::Bay0Free, RoadStopStatusFlag::Bay1Free}; ///< Current status of the Stop. Access using *Bay and *Busy functions.
	TileIndex xy = INVALID_TILE; ///< Position on the map
	RoadStop *next = nullptr; ///< Next stop of the given type at this station

	/** Initializes a RoadStop */
	inline RoadStop(TileIndex tile = INVALID_TILE) : xy(tile) { }

	~RoadStop();

	/**
	 * Checks whether there is a free bay in this road stop
	 * @return is at least one bay free?
	 */
	inline bool HasFreeBay() const
	{
		return this->status.Any({RoadStopStatusFlag::Bay0Free, RoadStopStatusFlag::Bay1Free});
	}

	/**
	 * Checks whether the given bay is free in this road stop
	 * @param nr bay to check
	 * @return is given bay free?
	 */
	inline bool IsFreeBay(uint nr) const
	{
		switch (nr) {
			case 0: return this->status.Test(RoadStopStatusFlag::Bay0Free);
			case 1: return this->status.Test(RoadStopStatusFlag::Bay1Free);
			default: NOT_REACHED();
		}
	}

	/**
	 * Checks whether the entrance of the road stop is occupied by a vehicle
	 * @return is entrance busy?
	 */
	inline bool IsEntranceBusy() const
	{
		return this->status.Test(RoadStopStatusFlag::EntryBusy);
	}

	/**
	 * Makes an entrance occupied or free
	 * @param busy If true, marks busy; free otherwise.
	 */
	inline void SetEntranceBusy(bool busy)
	{
		this->status.Set(RoadStopStatusFlag::EntryBusy, busy);
	}

	/**
	 * Get the drive through road stop entry struct for the given direction.
	 * @param dir The direction to get the entry for.
	 * @return the entry
	 */
	inline const Entry &GetEntry(DiagDirection dir) const
	{
		return dir >= DIAGDIR_SW ? this->entries->west : this->entries->east;
	}

	/**
	 * Get the drive through road stop entry struct for the given direction.
	 * @param dir The direction to get the entry for.
	 * @return the entry
	 */
	inline Entry &GetEntry(DiagDirection dir)
	{
		return dir >= DIAGDIR_SW ? this->entries->west : this->entries->east;
	}

	void MakeDriveThrough();
	void ClearDriveThrough();

	void Leave(RoadVehicle *rv);
	bool Enter(RoadVehicle *rv);

	RoadStop *GetNextRoadStop(const struct RoadVehicle *v) const;

	static RoadStop *GetByTile(TileIndex tile, RoadStopType type);

	static bool IsDriveThroughRoadStopContinuation(TileIndex rs, TileIndex next);

private:
	Entries *entries = nullptr; ///< Information about available and allocated bays.

	/**
	 * Allocates a bay
	 * @return the allocated bay number
	 * @pre this->HasFreeBay()
	 */
	inline uint AllocateBay()
	{
		assert(this->HasFreeBay());

		/* Find the first free bay. */
		uint bay_nr = 0;
		while (!this->IsFreeBay(bay_nr)) ++bay_nr;

		this->AllocateDriveThroughBay(bay_nr);
		return bay_nr;
	}

	/**
	 * Allocates a bay in a drive-through road stop
	 * @param nr the number of the bay to allocate
	 */
	inline void AllocateDriveThroughBay(uint nr)
	{
		switch (nr) {
			case 0: this->status.Reset(RoadStopStatusFlag::Bay0Free); break;
			case 1: this->status.Reset(RoadStopStatusFlag::Bay1Free); break;
			default: NOT_REACHED();
		}
	}

	/**
	 * Frees the given bay
	 * @param nr the number of the bay to free
	 */
	inline void FreeBay(uint nr)
	{
		switch (nr) {
			case 0: this->status.Set(RoadStopStatusFlag::Bay0Free); break;
			case 1: this->status.Set(RoadStopStatusFlag::Bay1Free); break;
			default: NOT_REACHED();
		}
	}
};

#endif /* ROADSTOP_BASE_H */
