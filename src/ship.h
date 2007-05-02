/* $Id$ */

/** @file ship.h */

#ifndef SHIP_H
#define SHIP_H

#include "vehicle.h"

void CcBuildShip(bool success, TileIndex tile, uint32 p1, uint32 p2);
void CcCloneShip(bool success, TileIndex tile, uint32 p1, uint32 p2);
void RecalcShipStuff(Vehicle *v);
void GetShipSpriteSize(EngineID engine, uint &width, uint &height);

static inline bool IsShipInDepot(const Vehicle* v)
{
	assert(v->type == VEH_SHIP);
	return v->u.ship.state == 0x80;
}

static inline bool IsShipInDepotStopped(const Vehicle* v)
{
	return IsShipInDepot(v) && v->vehstatus & VS_STOPPED;
}


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
	virtual ~Ship() {}

	const char *GetTypeString() { return "ship"; }
	void MarkDirty();
	void UpdateDeltaXY(Direction direction);
	ExpensesType GetExpenseType(bool income) { return income ? EXPENSES_SHIP_INC : EXPENSES_SHIP_RUN; }
	WindowClass GetVehicleListWindowClass() { return WC_SHIPS_LIST; }
};

#endif /* SHIP_H */
