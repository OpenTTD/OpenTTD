/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file  vehicle_base.h Base class for all vehicles. */

#ifndef VEHICLE_BASE_H
#define VEHICLE_BASE_H

#include "track_type.h"
#include "direction_type.h"
#include "command_type.h"
#include "order_base.h"
#include "cargopacket.h"
#include "texteff.hpp"
#include "engine_type.h"
#include "order_func.h"
#include "transport_type.h"
#include "group_type.h"

/** Vehicle status bits in #Vehicle::vehstatus. */
enum VehStatus {
	VS_HIDDEN          = 0x01, ///< Vehicle is not visible.
	VS_STOPPED         = 0x02, ///< Vehicle is stopped by the player.
	VS_UNCLICKABLE     = 0x04, ///< Vehicle is not clickable by the user (shadow vehicles).
	VS_DEFPAL          = 0x08, ///< Use default vehicle palette. @see DoDrawVehicle
	VS_TRAIN_SLOWING   = 0x10, ///< Train is slowing down.
	VS_SHADOW          = 0x20, ///< Vehicle is a shadow vehicle.
	VS_AIRCRAFT_BROKEN = 0x40, ///< Aircraft is broken down.
	VS_CRASHED         = 0x80, ///< Vehicle is crashed.
};

/** Bit numbers in #Vehicle::vehicle_flags. */
enum VehicleFlags {
	VF_LOADING_FINISHED,        ///< Vehicle has finished loading.
	VF_CARGO_UNLOADING,         ///< Vehicle is unloading cargo.
	VF_BUILT_AS_PROTOTYPE,      ///< Vehicle is a prototype (accepted as exclusive preview).
	VF_TIMETABLE_STARTED,       ///< Whether the vehicle has started running on the timetable yet.
	VF_AUTOFILL_TIMETABLE,      ///< Whether the vehicle should fill in the timetable automatically.
	VF_AUTOFILL_PRES_WAIT_TIME, ///< Whether non-destructive auto-fill should preserve waiting times
	VF_STOP_LOADING,            ///< Don't load anymore during the next load cycle.
	VF_PATHFINDER_LOST,         ///< Vehicle's pathfinder is lost.
};

/** Bit numbers used to indicate which of the #NewGRFCache values are valid. */
enum NewGRFCacheValidValues {
	NCVV_POSITION_CONSIST_LENGTH   = 0, ///< This bit will be set if the NewGRF var 40 currently stored is valid.
	NCVV_POSITION_SAME_ID_LENGTH   = 1, ///< This bit will be set if the NewGRF var 41 currently stored is valid.
	NCVV_CONSIST_CARGO_INFORMATION = 2, ///< This bit will be set if the NewGRF var 42 currently stored is valid.
	NCVV_COMPANY_INFORMATION       = 3, ///< This bit will be set if the NewGRF var 43 currently stored is valid.
	NCVV_END,                           ///< End of the bits.
};

/** Cached often queried (NewGRF) values */
struct NewGRFCache {
	/* Values calculated when they are requested for the first time after invalidating the NewGRF cache. */
	uint32 position_consist_length;   ///< Cache for NewGRF var 40.
	uint32 position_same_id_length;   ///< Cache for NewGRF var 41.
	uint32 consist_cargo_information; ///< Cache for NewGRF var 42.
	uint32 company_information;       ///< Cache for NewGRF var 43.
	uint8  cache_valid;               ///< Bitset that indicates which cache values are valid.
};

/** Meaning of the various bits of the visual effect. */
enum VisualEffect {
	VE_OFFSET_START        = 0, ///< First bit that contains the offset (0 = front, 8 = centre, 15 = rear)
	VE_OFFSET_COUNT        = 4, ///< Number of bits used for the offset
	VE_OFFSET_CENTRE       = 8, ///< Value of offset corresponding to a position above the centre of the vehicle

	VE_TYPE_START          = 4, ///< First bit used for the type of effect
	VE_TYPE_COUNT          = 2, ///< Number of bits used for the effect type
	VE_TYPE_DEFAULT        = 0, ///< Use default from engine class
	VE_TYPE_STEAM          = 1, ///< Steam plumes
	VE_TYPE_DIESEL         = 2, ///< Diesel fumes
	VE_TYPE_ELECTRIC       = 3, ///< Electric sparks

