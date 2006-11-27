/* $Id$ */

#ifndef TRAIN_H
#define TRAIN_H

#include "stdafx.h"
#include "vehicle.h"


/*
 * enum to handle train subtypes
 * Do not access it directly unless you have to. Use the access functions below
 * This is an enum to tell what bit to access as it is a bitmask
 */

typedef enum TrainSubtypes {
	Train_Front             = 0, // Leading engine of a train
	Train_Articulated_Part  = 1, // Articulated part of an engine
	Train_Wagon             = 2, // Wagon
	Train_Engine            = 3, // Engine, that can be front engines, but might be placed behind another engine
	Train_Free_Wagon        = 4, // First in a wagon chain (in depot)
	Train_Multiheaded       = 5, // Engine is a multiheaded
} TrainSubtype;


/** Check if a vehicle is front engine
 * @param v vehicle to check
 * @return Returns true if vehicle is a front engine
 */
static inline bool IsFrontEngine(const Vehicle *v)
{
	return HASBIT(v->subtype, Train_Front);
}

/** Set front engine state
 * @param v vehicle to change
 */
static inline void SetFrontEngine(Vehicle *v)
{
	SETBIT(v->subtype, Train_Front);
}

/** Remove the front engine state
 * @param v vehicle to change
 */
static inline void ClearFrontEngine(Vehicle *v)
{
	CLRBIT(v->subtype, Train_Front);
}

/** Check if a vehicle is an articulated part of an engine
 * @param v vehicle to check
 * @return Returns true if vehicle is an articulated part
 */
static inline bool IsArticulatedPart(const Vehicle *v)
{
	return HASBIT(v->subtype, Train_Articulated_Part);
}

/** Set a vehicle to be an articulated part
 * @param v vehicle to change
 */
static inline void SetArticulatedPart(Vehicle *v)
{
	SETBIT(v->subtype, Train_Articulated_Part);
}

/** Clear a vehicle from being an articulated part
 * @param v vehicle to change
 */
static inline void ClearArticulatedPart(Vehicle *v)
{
	CLRBIT(v->subtype, Train_Articulated_Part);
}

/** Check if a vehicle is a wagon
 * @param v vehicle to check
 * @return Returns true if vehicle is a wagon
 */
static inline bool IsTrainWagon(const Vehicle *v)
{
	return HASBIT(v->subtype, Train_Wagon);
}

/** Set a vehicle to be a wagon
 * @param v vehicle to change
 */
static inline void SetTrainWagon(Vehicle *v)
{
	SETBIT(v->subtype, Train_Wagon);
}

/** Clear wagon property
 * @param v vehicle to change
 */
static inline void ClearTrainWagon(Vehicle *v)
{
	CLRBIT(v->subtype, Train_Wagon);
}

/** Check if a vehicle is an engine (can be first in a train)
 * @param v vehicle to check
 * @return Returns true if vehicle is an engine
 */
static inline bool IsTrainEngine(const Vehicle *v)
{
	return HASBIT(v->subtype, Train_Engine);
}

/** Set engine status
 * @param v vehicle to change
 */
static inline void SetTrainEngine(Vehicle *v)
{
	SETBIT(v->subtype, Train_Engine);
}

/** Clear engine status
 * @param v vehicle to change
 */
static inline void ClearTrainEngine(Vehicle *v)
{
	CLRBIT(v->subtype, Train_Engine);
}

/** Check if a vehicle is a free wagon (got no engine in front of it)
 * @param v vehicle to check
 * @return Returns true if vehicle is a free wagon
 */
static inline bool IsFreeWagon(const Vehicle *v)
{
	return HASBIT(v->subtype, Train_Free_Wagon);
}

/** Set if a vehicle is a free wagon
 * @param v vehicle to change
 */
static inline void SetFreeWagon(Vehicle *v)
{
	SETBIT(v->subtype, Train_Free_Wagon);
}

/** Clear a vehicle from being a free wagon
 * @param v vehicle to change
 */
static inline void ClearFreeWagon(Vehicle *v)
{
	CLRBIT(v->subtype, Train_Free_Wagon);
}

/** Check if a vehicle is a multiheaded engine
 * @param v vehicle to check
 * @return Returns true if vehicle is a multiheaded engine
 */
static inline bool IsMultiheaded(const Vehicle *v)
{
	return HASBIT(v->subtype, Train_Multiheaded);
}

/** Set if a vehicle is a multiheaded engine
 * @param v vehicle to change
 */
static inline void SetMultiheaded(Vehicle *v)
{
	SETBIT(v->subtype, Train_Multiheaded);
}

/** Clear multiheaded engine property
 * @param v vehicle to change
 */
static inline void ClearMultiheaded(Vehicle *v)
{
	CLRBIT(v->subtype, Train_Multiheaded);
}

/** Check if an engine has an articulated part.
 * @param v Vehicle.
 * @return True if the engine has an articulated part.
 */
static inline bool EngineHasArticPart(const Vehicle *v)
{
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
	while (EngineHasArticPart(v)) v = GetNextArticPart(v);
	return v;
}

/** Get the next real (non-articulated part) vehicle in the consist.
 * @param v Vehicle.
 * @return Next vehicle in the consist.
 */
static inline Vehicle *GetNextVehicle(const Vehicle *v)
{
	while (EngineHasArticPart(v)) v = GetNextArticPart(v);

	/* v now contains the last artic part in the engine */
	return v->next;
}

void ConvertOldMultiheadToNew(void);
void ConnectMultiheadedTrains(void);
uint CountArticulatedParts(EngineID engine_type);

int CheckTrainInDepot(const Vehicle *v, bool needs_to_be_stopped);
void CcCloneTrain(bool success, TileIndex tile, uint32 p1, uint32 p2);

byte FreightWagonMult(const Vehicle *v);

#endif /* TRAIN_H */
