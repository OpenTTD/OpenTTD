/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file effectvehicle_func.h Functions related to effect vehicles. */

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

EffectVehicle *CreateEffectVehicle(int x, int y, int z, EffectVehicleType type);
EffectVehicle *CreateEffectVehicleAbove(int x, int y, int z, EffectVehicleType type);
EffectVehicle *CreateEffectVehicleRel(const Vehicle *v, int x, int y, int z, EffectVehicleType type);

#endif /* EFFECTVEHICLE_FUNC_H */