	VE_DISABLE_EFFECT      = 6, ///< Flag to disable visual effect
	VE_DISABLE_WAGON_POWER = 7, ///< Flag to disable wagon power

	VE_DEFAULT = 0xFF,          ///< Default value to indicate that visual effect should be based on engine class
};

/** Cached often queried values common to all vehicles. */
struct VehicleCache {
	uint16 cached_max_speed; ///< Maximum speed of the consist (minimum of the max speed of all vehicles in the consist).

	byte cached_vis_effect;  ///< Visual effect to show (see #VisualEffect)
};

/** A vehicle pool for a little over 1 million vehicles. */
typedef Pool<Vehicle, VehicleID, 512, 0xFF000> VehiclePool;
extern VehiclePool _vehicle_pool;

/* Some declarations of functions, so we can make them friendly */
struct SaveLoad;
struct GroundVehicleCache;
extern const SaveLoad *GetVehicleDescription(VehicleType vt);
struct LoadgameState;
extern bool LoadOldVehicle(LoadgameState *ls, int num);
extern bool AfterLoadGame();
extern void FixOldVehicles();

/** Vehicle data structure. */
struct Vehicle : VehiclePool::PoolItem<&_vehicle_pool>, BaseVehicle {
private:
	Vehicle *next;                      ///< pointer to the next vehicle in the chain
	Vehicle *previous;                  ///< NOSAVE: pointer to the previous vehicle in the chain
	Vehicle *first;                     ///< NOSAVE: pointer to the first vehicle in the chain

	Vehicle *next_shared;               ///< pointer to the next vehicle that shares the order
	Vehicle *previous_shared;           ///< NOSAVE: pointer to the previous vehicle in the shared order chain
public:
	friend const SaveLoad *GetVehicleDescription(VehicleType vt); ///< So we can use private/protected variables in the saveload code
	friend bool AfterLoadGame();
	friend void FixOldVehicles();
	friend void AfterLoadVehicles(bool part_of_load);             ///< So we can set the previous and first pointers while loading
	friend bool LoadOldVehicle(LoadgameState *ls, int num);       ///< So we can set the proper next pointer while loading

	char *name;                         ///< Name of vehicle

	TileIndex tile;                     ///< Current tile index

	/**
	 * Heading for this tile.
	 * For airports and train stations this tile does not necessarily belong to the destination station,
	 * but it can be used for heuristical purposes to estimate the distance.
	 */
	TileIndex dest_tile;

	Money profit_this_year;             ///< Profit this year << 8, low 8 bits are fract
	Money profit_last_year;             ///< Profit last year << 8, low 8 bits are fract
	Money value;                        ///< Value of the vehicle

	CargoPayment *cargo_payment;        ///< The cargo payment we're currently in

	/* Used for timetabling. */
	uint32 current_order_time;          ///< How many ticks have passed since this order started.
	int32 lateness_counter;             ///< How many ticks late (or early if negative) this vehicle is.
	Date timetable_start;               ///< When the vehicle is supposed to start the timetable.

	/* Boundaries for the current position in the world and a next hash link.
	 * NOSAVE: All of those can be updated with VehiclePositionChanged() */
	Rect coord;
	Vehicle *next_hash, **prev_hash;
	Vehicle *next_new_hash, **prev_new_hash;
	Vehicle **old_new_hash;

	SpriteID colourmap;                 ///< NOSAVE: cached colour mapping

	/* Related to age and service time */
	Year build_year;
	Date age;                           ///< Age in days
	Date max_age;                       ///< Maximum age
	Date date_of_last_service;
	Date service_interval;
	uint16 reliability;                 ///< Reliability.
	uint16 reliability_spd_dec;         ///< Reliability decrease speed.
	byte breakdown_ctr;                 ///< Counter for managing breakdown events. @see Vehicle::HandleBreakdown
	byte breakdown_delay;               ///< Counter for managing breakdown length.
	byte breakdowns_since_last_service;
	byte breakdown_chance;

