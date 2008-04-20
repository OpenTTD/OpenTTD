/* $Id$ */

/** @file effectvehicle_base.h Base class for all effect vehicles. */

#ifndef EFFECTVEHICLE_BASE_H
#define EFFECTVEHICLE_BASE_H

#include "vehicle_base.h"

/**
 * This class 'wraps' Vehicle; you do not actually instantiate this class.
 * You create a Vehicle using AllocateVehicle, so it is added to the pool
 * and you reinitialize that to a Train using:
 *   v = new (v) Train();
 *
 * As side-effect the vehicle type is set correctly.
 *
 * A special vehicle is one of the following:
 *  - smoke
 *  - electric sparks for trains
 *  - explosions
 *  - bulldozer (road works)
 *  - bubbles (industry)
 */
struct EffectVehicle : public Vehicle {
	/** Initializes the Vehicle to a special vehicle */
	EffectVehicle() { this->type = VEH_EFFECT; }

	/** We want to 'destruct' the right class. */
	virtual ~EffectVehicle() {}

	const char *GetTypeString() const { return "special vehicle"; }
	void UpdateDeltaXY(Direction direction);
	void Tick();
};

#endif /* EFFECTVEHICLE_BASE_H */
