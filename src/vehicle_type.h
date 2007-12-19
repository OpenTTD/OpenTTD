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

#endif /* VEHICLE_TYPE_H */