	int32 x_pos;                        ///< x coordinate.
	int32 y_pos;                        ///< y coordinate.
	byte z_pos;                         ///< z coordinate.
	DirectionByte direction;            ///< facing

	OwnerByte owner;                    ///< Which company owns the vehicle?
	byte spritenum;                     ///< currently displayed sprite index
	                                    ///< 0xfd == custom sprite, 0xfe == custom second head sprite
	                                    ///< 0xff == reserved for another custom sprite
	SpriteID cur_image;                 ///< sprite number for this vehicle
	byte x_extent;                      ///< x-extent of vehicle bounding box
	byte y_extent;                      ///< y-extent of vehicle bounding box
	byte z_extent;                      ///< z-extent of vehicle bounding box
	int8 x_offs;                        ///< x offset for vehicle sprite
	int8 y_offs;                        ///< y offset for vehicle sprite
	EngineID engine_type;

	TextEffectID fill_percent_te_id;    ///< a text-effect id to a loading indicator object
	UnitID unitnumber;                  ///< unit number, for display purposes only

	uint16 cur_speed;                   ///< current speed
	byte subspeed;                      ///< fractional speed
	byte acceleration;                  ///< used by train & aircraft
	uint32 motion_counter;
	byte progress;

	/* for randomized variational spritegroups
	 * bitmask used to resolve them; parts of it get reseeded when triggers
	 * of corresponding spritegroups get matched */
	byte random_bits;
	byte waiting_triggers;              ///< triggers to be yet matched

	StationID last_station_visited;

	CargoID cargo_type;                 ///< type of cargo this vehicle is carrying
	byte cargo_subtype;                 ///< Used for livery refits (NewGRF variations)
	uint16 cargo_cap;                   ///< total capacity
	VehicleCargoList cargo;             ///< The cargo this vehicle is carrying

	byte day_counter;                   ///< Increased by one for each day
	byte tick_counter;                  ///< Increased by one for each tick
	byte running_ticks;                 ///< Number of ticks this vehicle was not stopped this day

	byte vehstatus;                     ///< Status
	Order current_order;                ///< The current order (+ status, like: loading)
	VehicleOrderID cur_order_index;     ///< The index to the current order

	union {
		OrderList *list;            ///< Pointer to the order list for this vehicle
		Order     *old;             ///< Only used during conversion of old save games
	} orders;

	byte vehicle_flags;                 ///< Used for gradual loading and other miscellaneous things (@see VehicleFlags enum)

	/** Ticks to wait before starting next cycle. */
	uint16 load_unload_ticks;

	GroupID group_id;                   ///< Index of group Pool array

	byte subtype;                       ///< subtype (Filled with values from #EffectVehicles/#TrainSubTypes/#AircraftSubTypes)

	NewGRFCache grf_cache;              ///< Cache of often used calculated NewGRF values
	VehicleCache vcache;                ///< Cache of often used vehicle values.

	/** Create a new vehicle */
	Vehicle(VehicleType type = VEH_INVALID);

	/** Destroy all stuff that (still) needs the virtual functions to work properly */
	void PreDestructor();
	/** We want to 'destruct' the right class. */
	virtual ~Vehicle();

	void BeginLoading();
	void LeaveStation();

	GroundVehicleCache *GetGroundVehicleCache();
	const GroundVehicleCache *GetGroundVehicleCache() const;

	void DeleteUnreachedAutoOrders();

	/**
	 * Handle the loading of the vehicle; when not it skips through dummy
	 * orders and does nothing in all other cases.
	 * @param mode is the non-first call for this vehicle in this tick?
	 */
	void HandleLoading(bool mode = false);

	/**
	 * Get a string 'representation' of the vehicle type.
	 * @return the string representation.
	 */
	virtual const char *GetTypeString() const { return "base vehicle"; }

	/**
	 * Marks the vehicles to be redrawn and updates cached variables
	 *
	 * This method marks the area of the vehicle on the screen as dirty.
	 * It can be use to repaint the vehicle.
	 *
	 * @ingroup dirty
	 */
	virtual void MarkDirty() {}

