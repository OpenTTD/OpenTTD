/* $Id$ */

/** @file train.h */

#ifndef TRAIN_H
#define TRAIN_H

#include "stdafx.h"
#include "core/bitmath_func.hpp"
#include "vehicle_base.h"


/** enum to handle train subtypes
 * Do not access it directly unless you have to. Use the access functions below
 * This is an enum to tell what bit to access as it is a bitmask
 */
enum TrainSubtype {
	TS_FRONT             = 0, ///< Leading engine of a train
	TS_ARTICULATED_PART  = 1, ///< Articulated part of an engine
	TS_WAGON             = 2, ///< Wagon
	TS_ENGINE            = 3, ///< Engine, that can be front engines, but might be placed behind another engine
	TS_FREE_WAGON        = 4, ///< First in a wagon chain (in depot)
	TS_MULTIHEADED       = 5, ///< Engine is a multiheaded
};


/** Check if a vehicle is front engine
 * @param v vehicle to check
 * @return Returns true if vehicle is a front engine
 */
static inline bool IsFrontEngine(const Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	return HasBit(v->subtype, TS_FRONT);
}

/** Set front engine state
 * @param v vehicle to change
 */
static inline void SetFrontEngine(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	SetBit(v->subtype, TS_FRONT);
}

/** Remove the front engine state
 * @param v vehicle to change
 */
static inline void ClearFrontEngine(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	ClrBit(v->subtype, TS_FRONT);
}

/** Check if a vehicle is an articulated part of an engine
 * @param v vehicle to check
 * @return Returns true if vehicle is an articulated part
 */
static inline bool IsArticulatedPart(const Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	return HasBit(v->subtype, TS_ARTICULATED_PART);
}

/** Set a vehicle to be an articulated part
 * @param v vehicle to change
 */
static inline void SetArticulatedPart(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	SetBit(v->subtype, TS_ARTICULATED_PART);
}

/** Clear a vehicle from being an articulated part
 * @param v vehicle to change
 */
static inline void ClearArticulatedPart(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	ClrBit(v->subtype, TS_ARTICULATED_PART);
}

/** Check if a vehicle is a wagon
 * @param v vehicle to check
 * @return Returns true if vehicle is a wagon
 */
static inline bool IsTrainWagon(const Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	return HasBit(v->subtype, TS_WAGON);
}

/** Set a vehicle to be a wagon
 * @param v vehicle to change
 */
static inline void SetTrainWagon(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	SetBit(v->subtype, TS_WAGON);
}

/** Clear wagon property
 * @param v vehicle to change
 */
static inline void ClearTrainWagon(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	ClrBit(v->subtype, TS_WAGON);
}

/** Check if a vehicle is an engine (can be first in a train)
 * @param v vehicle to check
 * @return Returns true if vehicle is an engine
 */
static inline bool IsTrainEngine(const Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	return HasBit(v->subtype, TS_ENGINE);
}

/** Set engine status
 * @param v vehicle to change
 */
static inline void SetTrainEngine(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	SetBit(v->subtype, TS_ENGINE);
}

/** Clear engine status
 * @param v vehicle to change
 */
static inline void ClearTrainEngine(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	ClrBit(v->subtype, TS_ENGINE);
}

/** Check if a vehicle is a free wagon (got no engine in front of it)
 * @param v vehicle to check
 * @return Returns true if vehicle is a free wagon
 */
static inline bool IsFreeWagon(const Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	return HasBit(v->subtype, TS_FREE_WAGON);
}

/** Set if a vehicle is a free wagon
 * @param v vehicle to change
 */
static inline void SetFreeWagon(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	SetBit(v->subtype, TS_FREE_WAGON);
}

/** Clear a vehicle from being a free wagon
 * @param v vehicle to change
 */
static inline void ClearFreeWagon(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	ClrBit(v->subtype, TS_FREE_WAGON);
}

/** Check if a vehicle is a multiheaded engine
 * @param v vehicle to check
 * @return Returns true if vehicle is a multiheaded engine
 */
static inline bool IsMultiheaded(const Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	return HasBit(v->subtype, TS_MULTIHEADED);
}

/** Set if a vehicle is a multiheaded engine
 * @param v vehicle to change
 */
static inline void SetMultiheaded(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	SetBit(v->subtype, TS_MULTIHEADED);
}

