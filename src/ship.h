/* $Id$ */

/** @file ship.h Base for ships. */

#ifndef SHIP_H
#define SHIP_H

#include "vehicle_base.h"
#include "engine_func.h"
#include "engine_base.h"
#include "economy_func.h"

void CcBuildShip(bool success, TileIndex tile, uint32 p1, uint32 p2);
void RecalcShipStuff(Vehicle *v);
void GetShipSpriteSize(EngineID engine, uint &width, uint &height);

/**
 * This class 'wraps' Vehicle; you do not actually instantiate this class.
 * You create a Vehicle using AllocateVehicle, so it is added to the pool
 * and you reinitialize that to a Train using:
 *   v = new (v) Ship();
 *
 * As side-effect the vehicle type is set correctly.
 */
struct Ship: public Vehicle {
	/** Initializes the Vehicle to a ship */
	Ship() { this->type = VEH_SHIP; }

	/** We want to 'destruct' the right class. */
	virtual ~Ship() { this->PreDestructor(); }

	const char *GetTypeString() const { return "ship"; }
	void MarkDirty();
	void UpdateDeltaXY(Direction direction);
	ExpensesType GetExpenseType(bool income) const { return income ? EXPENSES_SHIP_INC : EXPENSES_SHIP_RUN; }
	void PlayLeaveStationSound() const;
	bool IsPrimaryVehicle() const { return true; }
	SpriteID GetImage(Direction direction) const;
	int GetDisplaySpeed() const { return this->cur_speed * 10 / 32; }
	int GetDisplayMaxSpeed() const { return this->max_speed * 10 / 32; }
	Money GetRunningCost() const { return ShipVehInfo(this->engine_type)->running_cost * _price.ship_running; }
	bool IsInDepot() const { return this->u.ship.state == TRACK_BIT_DEPOT; }
	void Tick();
	void OnNewDay();
	TileIndex GetOrderStationLocation(StationID station);
	bool FindClosestDepot(TileIndex *location, DestinationID *destination, bool *reverse);
};

#endif /* SHIP_H */