	/**
	 * Updates the x and y offsets and the size of the sprite used
	 * for this vehicle.
	 * @param direction the direction the vehicle is facing
	 */
	virtual void UpdateDeltaXY(Direction direction) {}

	/**
	 * Determines the effective direction-specific vehicle movement speed.
	 *
	 * This method belongs to the old vehicle movement method:
	 * A vehicle moves a step every 256 progress units.
	 * The vehicle speed is scaled by 3/4 when moving in X or Y direction due to the longer distance.
	 *
	 * However, this method is slightly wrong in corners, as the leftover progress is not scaled correctly
	 * when changing movement direction. #GetAdvanceSpeed() and #GetAdvanceDistance() are better wrt. this.
	 *
	 * @param speed Direction-independent unscaled speed.
	 * @return speed scaled by movement direction. 256 units are required for each movement step.
	 */
	FORCEINLINE uint GetOldAdvanceSpeed(uint speed)
	{
		return (this->direction & 1) ? speed : speed * 3 / 4;
	}

	/**
	 * Determines the effective vehicle movement speed.
	 *
	 * Together with #GetAdvanceDistance() this function is a replacement for #GetOldAdvanceSpeed().
	 *
	 * A vehicle progresses independent of it's movement direction.
	 * However different amounts of "progress" are needed for moving a step in a specific direction.
	 * That way the leftover progress does not need any adaption when changing movement direction.
	 *
	 * @param speed Direction-independent unscaled speed.
	 * @return speed, scaled to match #GetAdvanceDistance().
	 */
	static FORCEINLINE uint GetAdvanceSpeed(uint speed)
	{
		return speed * 3 / 4;
	}

	/**
	 * Determines the vehicle "progress" needed for moving a step.
	 *
	 * Together with #GetAdvanceSpeed() this function is a replacement for #GetOldAdvanceSpeed().
	 *
	 * @return distance to drive for a movement step on the map.
	 */
	FORCEINLINE uint GetAdvanceDistance()
	{
		return (this->direction & 1) ? 192 : 256;
	}

	/**
	 * Sets the expense type associated to this vehicle type
	 * @param income whether this is income or (running) expenses of the vehicle
	 */
	virtual ExpensesType GetExpenseType(bool income) const { return EXPENSES_OTHER; }

	/**
	 * Play the sound associated with leaving the station
	 */
	virtual void PlayLeaveStationSound() const {}

	/**
	 * Whether this is the primary vehicle in the chain.
	 */
	virtual bool IsPrimaryVehicle() const { return false; }

	/**
	 * Gets the sprite to show for the given direction
	 * @param direction the direction the vehicle is facing
	 * @return the sprite for the given vehicle in the given direction
	 */
	virtual SpriteID GetImage(Direction direction) const { return 0; }

	/**
	 * Invalidates cached NewGRF variables
	 * @see InvalidateNewGRFCacheOfChain
	 */
	FORCEINLINE void InvalidateNewGRFCache()
	{
		this->grf_cache.cache_valid = 0;
	}

	/**
	 * Invalidates cached NewGRF variables of all vehicles in the chain (after the current vehicle)
	 * @see InvalidateNewGRFCache
	 */
	FORCEINLINE void InvalidateNewGRFCacheOfChain()
	{
		for (Vehicle *u = this; u != NULL; u = u->Next()) {
			u->InvalidateNewGRFCache();
		}
	}

	/**
	 * Check if the vehicle is a ground vehicle.
	 * @return True iff the vehicle is a train or a road vehicle.
	 */
	FORCEINLINE bool IsGroundVehicle() const
	{
		return this->type == VEH_TRAIN || this->type == VEH_ROAD;
	}

	/**
	 * Gets the speed in km-ish/h that can be sent into SetDParam for string processing.
	 * @return the vehicle's speed
	 */
	virtual int GetDisplaySpeed() const { return 0; }

	/**
	 * Gets the maximum speed in km-ish/h that can be sent into SetDParam for string processing.
	 * @return the vehicle's maximum speed
	 */
	virtual int GetDisplayMaxSpeed() const { return 0; }

