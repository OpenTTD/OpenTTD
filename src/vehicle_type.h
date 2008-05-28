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
	VEH_EFFECT,
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

/** Pathfinding option states */
enum {
	VPF_OPF  = 0, ///< The Original PathFinder
	VPF_NTP  = 0, ///< New Train Pathfinder, replacing OPF for trains
	VPF_NPF  = 1, ///< New PathFinder
	VPF_YAPF = 2, ///< Yet Another PathFinder
};

/* Flags to add to p2 for goto depot commands */
/* Note: bits 8-10 are used for VLW flags */
enum DepotCommand {
	DEPOT_SERVICE       = (1 << 0), ///< The vehicle will leave the depot right after arrival (serivce only)
	DEPOT_MASS_SEND     = (1 << 1), ///< Tells that it's a mass send to depot command (type in VLW flag)
	DEPOT_DONT_CANCEL   = (1 << 2), ///< Don't cancel current goto depot command if any
	DEPOT_LOCATE_HANGAR = (1 << 3), ///< Find another airport if the target one lacks a hangar
	DEPOT_COMMAND_MASK  = 0xF,
};

#endif /* VEHICLE_TYPE_H */
