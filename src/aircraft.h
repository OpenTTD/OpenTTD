/* $Id$ */

/** @file aircraft.h */

#ifndef AIRCRAFT_H
#define AIRCRAFT_H

#include "station_map.h"
#include "vehicle_base.h"
#include "engine.h"

/** An aircraft can be one ot those types */
enum AircraftSubType {
	AIR_HELICOPTER = 0, ///< an helicopter
	AIR_AIRCRAFT   = 2, ///< an airplane
	AIR_SHADOW     = 4, ///< shadow of the aircraft
	AIR_ROTOR      = 6  ///< rotor of an helicopter
};


/** Check if the aircraft type is a normal flying device; eg
 * not a rotor or a shadow
 * @param v vehicle to check
 * @return Returns true if the aircraft is a helicopter/airplane and
 * false if it is a shadow or a rotor) */
static inline bool IsNormalAircraft(const Vehicle *v)
{
	assert(v->type == VEH_AIRCRAFT);
	/* To be fully correct the commented out functionality is the proper one,
	 * but since value can only be 0 or 2, it is sufficient to only check <= 2
	 * return (v->subtype == AIR_HELICOPTER) || (v->subtype == AIR_AIRCRAFT); */
	return v->subtype <= AIR_AIRCRAFT;
}

/** Checks if an aircraft can use the station in question
 * @param engine The engine to test
 * @param st The station
 * @return true if the aircraft can use the station
 */
static inline bool CanAircraftUseStation(EngineID engine, const Station *st)
{
	const AirportFTAClass *apc = st->Airport();
	const AircraftVehicleInfo *avi = AircraftVehInfo(engine);

	return (apc->flags & (avi->subtype & AIR_CTOL ? AirportFTAClass::AIRPLANES : AirportFTAClass::HELICOPTERS)) != 0;
}

/** Checks if an aircraft can use the station at the tile in question
 * @param engine The engine to test
 * @param tile The tile where the station is
 * @return true if the aircraft can use the station
 */
static inline bool CanAircraftUseStation(EngineID engine, TileIndex tile)
{
	return CanAircraftUseStation(engine, GetStationByTile(tile));
}

/**
 * Calculates cargo capacity based on an aircraft's passenger
 * and mail capacities.
 * @param cid Which cargo type to calculate a capacity for.
 * @param avi Which engine to find a cargo capacity for.
 * @return New cargo capacity value.
 */
uint16 AircraftDefaultCargoCapacity(CargoID cid, const AircraftVehicleInfo *avi);

/**
 * This is the Callback method after the construction attempt of an aircraft
 * @param success indicates completion (or not) of the operation
 * @param tile of depot where aircraft is built
 * @param p1 unused
 * @param p2 unused
 */
void CcBuildAircraft(bool success, TileIndex tile, uint32 p1, uint32 p2);

/** Handle Aircraft specific tasks when a an Aircraft enters a hangar
 * @param *v Vehicle that enters the hangar
 */
void HandleAircraftEnterHangar(Vehicle *v);

/** Get the size of the sprite of an aircraft sprite heading west (used for lists)
 * @param engine The engine to get the sprite from
 * @param width The width of the sprite
 * @param height The height of the sprite
 */
void GetAircraftSpriteSize(EngineID engine, uint &width, uint &height);

/**
 * Updates the status of the Aircraft heading or in the station
 * @param st Station been updated
 */
void UpdateAirplanesOnNewStation(const Station *st);

/** Update cached values of an aircraft.
 * Currently caches callback 36 max speed.
 * @param v Vehicle
 */
void UpdateAircraftCache(Vehicle *v);

/**
 * This class 'wraps' Vehicle; you do not actually instantiate this class.
 * You create a Vehicle using AllocateVehicle, so it is added to the pool
 * and you reinitialize that to a Train using:
 *   v = new (v) Aircraft();
 *
 * As side-effect the vehicle type is set correctly.
 */
struct Aircraft : public Vehicle {
	/** Initializes the Vehicle to an aircraft */
	Aircraft() { this->type = VEH_AIRCRAFT; }

	/** We want to 'destruct' the right class. */
	virtual ~Aircraft() { this->PreDestructor(); }

	const char *GetTypeString() const { return "aircraft"; }
	void MarkDirty();
	void UpdateDeltaXY(Direction direction);
	ExpensesType GetExpenseType(bool income) const { return income ? EXPENSES_AIRCRAFT_INC : EXPENSES_AIRCRAFT_RUN; }
	WindowClass GetVehicleListWindowClass() const { return WC_AIRCRAFT_LIST; }
	bool IsPrimaryVehicle() const { return IsNormalAircraft(this); }
	int GetImage(Direction direction) const;
	int GetDisplaySpeed() const { return this->cur_speed * 10 / 16; }
	int GetDisplayMaxSpeed() const { return this->max_speed * 10 / 16; }
	Money GetRunningCost() const { return AircraftVehInfo(this->engine_type)->running_cost * _price.aircraft_running; }
	bool IsInDepot() const { return (this->vehstatus & VS_HIDDEN) != 0 && IsHangarTile(this->tile); }
	void Tick();
	void OnNewDay();
};

#endif /* AIRCRAFT_H */
