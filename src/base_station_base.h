/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file base_station_base.h Base classes/functions for base stations. */

#ifndef BASE_STATION_BASE_H
#define BASE_STATION_BASE_H

#include "core/pool_type.hpp"
#include "command_type.h"
#include "viewport_type.h"
#include "station_map.h"
#include "timer/timer_game_calendar.h"

typedef Pool<BaseStation, StationID, 32, 64000> StationPool;
extern StationPool _station_pool;

struct StationSpecList {
	const StationSpec *spec;
	uint32_t grfid;      ///< GRF ID of this custom station
	uint16_t localidx; ///< Station ID within GRF of station
};

struct RoadStopSpecList {
	const RoadStopSpec *spec;
	uint32_t grfid;      ///< GRF ID of this custom road stop
	uint16_t localidx; ///< Station ID within GRF of road stop
};

struct RoadStopTileData {
	TileIndex tile;
	uint8_t random_bits;
	uint8_t animation_frame;
};

/** StationRect - used to track station spread out rectangle - cheaper than scanning whole map */
struct StationRect : public Rect {
	enum StationRectMode
	{
		ADD_TEST = 0,
		ADD_TRY,
		ADD_FORCE
	};

	StationRect();
	void MakeEmpty();
	bool PtInExtendedRect(int x, int y, int distance = 0) const;
	bool IsEmpty() const;
	CommandCost BeforeAddTile(TileIndex tile, StationRectMode mode);
	CommandCost BeforeAddRect(TileIndex tile, int w, int h, StationRectMode mode);
	bool AfterRemoveTile(BaseStation *st, TileIndex tile);
	bool AfterRemoveRect(BaseStation *st, TileArea ta);

	static bool ScanForStationTiles(StationID st_id, int left_a, int top_a, int right_a, int bottom_a);

	StationRect& operator = (const Rect &src);
};

/** Base class for all station-ish types */
struct BaseStation : StationPool::PoolItem<&_station_pool> {
	TileIndex xy;                   ///< Base tile of the station
	TrackedViewportSign sign;       ///< NOSAVE: Dimensions of sign
	byte delete_ctr;                ///< Delete counter. If greater than 0 then it is decremented until it reaches 0; the waypoint is then is deleted.

	std::string name;               ///< Custom name
	StringID string_id;             ///< Default name (town area) of station
	mutable std::string cached_name; ///< NOSAVE: Cache of the resolved name of the station, if not using a custom name

	Town *town;                     ///< The town this station is associated with
	Owner owner;                    ///< The owner of this station
	StationFacility facilities;     ///< The facilities that this station has

	std::vector<StationSpecList> speclist;           ///< List of rail station specs of this station.
	std::vector<RoadStopSpecList> roadstop_speclist; ///< List of road stop specs of this station

	TimerGameCalendar::Date build_date; ///< Date of construction

	uint16_t random_bits;             ///< Random bits assigned to this station
	byte waiting_triggers;          ///< Waiting triggers (NewGRF) for this station
	uint8_t cached_anim_triggers;                ///< NOSAVE: Combined animation trigger bitmask, used to determine if trigger processing should happen.
	uint8_t cached_roadstop_anim_triggers;       ///< NOSAVE: Combined animation trigger bitmask for road stops, used to determine if trigger processing should happen.
	CargoTypes cached_cargo_triggers;          ///< NOSAVE: Combined cargo trigger bitmask
	CargoTypes cached_roadstop_cargo_triggers; ///< NOSAVE: Combined cargo trigger bitmask for road stops

	TileArea train_station;         ///< Tile area the train 'station' part covers
	StationRect rect;               ///< NOSAVE: Station spread out rectangle maintained by StationRect::xxx() functions

	std::vector<RoadStopTileData> custom_roadstop_tile_data; ///< List of custom road stop tile data

	/**
	 * Initialize the base station.
	 * @param tile The location of the station sign
	 */
	BaseStation(TileIndex tile) :
		xy(tile),
		train_station(INVALID_TILE, 0, 0)
	{
	}

	virtual ~BaseStation();

	/**
	 * Check whether a specific tile belongs to this station.
	 * @param tile the tile to check
	 * @return true if the tile belongs to this station
	 */
	virtual bool TileBelongsToRailStation(TileIndex tile) const = 0;

	/**
	 * Helper function to get a NewGRF variable that isn't implemented by the base class.
	 * @param object the resolver object related to this query
	 * @param variable that is queried
	 * @param parameter parameter for that variable
	 * @param available will return false if ever the variable asked for does not exist
	 * @return the value stored in the corresponding variable
	 */
	virtual uint32_t GetNewGRFVariable(const struct ResolverObject &object, byte variable, byte parameter, bool *available) const = 0;

	/**
	 * Update the coordinated of the sign (as shown in the viewport).
	 */
	virtual void UpdateVirtCoord() = 0;

	inline const std::string &GetCachedName() const
	{
		if (!this->name.empty()) return this->name;
		if (this->cached_name.empty()) this->FillCachedName();
		return this->cached_name;
	}

	virtual void MoveSign(TileIndex new_xy)
	{
		this->xy = new_xy;
		this->UpdateVirtCoord();
	}