/** Clear multiheaded engine property
 * @param v vehicle to change
 */
static inline void ClearMultiheaded(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	ClrBit(v->subtype, TS_MULTIHEADED);
}

/** Check if an engine has an articulated part.
 * @param v Vehicle.
 * @return True if the engine has an articulated part.
 */
static inline bool EngineHasArticPart(const Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	return (v->Next() != NULL && IsArticulatedPart(v->Next()));
}

/**
 * Get the next part of a multi-part engine.
 * Will only work on a multi-part engine (EngineHasArticPart(v) == true),
 * Result is undefined for normal engine.
 */
static inline Vehicle *GetNextArticPart(const Vehicle *v)
{
	assert(EngineHasArticPart(v));
	return v->Next();
}

/** Get the last part of a multi-part engine.
 * @param v Vehicle.
 * @return Last part of the engine.
 */
static inline Vehicle *GetLastEnginePart(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	while (EngineHasArticPart(v)) v = GetNextArticPart(v);
	return v;
}

/** Tell if we are dealing with the rear end of a multiheaded engine.
 * @param v Vehicle.
 * @return True if the engine is the rear part of a dualheaded engine.
 */
static inline bool IsRearDualheaded(const Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	return (IsMultiheaded(v) && !IsTrainEngine(v));
}

/** Get the next real (non-articulated part) vehicle in the consist.
 * @param v Vehicle.
 * @return Next vehicle in the consist.
 */
static inline Vehicle *GetNextVehicle(const Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	while (EngineHasArticPart(v)) v = GetNextArticPart(v);

	/* v now contains the last artic part in the engine */
	return v->Next();
}

/** Get the next real (non-articulated part and non rear part of dualheaded engine) vehicle in the consist.
 * @param v Vehicle.
 * @return Next vehicle in the consist.
 */
static inline Vehicle *GetNextUnit(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	v = GetNextVehicle(v);
	if (v != NULL && IsRearDualheaded(v)) v = v->Next();

	return v;
}

void ConvertOldMultiheadToNew();
void ConnectMultiheadedTrains();
uint CountArticulatedParts(EngineID engine_type);

int CheckTrainInDepot(const Vehicle *v, bool needs_to_be_stopped);
void CcBuildLoco(bool success, TileIndex tile, uint32 p1, uint32 p2);
void CcBuildWagon(bool success, TileIndex tile, uint32 p1, uint32 p2);

byte FreightWagonMult(CargoID cargo);

int CheckTrainInDepot(const Vehicle *v, bool needs_to_be_stopped);
int CheckTrainStoppedInDepot(const Vehicle *v);
void UpdateTrainAcceleration(Vehicle* v);

/**
 * This class 'wraps' Vehicle; you do not actually instantiate this class.
 * You create a Vehicle using AllocateVehicle, so it is added to the pool
 * and you reinitialize that to a Train using:
 *   v = new (v) Train();
 *
 * As side-effect the vehicle type is set correctly.
 */
struct Train : public Vehicle {
	/** Initializes the Vehicle to a train */
	Train() { this->type = VEH_TRAIN; }

	/** We want to 'destruct' the right class. */
	virtual ~Train() { this->PreDestructor(); }

	const char *GetTypeString() const { return "train"; }
	void MarkDirty();
	void UpdateDeltaXY(Direction direction);
	ExpensesType GetExpenseType(bool income) const { return income ? EXPENSES_TRAIN_INC : EXPENSES_TRAIN_RUN; }
	WindowClass GetVehicleListWindowClass() const { return WC_TRAINS_LIST; }
	void PlayLeaveStationSound() const;
	bool IsPrimaryVehicle() const { return IsFrontEngine(this); }
	int GetImage(Direction direction) const;
	int GetDisplaySpeed() const { return this->u.rail.last_speed * 10 / 16; }
	int GetDisplayMaxSpeed() const { return this->u.rail.cached_max_speed * 10 / 16; }
	Money GetRunningCost() const;
	bool IsInDepot() const { return CheckTrainInDepot(this, false) != -1; }
	bool IsStoppedInDepot() const { return CheckTrainStoppedInDepot(this) >= 0; }
	void Tick();
	void OnNewDay();
	TileIndex GetOrderStationLocation(StationID station);
};

#endif /* TRAIN_H */
