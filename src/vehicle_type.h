/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehicle_type.h Types related to vehicles. */

#ifndef VEHICLE_TYPE_H
#define VEHICLE_TYPE_H

#include "core/enum_type.hpp"

typedef uint16 VehicleID;

/** Available vehicle types. */
enum VehicleType {
	VEH_TRAIN,          ///< %Train vehicle type.
	VEH_ROAD,           ///< Road vehicle type.
	VEH_SHIP,           ///< %Ship vehicle type.
	VEH_AIRCRAFT,       ///< %Aircraft vehicle type.
	VEH_EFFECT,         ///< Effect vehicle type (smoke, explosions, sparks, bubbles)
	VEH_DISASTER,       ///< Disaster vehicle type.
	VEH_END,
	VEH_INVALID = 0xFF, ///< Non-existing type of vehicle.
};
DECLARE_POSTFIX_INCREMENT(VehicleType);
/** It needs to be 8bits, because we save and load it as such */
typedef SimpleTinyEnumT<VehicleType, byte> VehicleTypeByte;

struct Vehicle;
struct Train;
struct RoadVehicle;
struct Ship;
struct Aircraft;
struct EffectVehicle;
struct DisasterVehicle;

struct BaseVehicle
{
	VehicleTypeByte type;    ///< Type of vehicle
};

static const VehicleID INVALID_VEHICLE = 0xFFFF; ///< Constant representing a non-existing vehicle.

/** Pathfinding option states */
enum {
	VPF_OPF  = 0, ///< The Original PathFinder (only for ships)
	VPF_NPF  = 1, ///< New PathFinder
	VPF_YAPF = 2, ///< Yet Another PathFinder
};

/* Flags to add to p2 for goto depot commands
 * Note: bits 8-10 are used for VLW flags */
enum DepotCommand {
	DEPOT_SERVICE       = (1 << 0), ///< The vehicle will leave the depot right after arrival (serivce only)
	DEPOT_MASS_SEND     = (1 << 1), ///< Tells that it's a mass send to depot command (type in VLW flag)
	DEPOT_DONT_CANCEL   = (1 << 2), ///< Don't cancel current goto depot command if any
	DEPOT_LOCATE_HANGAR = (1 << 3), ///< Find another airport if the target one lacks a hangar
	DEPOT_COMMAND_MASK  = 0xF,
};

enum {
	MAX_LENGTH_VEHICLE_NAME_BYTES  =  31, ///< The maximum length of a vehicle name in bytes including '\0'
	MAX_LENGTH_VEHICLE_NAME_PIXELS = 150, ///< The maximum length of a vehicle name in pixels
};

/** Vehicle acceleration models. */
enum AccelerationModel {
	AM_ORIGINAL,
	AM_REALISTIC,
};

#endif /* VEHICLE_TYPE_H */
