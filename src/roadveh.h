/* $Id$ */

/** @file src/roadveh.h Road vehicle states */

#ifndef ROADVEH_H
#define ROADVEH_H

#include "vehicle_base.h"
#include "engine.h"
#include "economy_func.h"

enum RoadVehicleSubType {
	RVST_FRONT,
	RVST_ARTIC_PART,
};

static inline bool IsRoadVehFront(const Vehicle *v)
{
	assert(v->type == VEH_ROAD);
	return v->subtype == RVST_FRONT;
}

static inline void SetRoadVehFront(Vehicle *v)
{
	assert(v->type == VEH_ROAD);
	v->subtype = RVST_FRONT;
}

static inline bool IsRoadVehArticPart(const Vehicle *v)
{
	assert(v->type == VEH_ROAD);
	return v->subtype == RVST_ARTIC_PART;
}

static inline void SetRoadVehArticPart(Vehicle *v)
{
	assert(v->type == VEH_ROAD);
	v->subtype = RVST_ARTIC_PART;
}

static inline bool RoadVehHasArticPart(const Vehicle *v)
{
	assert(v->type == VEH_ROAD);
	return v->Next() != NULL && IsRoadVehArticPart(v->Next());
}


void CcBuildRoadVeh(bool success, TileIndex tile, uint32 p1, uint32 p2);


/**
 * This class 'wraps' Vehicle; you do not actually instantiate this class.
 * You create a Vehicle using AllocateVehicle, so it is added to the pool
 * and you reinitialize that to a Train using:
 *   v = new (v) RoadVehicle();
 *
 * As side-effect the vehicle type is set correctly.
 */
struct RoadVehicle : public Vehicle {
	/** Initializes the Vehicle to a road vehicle */
	RoadVehicle() { this->type = VEH_ROAD; }

	/** We want to 'destruct' the right class. */
	virtual ~RoadVehicle() { this->PreDestructor(); }

	const char *GetTypeString() const { return "road vehicle"; }
	void MarkDirty();
	void UpdateDeltaXY(Direction direction);
	ExpensesType GetExpenseType(bool income) const { return income ? EXPENSES_ROADVEH_INC : EXPENSES_ROADVEH_RUN; }
	WindowClass GetVehicleListWindowClass() const { return WC_ROADVEH_LIST; }
	bool IsPrimaryVehicle() const { return IsRoadVehFront(this); }
	int GetImage(Direction direction) const;
	int GetDisplaySpeed() const { return this->cur_speed * 10 / 32; }
	int GetDisplayMaxSpeed() const { return this->max_speed * 10 / 32; }
	Money GetRunningCost() const { return RoadVehInfo(this->engine_type)->running_cost * _price.roadveh_running; }
	bool IsInDepot() const { return this->u.road.state == RVSB_IN_DEPOT; }
	void Tick();
};

byte GetRoadVehLength(const Vehicle *v);

void RoadVehUpdateCache(Vehicle *v);

#endif /* ROADVEH_H */
