/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file disaster_vehicle.h All disaster vehicles. */

#ifndef DISASTER_VEHICLE_H
#define DISASTER_VEHICLE_H

#include "vehicle_base.h"

/** Different sub types of disaster vehicles. */
enum DisasterSubType {
	ST_ZEPPELINER,               ///< Zeppelin, crashes at airports.
	ST_ZEPPELINER_SHADOW,        ///< Shadow of the zeppelin.
	ST_SMALL_UFO,                ///< Small UFO, tries to find a road vehicle to destroy.
	ST_SMALL_UFO_SHADOW,         ///< Shadow of small UFO
	ST_AIRPLANE,                 ///< Airplane destroying an oil refinery
	ST_AIRPLANE_SHADOW,          ///< Shadow of airplane
	ST_HELICOPTER,               ///< Helicopter destroying a factory.
	ST_HELICOPTER_SHADOW,        ///< Shadow of helicopter.
	ST_HELICOPTER_ROTORS,        ///< Rotors of helicopter.
	ST_BIG_UFO,                  ///< Big UFO, finds a piece of railroad to "park" on
	ST_BIG_UFO_SHADOW,           ///< Shadow of the big UFO
	ST_BIG_UFO_DESTROYER,        ///< Aircraft the will bomb the big UFO
	ST_BIG_UFO_DESTROYER_SHADOW, ///< Shadow of the aircraft.
	ST_SMALL_SUBMARINE,          ///< Small submarine, pops up in the oceans but doesn't do anything
	ST_BIG_SUBMARINE,            ///< Big submarine, pops up in the oceans but doesn't do anything
};

/**
 * Disasters, like submarines, skyrangers and their shadows, belong to this class.
 */
struct DisasterVehicle FINAL : public SpecializedVehicle<DisasterVehicle, VEH_DISASTER> {
	SpriteID image_override;            ///< Override for the default disaster vehicle sprite.
	VehicleID big_ufo_destroyer_target; ///< The big UFO that this destroyer is supposed to bomb.
	byte flags;                         ///< Flags about the state of the vehicle, @see AirVehicleFlags
	uint16_t state;                     ///< Action stage of the disaster vehicle.

	/** For use by saveload. */
	DisasterVehicle() : SpecializedVehicleBase() {}
	DisasterVehicle(int x, int y, Direction direction, DisasterSubType subtype, VehicleID big_ufo_destroyer_target = VEH_INVALID);
	/** We want to 'destruct' the right class. */
	virtual ~DisasterVehicle() = default;

	void UpdatePosition(int x, int y, int z);
	void UpdateDeltaXY() override;
	void UpdateImage();
	bool Tick() override;
};

#endif /* DISASTER_VEHICLE_H */
