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
	EV_CHIMNEY_SMOKE            =  0, ///< Smoke of power plant (industry).
	EV_STEAM_SMOKE              =  1, ///< Smoke of steam engines.
	EV_DIESEL_SMOKE             =  2, ///< Smoke of diesel engines.
	EV_ELECTRIC_SPARK           =  3, ///< Sparcs of electric engines.
	EV_CRASH_SMOKE              =  4, ///< Smoke of disasters.
	EV_EXPLOSION_LARGE          =  5, ///< Various explosions.
	EV_BREAKDOWN_SMOKE          =  6, ///< Smoke of broken vehicles except aircraft.
	EV_EXPLOSION_SMALL          =  7, ///< Various explosions.
	EV_BULLDOZER                =  8, ///< Bulldozer at roadworks.
	EV_BUBBLE                   =  9, ///< Bubble of bubble generator (industry).
	EV_BREAKDOWN_SMOKE_AIRCRAFT = 10, ///< Smoke of broken aircraft.
	EV_COPPER_MINE_SMOKE        = 11, ///< Smoke at copper mine.
	EV_END
};

EffectVehicle *CreateEffectVehicle(int x, int y, int z, EffectVehicleType type);
EffectVehicle *CreateEffectVehicleAbove(int x, int y, int z, EffectVehicleType type);
EffectVehicle *CreateEffectVehicleRel(const Vehicle *v, int x, int y, int z, EffectVehicleType type);

#endif /* EFFECTVEHICLE_FUNC_H */
