/* $Id$ */

/** @file train.h */

#ifndef TRAIN_H
#define TRAIN_H

#include "stdafx.h"
#include "vehicle.h"


/*
 * enum to handle train subtypes
 * Do not access it directly unless you have to. Use the access functions below
 * This is an enum to tell what bit to access as it is a bitmask
 */

enum TrainSubtype {
	Train_Front             = 0, ///< Leading engine of a train
	Train_Articulated_Part  = 1, ///< Articulated part of an engine
	Train_Wagon             = 2, ///< Wagon
	Train_Engine            = 3, ///< Engine, that can be front engines, but might be placed behind another engine
	Train_Free_Wagon        = 4, ///< First in a wagon chain (in depot)
	Train_Multiheaded       = 5, ///< Engine is a multiheaded
};


/** Check if a vehicle is front engine
 * @param v vehicle to check
 * @return Returns true if vehicle is a front engine
 */
static inline bool IsFrontEngine(const Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	return HASBIT(v->subtype, Train_Front);
}

/** Set front engine state
 * @param v vehicle to change
 */
static inline void SetFrontEngine(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	SETBIT(v->subtype, Train_Front);
}

/** Remove the front engine state
 * @param v vehicle to change
 */
static inline void ClearFrontEngine(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	CLRBIT(v->subtype, Train_Front);
}

/** Check if a vehicle is an articulated part of an engine
 * @param v vehicle to check
 * @return Returns true if vehicle is an articulated part
 */
static inline bool IsArticulatedPart(const Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	return HASBIT(v->subtype, Train_Articulated_Part);
}

/** Set a vehicle to be an articulated part
 * @param v vehicle to change
 */
static inline void SetArticulatedPart(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	SETBIT(v->subtype, Train_Articulated_Part);
}

/** Clear a vehicle from being an articulated part
 * @param v vehicle to change
 */
static inline void ClearArticulatedPart(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	CLRBIT(v->subtype, Train_Articulated_Part);
}

/** Check if a vehicle is a wagon
 * @param v vehicle to check
 * @return Returns true if vehicle is a wagon
 */
static inline bool IsTrainWagon(const Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	return HASBIT(v->subtype, Train_Wagon);
}

/** Set a vehicle to be a wagon
 * @param v vehicle to change
 */
static inline void SetTrainWagon(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	SETBIT(v->subtype, Train_Wagon);
}

/** Clear wagon property
 * @param v vehicle to change
 */
static inline void ClearTrainWagon(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	CLRBIT(v->subtype, Train_Wagon);
}

/** Check if a vehicle is an engine (can be first in a train)
 * @param v vehicle to check
 * @return Returns true if vehicle is an engine
 */
static inline bool IsTrainEngine(const Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	return HASBIT(v->subtype, Train_Engine);
}

/** Set engine status
 * @param v vehicle to change
 */
static inline void SetTrainEngine(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	SETBIT(v->subtype, Train_Engine);
}

/** Clear engine status
 * @param v vehicle to change
 */
static inline void ClearTrainEngine(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	CLRBIT(v->subtype, Train_Engine);
}

/** Check if a vehicle is a free wagon (got no engine in front of it)
 * @param v vehicle to check
 * @return Returns true if vehicle is a free wagon
 */
static inline bool IsFreeWagon(const Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	return HASBIT(v->subtype, Train_Free_Wagon);
}

/** Set if a vehicle is a free wagon
 * @param v vehicle to change
 */
static inline void SetFreeWagon(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	SETBIT(v->subtype, Train_Free_Wagon);
}

/** Clear a vehicle from being a free wagon
 * @param v vehicle to change
 */
static inline void ClearFreeWagon(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	CLRBIT(v->subtype, Train_Free_Wagon);
}

/** Check if a vehicle is a multiheaded engine
 * @param v vehicle to check
 * @return Returns true if vehicle is a multiheaded engine
 */
static inline bool IsMultiheaded(const Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	return HASBIT(v->subtype, Train_Multiheaded);
}

/** Set if a vehicle is a multiheaded engine
 * @param v vehicle to change
 */
static inline void SetMultiheaded(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	SETBIT(v->subtype, Train_Multiheaded);
}

/** Clear multiheaded engine property
 * @param v vehicle to change
 */
static inline void ClearMultiheaded(Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	CLRBIT(v->subtype, Train_Multiheaded);
}

/** Check if an engine has an articulated part.
 * @param v Vehicle.
 * @return True if the engine has an articulated part.
 */
static inline bool EngineHasArticPart(const Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	return (v->next != NULL && IsArticulatedPart(v->next));
}

/**
 * Get the next part of a multi-part engine.
 * Will only work on a multi-part engine (EngineHasArticPart(v) == true),
 * Result is undefined for normal engine.
 */
static inline Vehicle *GetNextArticPart(const Vehicle *v)
{
	assert(EngineHasArticPart(v));
	return v->next;
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

/** Get the next real (non-articulated part) vehicle in the consist.
 * @param v Vehicle.
 * @return Next vehicle in the consist.
 */
static inline Vehicle *GetNextVehicle(const Vehicle *v)
{
	assert(v->type == VEH_TRAIN);
	while (EngineHasArticPart(v)) v = GetNextArticPart(v);

	/* v now contains the last artic part in the engine */
	return v->next;
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
	bool HasFront() const { return true; }
	int GetImage(Direction direction) const;
	int GetDisplaySpeed() const { return this->cur_speed * 10 / 16; }
	int GetDisplayMaxSpeed() const { return this->u.rail.cached_max_speed * 10 / 16; }
	Money GetRunningCost() const;
	bool IsInDepot() const { return CheckTrainInDepot(this, false) != -1; }
	bool IsStoppedInDepot() const { return CheckTrainStoppedInDepot(this) >= 0; }
	void Tick();
};

#endif /* TRAIN_H */
