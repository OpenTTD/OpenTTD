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

/** The type all our vehicle IDs have. */
typedef uint32_t VehicleID;

static const int GROUND_ACCELERATION = 9800; ///< Acceleration due to gravity, 9.8 m/s^2

/** Available vehicle types. It needs to be 8bits, because we save and load it as such */
enum VehicleType : byte {
	VEH_BEGIN,

	VEH_TRAIN = VEH_BEGIN,        ///< %Train vehicle type.
	VEH_ROAD,                     ///< Road vehicle type.
	VEH_SHIP,                     ///< %Ship vehicle type.
	VEH_AIRCRAFT,                 ///< %Aircraft vehicle type.

	VEH_COMPANY_END,              ///< Last company-ownable type.

	VEH_EFFECT = VEH_COMPANY_END, ///< Effect vehicle type (smoke, explosions, sparks, bubbles)
	VEH_DISASTER,                 ///< Disaster vehicle type.

	VEH_END,
	VEH_INVALID = 0xFF,           ///< Non-existing type of vehicle.
};
DECLARE_POSTFIX_INCREMENT(VehicleType)

struct Vehicle;
struct Train;
struct RoadVehicle;
struct Ship;
struct Aircraft;
struct EffectVehicle;
struct DisasterVehicle;

/** Base vehicle class. */
struct BaseVehicle
{
	VehicleType type; ///< Type of vehicle
};

static const VehicleID INVALID_VEHICLE = 0xFFFFF; ///< Constant representing a non-existing vehicle.

/** Pathfinding option states */
enum VehiclePathFinders {
	// Original PathFinder (OPF) used to be 0
	VPF_NPF  = 1, ///< New PathFinder
	VPF_YAPF = 2, ///< Yet Another PathFinder
};

/** Flags for goto depot commands. */
enum class DepotCommand : byte {
	None         = 0,         ///< No special flags.
	Service      = (1U << 0), ///< The vehicle will leave the depot right after arrival (service only)
	MassSend     = (1U << 1), ///< Tells that it's a mass send to depot command (type in VLW flag)
	DontCancel   = (1U << 2), ///< Don't cancel current goto depot command if any
	LocateHangar = (1U << 3), ///< Find another airport if the target one lacks a hangar
};
DECLARE_ENUM_AS_BIT_SET(DepotCommand)

static const uint MAX_LENGTH_VEHICLE_NAME_CHARS = 32; ///< The maximum length of a vehicle name in characters including '\0'

/** The length of a vehicle in tile units. */
static const uint VEHICLE_LENGTH = 8;

/** Vehicle acceleration models. */
enum AccelerationModel {
	AM_ORIGINAL,
	AM_REALISTIC,
};

/** Visualisation contexts of vehicles and engines. */
enum EngineImageType {
	EIT_ON_MAP     = 0x00,  ///< Vehicle drawn in viewport.
	EIT_IN_DEPOT   = 0x10,  ///< Vehicle drawn in depot.
	EIT_IN_DETAILS = 0x11,  ///< Vehicle drawn in vehicle details, refit window, ...
	EIT_IN_LIST    = 0x12,  ///< Vehicle drawn in vehicle list, group list, ...
	EIT_PURCHASE   = 0x20,  ///< Vehicle drawn in purchase list, autoreplace gui, ...
	EIT_PREVIEW    = 0x21,  ///< Vehicle drawn in preview window, news, ...
};

#endif /* VEHICLE_TYPE_H */