	/**
	 * Gets the running cost of a vehicle
	 * @return the vehicle's running cost
	 */
	virtual Money GetRunningCost() const { return 0; }

	/**
	 * Check whether the vehicle is in the depot.
	 * @return true if and only if the vehicle is in the depot.
	 */
	virtual bool IsInDepot() const { return false; }

	/**
	 * Check whether the vehicle is in the depot *and* stopped.
	 * @return true if and only if the vehicle is in the depot and stopped.
	 */
	virtual bool IsStoppedInDepot() const { return this->IsInDepot() && (this->vehstatus & VS_STOPPED) != 0; }

	/**
	 * Calls the tick handler of the vehicle
	 * @return is this vehicle still valid?
	 */
	virtual bool Tick() { return true; };

	/**
	 * Calls the new day handler of the vehicle
	 */
	virtual void OnNewDay() {};

	/**
	 * Crash the (whole) vehicle chain.
	 * @param flooded whether the cause of the crash is flooding or not.
	 * @return the number of lost souls.
	 */
	virtual uint Crash(bool flooded = false);

	/**
	 * Returns the Trackdir on which the vehicle is currently located.
	 * Works for trains and ships.
	 * Currently works only sortof for road vehicles, since they have a fuzzy
	 * concept of being "on" a trackdir. Dunno really what it returns for a road
	 * vehicle that is halfway a tile, never really understood that part. For road
	 * vehicles that are at the beginning or end of the tile, should just return
	 * the diagonal trackdir on which they are driving. I _think_.
	 * For other vehicles types, or vehicles with no clear trackdir (such as those
	 * in depots), returns 0xFF.
	 * @return the trackdir of the vehicle
	 */
	virtual Trackdir GetVehicleTrackdir() const { return INVALID_TRACKDIR; }

	/**
	 * Gets the running cost of a vehicle  that can be sent into SetDParam for string processing.
	 * @return the vehicle's running cost
	 */
	Money GetDisplayRunningCost() const { return (this->GetRunningCost() >> 8); }

	/**
	 * Gets the profit vehicle had this year. It can be sent into SetDParam for string processing.
	 * @return the vehicle's profit this year
	 */
	Money GetDisplayProfitThisYear() const { return (this->profit_this_year >> 8); }

	/**
	 * Gets the profit vehicle had last year. It can be sent into SetDParam for string processing.
	 * @return the vehicle's profit last year
	 */
	Money GetDisplayProfitLastYear() const { return (this->profit_last_year >> 8); }

	/**
	 * Set the next vehicle of this vehicle.
	 * @param next the next vehicle. NULL removes the next vehicle.
	 */
	void SetNext(Vehicle *next);

	/**
	 * Get the next vehicle of this vehicle.
	 * @note articulated parts are also counted as vehicles.
	 * @return the next vehicle or NULL when there isn't a next vehicle.
	 */
	inline Vehicle *Next() const { return this->next; }

	/**
	 * Get the previous vehicle of this vehicle.
	 * @note articulated parts are also counted as vehicles.
	 * @return the previous vehicle or NULL when there isn't a previous vehicle.
	 */
	inline Vehicle *Previous() const { return this->previous; }

	/**
	 * Get the first vehicle of this vehicle chain.
	 * @return the first vehicle of the chain.
	 */
	inline Vehicle *First() const { return this->first; }

	/**
	 * Get the last vehicle of this vehicle chain.
	 * @return the last vehicle of the chain.
	 */
	inline Vehicle *Last()
	{
		Vehicle *v = this;
		while (v->Next() != NULL) v = v->Next();
		return v;
	}

	/**
	 * Get the last vehicle of this vehicle chain.
	 * @return the last vehicle of the chain.
	 */
	inline const Vehicle *Last() const
	{
		const Vehicle *v = this;
		while (v->Next() != NULL) v = v->Next();
		return v;
	}

	/**
	 * Get the first order of the vehicles order list.
	 * @return first order of order list.
	 */
	inline Order *GetFirstOrder() const { return (this->orders.list == NULL) ? NULL : this->orders.list->GetFirstOrder(); }

