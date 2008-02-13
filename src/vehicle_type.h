/* $Id$ */

/** @file vehicle_type.h Types related to vehicles. */

#ifndef VEHICLE_TYPE_H
#define VEHICLE_TYPE_H

#include "core/enum_type.hpp"

typedef uint16 VehicleID;

enum VehicleType {
	VEH_TRAIN,
	VEH_ROAD,
	VEH_SHIP,
	VEH_AIRCRAFT,
	VEH_SPECIAL,
	VEH_DISASTER,
	VEH_END,
	VEH_INVALID = 0xFF,
};
DECLARE_POSTFIX_INCREMENT(VehicleType);
template <> struct EnumPropsT<VehicleType> : MakeEnumPropsT<VehicleType, byte, VEH_TRAIN, VEH_END, VEH_INVALID> {};
typedef TinyEnumT<VehicleType> VehicleTypeByte;

struct Vehicle;

struct BaseVehicle
{
	VehicleTypeByte type;    ///< Type of vehicle

	/**
	 * Is this vehicle a valid vehicle?
	 * @return true if and only if the vehicle is valid.
	 */
	inline bool IsValid() const { return this->type != VEH_INVALID; }
};

static const VehicleID INVALID_VEHICLE = 0xFFFF;

/* Effect vehicle types */
enum EffectVehicle {
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

/** Pathfinding option states */
enum {
	VPF_OPF  = 0, ///< The Original PathFinder
	VPF_NTP  = 0, ///< New Train Pathfinder, replacing OPF for trains
	VPF_NPF  = 1, ///< New PathFinder
	VPF_YAPF = 2, ///< Yet Another PathFinder
};

#endif /* VEHICLE_TYPE_H */
