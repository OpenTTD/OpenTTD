/* $Id$ */

/** @file effectvehicle_base.h Base class for all effect vehicles. */

#ifndef EFFECTVEHICLE_BASE_H
#define EFFECTVEHICLE_BASE_H

#include "vehicle_base.h"

/**
 * A special vehicle is one of the following:
 *  - smoke
 *  - electric sparks for trains
 *  - explosions
 *  - bulldozer (road works)
 *  - bubbles (industry)
 */
struct EffectVehicle : public SpecializedVehicle<EffectVehicle, VEH_EFFECT> {
	uint16 animation_state;
	byte animation_substate;

	/** Initializes the Vehicle to a special vehicle */
	EffectVehicle() { this->type = VEH_EFFECT; }

	/** We want to 'destruct' the right class. */
	virtual ~EffectVehicle() {}

	const char *GetTypeString() const { return "special vehicle"; }
	void UpdateDeltaXY(Direction direction);
	bool Tick();
};

#define FOR_ALL_EFFECTVEHICLES(var) FOR_ALL_VEHICLES_OF_TYPE(EffectVehicle, var)

#endif /* EFFECTVEHICLE_BASE_H */