	/**
	 * Adds this vehicle to a shared vehicle chain.
	 * @param shared_chain a vehicle of the chain with shared vehicles.
	 * @pre !this->IsOrderListShared()
	 */
	void AddToShared(Vehicle *shared_chain);

	/**
	 * Removes the vehicle from the shared order list.
	 */
	void RemoveFromShared();

	/**
	 * Get the next vehicle of the shared vehicle chain.
	 * @return the next shared vehicle or NULL when there isn't a next vehicle.
	 */
	inline Vehicle *NextShared() const { return this->next_shared; }

	/**
	 * Get the previous vehicle of the shared vehicle chain
	 * @return the previous shared vehicle or NULL when there isn't a previous vehicle.
	 */
	inline Vehicle *PreviousShared() const { return this->previous_shared; }

	/**
	 * Get the first vehicle of this vehicle chain.
	 * @return the first vehicle of the chain.
	 */
	inline Vehicle *FirstShared() const { return (this->orders.list == NULL) ? this->First() : this->orders.list->GetFirstSharedVehicle(); }

	/**
	 * Check if we share our orders with another vehicle.
	 * @return true if there are other vehicles sharing the same order
	 */
	inline bool IsOrderListShared() const { return this->orders.list != NULL && this->orders.list->IsShared(); }

	/**
	 * Get the number of orders this vehicle has.
	 * @return the number of orders this vehicle has.
	 */
	inline VehicleOrderID GetNumOrders() const { return (this->orders.list == NULL) ? 0 : this->orders.list->GetNumOrders(); }

	/**
	 * Get the number of manually added orders this vehicle has.
	 * @return the number of manually added orders this vehicle has.
	 */
	inline VehicleOrderID GetNumManualOrders() const { return (this->orders.list == NULL) ? 0 : this->orders.list->GetNumManualOrders(); }

	/**
	 * Copy certain configurations and statistics of a vehicle after successful autoreplace/renew
	 * The function shall copy everything that cannot be copied by a command (like orders / group etc),
	 * and that shall not be resetted for the new vehicle.
	 * @param src The old vehicle
	 */
	inline void CopyVehicleConfigAndStatistics(const Vehicle *src)
	{
		this->unitnumber = src->unitnumber;

		this->cur_order_index = src->cur_order_index;
		this->current_order = src->current_order;
		this->dest_tile  = src->dest_tile;

		this->profit_this_year = src->profit_this_year;
		this->profit_last_year = src->profit_last_year;

		this->current_order_time = src->current_order_time;
		this->lateness_counter = src->lateness_counter;
		this->timetable_start = src->timetable_start;

		if (HasBit(src->vehicle_flags, VF_TIMETABLE_STARTED)) SetBit(this->vehicle_flags, VF_TIMETABLE_STARTED);
		if (HasBit(src->vehicle_flags, VF_AUTOFILL_TIMETABLE)) SetBit(this->vehicle_flags, VF_AUTOFILL_TIMETABLE);
		if (HasBit(src->vehicle_flags, VF_AUTOFILL_PRES_WAIT_TIME)) SetBit(this->vehicle_flags, VF_AUTOFILL_PRES_WAIT_TIME);

		this->service_interval = src->service_interval;
	}


	/**
	 * Handle all of the aspects of a vehicle breakdown
	 * This includes adding smoke and sounds, and ending the breakdown when appropriate.
	 * @return true iff the vehicle is stopped because of a breakdown
	 * @note This function always returns false for aircraft, since these never stop for breakdowns
	 */
	bool HandleBreakdown();

	bool NeedsAutorenewing(const Company *c) const;

	/**
	 * Check if the vehicle needs to go to a depot in near future (if a opportunity presents itself) for service or replacement.
	 *
	 * @see NeedsAutomaticServicing()
	 * @return true if the vehicle should go to a depot if a opportunity presents itself.
	 */
	bool NeedsServicing() const;

	/**
	 * Checks if the current order should be interupted for a service-in-depot-order.
	 * @see NeedsServicing()
	 * @return true if the current order should be interupted.
	 */
	bool NeedsAutomaticServicing() const;

