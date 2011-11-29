/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_cargo.hpp Everything to query cargos. */

#ifndef SCRIPT_CARGO_HPP
#define SCRIPT_CARGO_HPP

#include "script_object.hpp"

/**
 * Class that handles all cargo related functions.
 */
class AICargo : public AIObject {
public:
	/**
	 * The classes of cargo (from newgrf_cargo.h).
	 */
	enum CargoClass {
		CC_PASSENGERS   = 1 <<  0, ///< Passengers. Cargos of this class appear at bus stops. Cargos not of this class appear at truck stops.
		CC_MAIL         = 1 <<  1, ///< Mail
		CC_EXPRESS      = 1 <<  2, ///< Express cargo (Goods, Food, Candy, but also possible for passengers)
		CC_ARMOURED     = 1 <<  3, ///< Armoured cargo (Valuables, Gold, Diamonds)
		CC_BULK         = 1 <<  4, ///< Bulk cargo (Coal, Grain etc., Ores, Fruit)
		CC_PIECE_GOODS  = 1 <<  5, ///< Piece goods (Livestock, Wood, Steel, Paper)
		CC_LIQUID       = 1 <<  6, ///< Liquids (Oil, Water, Rubber)
		CC_REFRIGERATED = 1 <<  7, ///< Refrigerated cargo (Food, Fruit)
		CC_HAZARDOUS    = 1 <<  8, ///< Hazardous cargo (Nuclear Fuel, Explosives, etc.)
		CC_COVERED      = 1 <<  9, ///< Covered/Sheltered Freight (Transporation in Box Vans, Silo Wagons, etc.)
	};

	/**
	 * The effects a cargo can have on a town.
	 */
	enum TownEffect {
		TE_NONE       = 0, ///< This cargo has no effect on a town
		TE_PASSENGERS = 1, ///< This cargo supplies passengers to a town
		TE_MAIL       = 2, ///< This cargo supplies mail to a town
		TE_GOODS      = 3, ///< This cargo supplies goods to a town
		TE_WATER      = 4, ///< This cargo supplies water to a town
		TE_FOOD       = 5, ///< This cargo supplies food to a town
	};

	/**
	 * Special cargo types.
	 */
	enum SpecialCargoID {
		CT_AUTO_REFIT = 0xFD, ///< Automatically choose cargo type when doing auto-refitting.
		CT_NO_REFIT   = 0xFE, ///< Do not refit cargo of a vehicle.
	};

	/**
	 * Checks whether the given cargo type is valid.
	 * @param cargo_type The cargo to check.
	 * @return True if and only if the cargo type is valid.
	 */
	static bool IsValidCargo(CargoID cargo_type);

	/**
	 * Checks whether the given town effect type is valid.
	 * @param towneffect_type The town effect to check.
	 * @return True if and only if the town effect type is valid.
	 */
	static bool IsValidTownEffect(TownEffect towneffect_type);

	/**
	 * Gets the string representation of the cargo label.
	 * @param cargo_type The cargo to get the string representation of.
	 * @pre AICargo::IsValidCargo(cargo_type).
	 * @return The cargo label.
	 * @note Never use this to check if it is a certain cargo. NewGRF can
	 *  redefine all of the names.
	 */
	static char *GetCargoLabel(CargoID cargo_type);

	/**
	 * Checks whether the give cargo is a freight or not.
	 * This defines whether the "freight train weight multiplier" will apply to
	 * trains transporting this cargo.
	 * @param cargo_type The cargo to check on.
	 * @pre AICargo::IsValidCargo(cargo_type).
	 * @return True if and only if the cargo is freight.
	 */
	static bool IsFreight(CargoID cargo_type);

	/**
	 * Check if this cargo is in the requested cargo class.
	 * @param cargo_type The cargo to check on.
	 * @pre AICargo::IsValidCargo(cargo_type).
	 * @param cargo_class The class to check for.
	 * @return True if and only if the cargo is in the cargo class.
	 */
	static bool HasCargoClass(CargoID cargo_type, CargoClass cargo_class);

	/**
	 * Get the effect this cargo has on a town.
	 * @param cargo_type The cargo to check on.
	 * @pre AICargo::IsValidCargo(cargo_type).
	 * @return The effect this cargo has on a town, or TE_NONE if it has no effect.
	 */
	static TownEffect GetTownEffect(CargoID cargo_type);

	/**
	 * Get the income for transporting a piece of cargo over the
	 *   given distance within the specified time.
	 * @param cargo_type The cargo to transport.
	 * @pre AICargo::IsValidCargo(cargo_type).
	 * @param distance The distance the cargo travels from begin to end.
	 * @param days_in_transit Amount of (game) days the cargo is in transit. The max value of this variable is 637. Any value higher returns the same as 637 would.
	 * @return The amount of money that would be earned by this trip.
	 */
	static Money GetCargoIncome(CargoID cargo_type, uint32 distance, uint32 days_in_transit);
};

#endif /* SCRIPT_CARGO_HPP */
