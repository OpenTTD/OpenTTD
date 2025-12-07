/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_type.h Types related to vehicles. */

#ifndef VEHICLE_TYPE_H
#define VEHICLE_TYPE_H

#include "core/enum_type.hpp"
#include "core/pool_type.hpp"

/** The type all our vehicle IDs have. */
using VehicleID = PoolID<uint32_t, struct VehicleIDTag, 0xFF000, 0xFFFFF>;

static const int GROUND_ACCELERATION = 9800; ///< Acceleration due to gravity, 9.8 m/s^2

/** Available vehicle types. It needs to be 8bits, because we save and load it as such */
enum VehicleType : uint8_t {
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
DECLARE_INCREMENT_DECREMENT_OPERATORS(VehicleType)
DECLARE_ENUM_AS_ADDABLE(VehicleType)

struct Vehicle;
struct Train;
struct RoadVehicle;
struct Ship;
struct Aircraft;
struct EffectVehicle;
struct DisasterVehicle;

/** Base vehicle class. */
struct BaseVehicle {
	VehicleType type = VEH_INVALID; ///< Type of vehicle
};

/** Flags for goto depot commands. */
enum class DepotCommandFlag : uint8_t {
	Service, ///< The vehicle will leave the depot right after arrival (service only)
	MassSend, ///< Tells that it's a mass send to depot command (type in VLW flag)
	DontCancel, ///< Don't cancel current goto depot command if any
};
using DepotCommandFlags = EnumBitSet<DepotCommandFlag, uint8_t>;

static const uint MAX_LENGTH_VEHICLE_NAME_CHARS = 32; ///< The maximum length of a vehicle name in characters including '\0'

/** The length of a vehicle in tile units. */
static const uint VEHICLE_LENGTH = 8;

/** Vehicle acceleration models. */
enum AccelerationModel : uint8_t {
	AM_ORIGINAL,
	AM_REALISTIC,
};

/** Visualisation contexts of vehicles and engines. */
enum EngineImageType : uint8_t {
	EIT_ON_MAP     = 0x00,  ///< Vehicle drawn in viewport.
	EIT_IN_DEPOT   = 0x10,  ///< Vehicle drawn in depot.
	EIT_IN_DETAILS = 0x11,  ///< Vehicle drawn in vehicle details, refit window, ...
	EIT_IN_LIST    = 0x12,  ///< Vehicle drawn in vehicle list, group list, ...
	EIT_PURCHASE   = 0x20,  ///< Vehicle drawn in purchase list, autoreplace gui, ...
	EIT_PREVIEW    = 0x21,  ///< Vehicle drawn in preview window, news, ...
};

/** Randomisation triggers for vehicles */
enum class VehicleRandomTrigger : uint8_t {
	NewCargo, ///< Affected vehicle only: Vehicle is loaded with cargo, after it was empty.
	Depot, ///< Front vehicle only: Consist arrived in depot.
	Empty, ///< Front vehicle only: Entire consist is empty.
	AnyNewCargo, ///< All vehicles in consist: Any vehicle in the consist received new cargo.
	Callback32, ///< All vehicles in consist: 32 day callback requested rerandomisation
};
using VehicleRandomTriggers = EnumBitSet<VehicleRandomTrigger, uint8_t>;

#endif /* VEHICLE_TYPE_H */