	/**
	 * Determine the location for the station where the vehicle goes to next.
	 * Things done for example are allocating slots in a road stop or exact
	 * location of the platform is determined for ships.
	 * @param station the station to make the next location of the vehicle.
	 * @return the location (tile) to aim for.
	 */
	virtual TileIndex GetOrderStationLocation(StationID station) { return INVALID_TILE; }

	/**
	 * Find the closest depot for this vehicle and tell us the location,
	 * DestinationID and whether we should reverse.
	 * @param location    where do we go to?
	 * @param destination what hangar do we go to?
	 * @param reverse     should the vehicle be reversed?
	 * @return true if a depot could be found.
	 */
	virtual bool FindClosestDepot(TileIndex *location, DestinationID *destination, bool *reverse) { return false; }

	/**
	 * Send this vehicle to the depot using the given command(s).
	 * @param flags   the command flags (like execute and such).
	 * @param command the command to execute.
	 * @return the cost of the depot action.
	 */
	CommandCost SendToDepot(DoCommandFlag flags, DepotCommand command);

	/**
	 * Update the cached visual effect.
	 * @param allow_power_change true if the wagon-is-powered-state may change.
	 */
	void UpdateVisualEffect(bool allow_power_change = true);

	/*
	 * Draw visual effects (smoke and/or sparks) for a vehicle chain.
	 * @pre this->IsPrimaryVehicle()
	 */
	void ShowVisualEffect() const;

	/**
	 * Increments cur_order_index, keeps care of the wrap-around and invalidates the GUI.
	 * Note: current_order is not invalidated.
	 */
	void IncrementOrderIndex()
	{
		this->cur_order_index++;
		if (this->cur_order_index >= this->GetNumOrders()) this->cur_order_index = 0;
		InvalidateVehicleOrder(this, 0);
	}

	/**
	 * Returns order 'index' of a vehicle or NULL when it doesn't exists
	 * @param index the order to fetch
	 * @return the found (or not) order
	 */
	inline Order *GetOrder(int index) const
	{
		return (this->orders.list == NULL) ? NULL : this->orders.list->GetOrderAt(index);
	}

	Order *GetNextManualOrder(int index) const;

	/**
	 * Returns the last order of a vehicle, or NULL if it doesn't exists
	 * @return last order of a vehicle, if available
	 */
	inline Order *GetLastOrder() const
	{
		return (this->orders.list == NULL) ? NULL : this->orders.list->GetLastOrder();
	}

	bool IsEngineCountable() const;
	bool HasDepotOrder() const;
	void HandlePathfindingResult(bool path_found);
};

#define FOR_ALL_VEHICLES_FROM(var, start) FOR_ALL_ITEMS_FROM(Vehicle, vehicle_index, var, start)
#define FOR_ALL_VEHICLES(var) FOR_ALL_VEHICLES_FROM(var, 0)

/**
 * Class defining several overloaded accessors so we don't
 * have to cast vehicle types that often
 */
template <class T, VehicleType Type>
struct SpecializedVehicle : public Vehicle {
	static const VehicleType EXPECTED_TYPE = Type; ///< Specialized type

	/**
	 * Set vehicle type correctly
	 */
	FORCEINLINE SpecializedVehicle<T, Type>() : Vehicle(Type) { }

	/**
	 * Get the first vehicle in the chain
	 * @return first vehicle in the chain
	 */
	FORCEINLINE T *First() const { return (T *)this->Vehicle::First(); }

	/**
	 * Get the last vehicle in the chain
	 * @return last vehicle in the chain
	 */
	FORCEINLINE T *Last() { return (T *)this->Vehicle::Last(); }

	/**
	 * Get the last vehicle in the chain
	 * @return last vehicle in the chain
	 */
	FORCEINLINE const T *Last() const { return (const T *)this->Vehicle::Last(); }

	/**
	 * Get next vehicle in the chain
	 * @return next vehicle in the chain
	 */
	FORCEINLINE T *Next() const { return (T *)this->Vehicle::Next(); }

	/**
	 * Get previous vehicle in the chain
	 * @return previous vehicle in the chain
	 */
	FORCEINLINE T *Previous() const { return (T *)this->Vehicle::Previous(); }


