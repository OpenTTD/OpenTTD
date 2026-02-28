/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_cargo.hpp Everything to query cargoes. */

#ifndef SCRIPT_CARGO_HPP
#define SCRIPT_CARGO_HPP

#include "script_object.hpp"
#include "../../cargotype.h"
#include "../../linkgraph/linkgraph_type.h"

/**
 * Class that handles all cargo related functions.
 * @api ai game
 */
class ScriptCargo : public ScriptObject {
public:
	/**
	 * The classes of cargo.
	 */
	enum CargoClass {
		/* Note: these values represent part of the in-game CargoClass enum */
		CC_PASSENGERS   = ::CargoClasses{::CargoClass::Passengers}.base(),   ///< Passengers. Cargoes of this class appear at bus stops. Cargoes not of this class appear at truck stops.
		CC_MAIL         = ::CargoClasses{::CargoClass::Mail}.base(),         ///< Mail
		CC_EXPRESS      = ::CargoClasses{::CargoClass::Express}.base(),      ///< Express cargo (Goods, Food, Candy, but also possible for passengers)
		CC_ARMOURED     = ::CargoClasses{::CargoClass::Armoured}.base(),     ///< Armoured cargo (Valuables, Gold, Diamonds)
		CC_BULK         = ::CargoClasses{::CargoClass::Bulk}.base(),         ///< Bulk cargo (Coal, Grain etc., Ores, Fruit)
		CC_PIECE_GOODS  = ::CargoClasses{::CargoClass::PieceGoods}.base(),   ///< Piece goods (Livestock, Wood, Steel, Paper)
		CC_LIQUID       = ::CargoClasses{::CargoClass::Liquid}.base(),       ///< Liquids (Oil, Water, Rubber)
		CC_REFRIGERATED = ::CargoClasses{::CargoClass::Refrigerated}.base(), ///< Refrigerated cargo (Food, Fruit)
		CC_HAZARDOUS    = ::CargoClasses{::CargoClass::Hazardous}.base(),    ///< Hazardous cargo (Nuclear Fuel, Explosives, etc.)
		CC_COVERED      = ::CargoClasses{::CargoClass::Covered}.base(),      ///< Covered/Sheltered Freight (Transportation in Box Vans, Silo Wagons, etc.)
		CC_OVERSIZED    = ::CargoClasses{::CargoClass::Oversized}.base(),    ///< Oversized (stake/flatbed wagon)
		CC_POWDERIZED   = ::CargoClasses{::CargoClass::Powderized}.base(),   ///< Powderized, moist protected (powder/silo wagon)
		CC_NON_POURABLE = ::CargoClasses{::CargoClass::NotPourable}.base(),  ///< Non-pourable (open wagon, but not hopper wagon)
		CC_POTABLE      = ::CargoClasses{::CargoClass::Potable}.base(),      ///< Potable / food / clean.
		CC_NON_POTABLE  = ::CargoClasses{::CargoClass::NonPotable}.base(),   ///< Non-potable / non-food / dirty.
	};

	/**
	 * The effects a cargo can have on a town.
	 */
	enum TownEffect {
		/* Note: these values represent part of the in-game TownEffect enum */
		TE_NONE       = ::TAE_NONE,       ///< This cargo has no effect on a town
		TE_PASSENGERS = ::TAE_PASSENGERS, ///< This cargo supplies passengers to a town
		TE_MAIL       = ::TAE_MAIL,       ///< This cargo supplies mail to a town
		TE_GOODS      = ::TAE_GOODS,      ///< This cargo supplies goods to a town
		TE_WATER      = ::TAE_WATER,      ///< This cargo supplies water to a town
		TE_FOOD       = ::TAE_FOOD,       ///< This cargo supplies food to a town
	};

	/**
	 * Special cargo types.
	 */
	enum SpecialCargoType {
		/* Note: these values represent part of the in-game CargoTypes enum */
		CT_AUTO_REFIT = ::CARGO_AUTO_REFIT, ///< Automatically choose cargo type when doing auto-refitting.
		CT_NO_REFIT   = ::CARGO_NO_REFIT, ///< Do not refit cargo of a vehicle.
		CT_INVALID    = ::INVALID_CARGO, ///< An invalid cargo type.
	};

	/**
	 * Type of cargo distribution.
	 */
	enum DistributionType {
		DT_MANUAL = to_underlying(::DistributionType::Manual), ///< Manual distribution.
		DT_ASYMMETRIC = to_underlying(::DistributionType::Asymmetric), ///< Asymmetric distribution. Usually cargo will only travel in one direction.
		DT_SYMMETRIC = to_underlying(::DistributionType::Symmetric), ///< Symmetric distribution. The same amount of cargo travels in each direction between each pair of nodes.
		INVALID_DISTRIBUTION_TYPE = 0xFFFF, ///< Invalid distribution type.
	};