	/**
	 * Get the tile area for a given station type.
	 * @param ta tile area to fill.
	 * @param type the type of the area
	 */
	virtual void GetTileArea(TileArea *ta, StationType type) const = 0;


	/**
	 * Obtain the length of a platform
	 * @pre tile must be a rail station tile
	 * @param tile A tile that contains the platform in question
	 * @return The length of the platform
	 */
	virtual uint GetPlatformLength(TileIndex tile) const = 0;

	/**
	 * Determines the REMAINING length of a platform, starting at (and including)
	 * the given tile.
	 * @param tile the tile from which to start searching. Must be a rail station tile
	 * @param dir The direction in which to search.
	 * @return The platform length
	 */
	virtual uint GetPlatformLength(TileIndex tile, DiagDirection dir) const = 0;

	/**
	 * Get the base station belonging to a specific tile.
	 * @param tile The tile to get the base station from.
	 * @return the station associated with that tile.
	 */
	static inline BaseStation *GetByTile(TileIndex tile)
	{
		return BaseStation::Get(GetStationIndex(tile));
	}

	/**
	 * Check whether the base station currently is in use; in use means
	 * that it is not scheduled for deletion and that it still has some
	 * facilities left.
	 * @return true if still in use
	 */
	inline bool IsInUse() const
	{
		return (this->facilities & ~FACIL_WAYPOINT) != 0;
	}

	inline byte GetRoadStopRandomBits(TileIndex tile) const
	{
		for (const RoadStopTileData &tile_data : this->custom_roadstop_tile_data) {
			if (tile_data.tile == tile) return tile_data.random_bits;
		}
		return 0;
	}

	inline byte GetRoadStopAnimationFrame(TileIndex tile) const
	{
		for (const RoadStopTileData &tile_data : this->custom_roadstop_tile_data) {
			if (tile_data.tile == tile) return tile_data.animation_frame;
		}
		return 0;
	}

private:
	void SetRoadStopTileData(TileIndex tile, byte data, bool animation);

public:
	inline void SetRoadStopRandomBits(TileIndex tile, byte random_bits) { this->SetRoadStopTileData(tile, random_bits, false); }
	inline void SetRoadStopAnimationFrame(TileIndex tile, byte frame) { this->SetRoadStopTileData(tile, frame, true); }
	void RemoveRoadStopTileData(TileIndex tile);

	static void PostDestructor(size_t index);

private:
	void FillCachedName() const;
};

/**
 * Class defining several overloaded accessors so we don't
 * have to cast base stations that often
 */
template <class T, bool Tis_waypoint>
struct SpecializedStation : public BaseStation {
	static const StationFacility EXPECTED_FACIL = Tis_waypoint ? FACIL_WAYPOINT : FACIL_NONE; ///< Specialized type

	/**
	 * Set station type correctly
	 * @param tile The base tile of the station.
	 */
	inline SpecializedStation<T, Tis_waypoint>(TileIndex tile) :
			BaseStation(tile)
	{
		this->facilities = EXPECTED_FACIL;
	}

	/**
	 * Helper for checking whether the given station is of this type.
	 * @param st the station to check.
	 * @return true if the station is the type we expect it to be.
	 */
	static inline bool IsExpected(const BaseStation *st)
	{
		return (st->facilities & FACIL_WAYPOINT) == EXPECTED_FACIL;
	}

	/**
	 * Tests whether given index is a valid index for station of this type
	 * @param index tested index
	 * @return is this index valid index of T?
	 */
	static inline bool IsValidID(size_t index)
	{
		return BaseStation::IsValidID(index) && IsExpected(BaseStation::Get(index));
	}

	/**
	 * Gets station with given index
	 * @return pointer to station with given index casted to T *
	 */
	static inline T *Get(size_t index)
	{
		return (T *)BaseStation::Get(index);
	}

	/**
	 * Returns station if the index is a valid index for this station type
	 * @return pointer to station with given index if it's a station of this type
	 */
	static inline T *GetIfValid(size_t index)
	{
		return IsValidID(index) ? Get(index) : nullptr;
	}

	/**
	 * Get the station belonging to a specific tile.
	 * @param tile The tile to get the station from.
	 * @return the station associated with that tile.
	 */
	static inline T *GetByTile(TileIndex tile)
	{
		return GetIfValid(GetStationIndex(tile));
	}

	/**
	 * Converts a BaseStation to SpecializedStation with type checking.
	 * @param st BaseStation pointer
	 * @return pointer to SpecializedStation
	 */
	static inline T *From(BaseStation *st)
	{
		assert(IsExpected(st));
		return (T *)st;
	}

	/**
	 * Converts a const BaseStation to const SpecializedStation with type checking.
	 * @param st BaseStation pointer
	 * @return pointer to SpecializedStation
	 */
	static inline const T *From(const BaseStation *st)
	{
		assert(IsExpected(st));
		return (const T *)st;
	}

	/**
	 * Returns an iterable ensemble of all valid stations of type T
	 * @param from index of the first station to consider
	 * @return an iterable ensemble of all valid stations of type T
	 */
	static Pool::IterateWrapper<T> Iterate(size_t from = 0) { return Pool::IterateWrapper<T>(from); }
};

#endif /* BASE_STATION_BASE_H */