	/**
	 * Tests whether given index is a valid index for vehicle of this type
	 * @param index tested index
	 * @return is this index valid index of T?
	 */
	static FORCEINLINE bool IsValidID(size_t index)
	{
		return Vehicle::IsValidID(index) && Vehicle::Get(index)->type == Type;
	}

	/**
	 * Gets vehicle with given index
	 * @return pointer to vehicle with given index casted to T *
	 */
	static FORCEINLINE T *Get(size_t index)
	{
		return (T *)Vehicle::Get(index);
	}

	/**
	 * Returns vehicle if the index is a valid index for this vehicle type
	 * @return pointer to vehicle with given index if it's a vehicle of this type
	 */
	static FORCEINLINE T *GetIfValid(size_t index)
	{
		return IsValidID(index) ? Get(index) : NULL;
	}

	/**
	 * Converts a Vehicle to SpecializedVehicle with type checking.
	 * @param v Vehicle pointer
	 * @return pointer to SpecializedVehicle
	 */
	static FORCEINLINE T *From(Vehicle *v)
	{
		assert(v->type == Type);
		return (T *)v;
	}

	/**
	 * Converts a const Vehicle to const SpecializedVehicle with type checking.
	 * @param v Vehicle pointer
	 * @return pointer to SpecializedVehicle
	 */
	static FORCEINLINE const T *From(const Vehicle *v)
	{
		assert(v->type == Type);
		return (const T *)v;
	}

	/**
	 * Update vehicle sprite- and position caches
	 * @param moved Was the vehicle moved?
	 * @param turned Did the vehicle direction change?
	 */
	FORCEINLINE void UpdateViewport(bool moved, bool turned)
	{
		extern void VehicleMove(Vehicle *v, bool update_viewport);

		/* Explicitly choose method to call to prevent vtable dereference -
		 * it gives ~3% runtime improvements in games with many vehicles */
		if (turned) ((T *)this)->T::UpdateDeltaXY(this->direction);
		SpriteID old_image = this->cur_image;
		this->cur_image = ((T *)this)->T::GetImage(this->direction);
		if (moved || this->cur_image != old_image) VehicleMove(this, true);
	}
};

#define FOR_ALL_VEHICLES_OF_TYPE(name, var) FOR_ALL_ITEMS_FROM(name, vehicle_index, var, 0) if (var->type == name::EXPECTED_TYPE)

/**
 * Disasters, like submarines, skyrangers and their shadows, belong to this class.
 */
struct DisasterVehicle : public SpecializedVehicle<DisasterVehicle, VEH_DISASTER> {
	uint16 image_override;
	VehicleID big_ufo_destroyer_target;

	/** We don't want GCC to zero our struct! It already is zeroed and has an index! */
	DisasterVehicle() : SpecializedVehicle<DisasterVehicle, VEH_DISASTER>() {}
	/** We want to 'destruct' the right class. */
	virtual ~DisasterVehicle() {}

	const char *GetTypeString() const { return "disaster vehicle"; }
	void UpdateDeltaXY(Direction direction);
	bool Tick();
};

#define FOR_ALL_DISASTERVEHICLES(var) FOR_ALL_VEHICLES_OF_TYPE(DisasterVehicle, var)

/** Generates sequence of free UnitID numbers */
struct FreeUnitIDGenerator {
	bool *cache;  ///< array of occupied unit id numbers
	UnitID maxid; ///< maximum ID at the moment of constructor call
	UnitID curid; ///< last ID returned; 0 if none

	/**
	 * Initializes the structure. Vehicle unit numbers are supposed not to change after
	 * struct initialization, except after each call to this->NextID() the returned value
	 * is assigned to a vehicle.
	 * @param type type of vehicle
	 * @param owner owner of vehicles
	 */
	FreeUnitIDGenerator(VehicleType type, CompanyID owner);

	/** Returns next free UnitID. Supposes the last returned value was assigned to a vehicle. */
	UnitID NextID();

	/** Releases allocated memory */
	~FreeUnitIDGenerator() { free(this->cache); }
};

static const int32 INVALID_COORD = 0x7fffffff;

#endif /* VEHICLE_BASE_H */