	/**
	 * Checks whether the given cargo type is valid.
	 * @param cargo_type The cargo to check.
	 * @return True if and only if the cargo type is valid.
	 */
	static bool IsValidCargo(CargoType cargo_type);

	/**
	 * Checks whether the given town effect type is valid.
	 * @param towneffect_type The town effect to check.
	 * @return True if and only if the town effect type is valid.
	 */
	static bool IsValidTownEffect(TownEffect towneffect_type);

	/**
	 * Get the name of the cargo type.
	 * @param cargo_type The cargo type to get the name of.
	 * @pre IsValidCargo(cargo_type).
	 * @return The name of the cargo type.
	 */
	static std::optional<std::string> GetName(CargoType cargo_type);

	/**
	 * Gets the string representation of the cargo label.
	 * @param cargo_type The cargo to get the string representation of.
	 * @pre ScriptCargo::IsValidCargo(cargo_type).
	 * @return The cargo label.
	 * @note
	 *  - The label uniquely identifies a specific cargo. Use this if you want to
	 *    detect special cargos from specific industry set (like production booster cargos, supplies, ...).
	 *  - For more generic cargo support, rather check cargo properties though. For example:
	 *     - Use ScriptCargo::HasCargoClass(..., CC_PASSENGER) to decide bus vs. truck requirements.
	 *     - Use ScriptCargo::GetTownEffect(...) paired with ScriptTown::GetCargoGoal(...) to determine
	 *       town growth requirements.
	 *  - In other words: Only use the cargo label, if you know more about the behaviour
	 *    of a specific cargo from a specific industry set, than the API methods can tell you.
	 */
	static std::optional<std::string> GetCargoLabel(CargoType cargo_type);

	/**
	 * Checks whether the give cargo is a freight or not.
	 * This defines whether the "freight train weight multiplier" will apply to
	 * trains transporting this cargo.
	 * @param cargo_type The cargo to check on.
	 * @pre ScriptCargo::IsValidCargo(cargo_type).
	 * @return True if and only if the cargo is freight.
	 */
	static bool IsFreight(CargoType cargo_type);

	/**
	 * Check if this cargo is in the requested cargo class.
	 * @param cargo_type The cargo to check on.
	 * @pre ScriptCargo::IsValidCargo(cargo_type).
	 * @param cargo_class The class to check for.
	 * @return True if and only if the cargo is in the cargo class.
	 */
	static bool HasCargoClass(CargoType cargo_type, CargoClass cargo_class);

	/**
	 * Get the effect this cargo has on a town.
	 * @param cargo_type The cargo to check on.
	 * @pre ScriptCargo::IsValidCargo(cargo_type).
	 * @return The effect this cargo has on a town, or TE_NONE if it has no effect.
	 */
	static TownEffect GetTownEffect(CargoType cargo_type);

	/**
	 * Get the income for transporting a piece of cargo over the
	 *   given distance within the specified time.
	 * @param cargo_type The cargo to transport.
	 * @pre ScriptCargo::IsValidCargo(cargo_type).
	 * @param distance The distance the cargo travels from begin to end.
	 *                 The value will be clamped to 0 .. MAX(uint32_t).
	 * @param days_in_transit Amount of (game) days the cargo is in transit.
	 *                        The max value of this variable is 637. Any value higher returns the same as 637 would.
	 * @return The amount of money that would be earned by this trip.
	 */
	static Money GetCargoIncome(CargoType cargo_type, SQInteger distance, SQInteger days_in_transit);

	/**
	 * Get the cargo distribution type for a cargo.
	 * @param cargo_type The cargo to check on.
	 * @return The cargo distribution type for the given cargo.
	 */
	static DistributionType GetDistributionType(CargoType cargo_type);

	/**
	 * Get the weight in tonnes for the given amount of
	 *   cargo for the specified type.
	 * @param cargo_type The cargo to check on.
	 * @param amount The quantity of cargo.
	 *               The value will be clamped to 0 .. MAX(uint32_t).
	 * @pre ScriptCargo::IsValidCargo(cargo_type).
	 * @return The weight in tonnes for that quantity of cargo.
	 */
	static SQInteger GetWeight(CargoType cargo_type, SQInteger amount);
};

#endif /* SCRIPT_CARGO_HPP */
