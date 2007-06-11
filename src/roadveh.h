/* $Id$ */

/** @file src/roadveh.h Road vehicle states */

#ifndef ROADVEH_H
#define ROADVEH_H

#include "vehicle.h"


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
	return v->next != NULL && IsRoadVehArticPart(v->next);
}


static inline bool IsRoadVehInDepot(const Vehicle* v)
{
	assert(v->type == VEH_ROAD);
	return v->u.road.state == 254;
}

static inline bool IsRoadVehInDepotStopped(const Vehicle* v)
{
	return IsRoadVehInDepot(v) && v->vehstatus & VS_STOPPED;
}

void CcBuildRoadVeh(bool success, TileIndex tile, uint32 p1, uint32 p2);
void CcCloneRoadVeh(bool success, TileIndex tile, uint32 p1, uint32 p2);


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
	virtual ~RoadVehicle() {}

	const char *GetTypeString() const { return "road vehicle"; }
	void MarkDirty();
	void UpdateDeltaXY(Direction direction);
	ExpensesType GetExpenseType(bool income) const { return income ? EXPENSES_ROADVEH_INC : EXPENSES_ROADVEH_RUN; }
	WindowClass GetVehicleListWindowClass() const { return WC_ROADVEH_LIST; }
	bool IsPrimaryVehicle() const { return IsRoadVehFront(this); }
	bool HasFront() const { return true; }
};

byte GetRoadVehLength(const Vehicle *v);

void RoadVehUpdateCache(Vehicle *v);

#endif /* ROADVEH_H */
