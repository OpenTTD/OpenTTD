/* $Id$ */

/** @file effectvehicle.h Functions related to effect vehicles. */

#ifndef EFFECTVEHICLE_FUNC_H
#define EFFECTVEHICLE_FUNC_H

#include "vehicle_type.h"

/** Effect vehicle types */
enum EffectVehicleType {
	EV_CHIMNEY_SMOKE   = 0,
	EV_STEAM_SMOKE     = 1,
	EV_DIESEL_SMOKE    = 2,
	EV_ELECTRIC_SPARK  = 3,
	EV_SMOKE           = 4,
	EV_EXPLOSION_LARGE = 5,
	EV_BREAKDOWN_SMOKE = 6,
	EV_EXPLOSION_SMALL = 7,
	EV_BULLDOZER       = 8,
	EV_BUBBLE          = 9
};

Vehicle *CreateEffectVehicle(int x, int y, int z, EffectVehicleType type);
Vehicle *CreateEffectVehicleAbove(int x, int y, int z, EffectVehicleType type);
Vehicle *CreateEffectVehicleRel(const Vehicle *v, int x, int y, int z, EffectVehicleType type);

#endif /* EFFECTVEHICLE_FUNC_H */
