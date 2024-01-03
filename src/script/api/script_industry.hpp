/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_industry.hpp Everything to query and build industries. */

#ifndef SCRIPT_INDUSTRY_HPP
#define SCRIPT_INDUSTRY_HPP

#include "script_company.hpp"
#include "script_date.hpp"
#include "script_object.hpp"
#include "../../industry.h"

/**
 * Class that handles all industry related functions.
 * @api ai game
 */
class ScriptIndustry : public ScriptObject {
public:
	/** Ways for an industry to accept a cargo. */
	enum CargoAcceptState {
		CAS_NOT_ACCEPTED, ///< The CargoID is not accepted by this industry.
		CAS_ACCEPTED,     ///< The industry currently accepts this CargoID.
		CAS_TEMP_REFUSED, ///< The industry temporarily refuses to accept this CargoID but may do so again in the future.
	};

	/**
	 * Control flags for industry
	 * @api -ai
	 */
	enum IndustryControlFlags {
		/**
		 * When industry production change is evaluated, rolls to decrease are ignored.
		 * This also prevents industry closure due to production dropping to the lowest level.
		 */
		INDCTL_NO_PRODUCTION_DECREASE = ::INDCTL_NO_PRODUCTION_DECREASE,
		/**
		 * When industry production change is evaluated, rolls to increase are ignored.
		 */
		INDCTL_NO_PRODUCTION_INCREASE = ::INDCTL_NO_PRODUCTION_INCREASE,
		/**
		 * Industry can not close regardless of production level or time since last delivery.
		 * This does not prevent a closure already announced.
		 */
		INDCTL_NO_CLOSURE             = ::INDCTL_NO_CLOSURE,
		/**
		 * Indicates that the production level of the industry is controlled by a game script.
		 */
		INDCTL_EXTERNAL_PROD_LEVEL    = ::INDCTL_EXTERNAL_PROD_LEVEL,
	};

	/**
	 * Gets the number of industries.
	 * @return The number of industries.
	 * @note The maximum valid IndustryID can be higher than the value returned.
	 */
	static SQInteger GetIndustryCount();

	/**
	 * Checks whether the given industry index is valid.
	 * @param industry_id The index to check.
	 * @return True if and only if the industry is valid.
	 */
	static bool IsValidIndustry(IndustryID industry_id);

	/**
	 * Get the IndustryID of a tile, if there is an industry.
	 * @param tile The tile to find the IndustryID of.
	 * @return IndustryID of the industry.
	 * @post Use IsValidIndustry() to see if the industry is valid.
	 * @note GetIndustryID will return an invalid IndustryID for the
	 *   station tile of industries with a dock/heliport.
	 */
	static IndustryID GetIndustryID(TileIndex tile);

	/**
	 * Get the name of the industry.
	 * @param industry_id The industry to get the name of.
	 * @pre IsValidIndustry(industry_id).
	 * @return The name of the industry.
	 */
	static std::optional<std::string> GetName(IndustryID industry_id);

	/**
	 * Get the construction date of an industry.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return Date the industry was constructed.
	 * @api -ai
	 */
	static ScriptDate::Date GetConstructionDate(IndustryID industry_id);

	/**
	 * Set the custom text of an industry, shown in the GUI.
	 * @param industry_id The industry to set the custom text of.
	 * @param text The text to set it to (can be either a raw string, or a ScriptText object). If null, or an empty string, is passed, the text will be removed.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre IsValidIndustry(industry_id).
	 * @return True if the action succeeded.
	 * @api -ai
	 */
	static bool SetText(IndustryID industry_id, Text *text);

	/**
	 * See whether an industry currently accepts a certain cargo.
	 * @param industry_id The index of the industry.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptCargo::IsValidCargo(cargo_id).
	 * @return Whether the industry accepts, temporarily refuses or never accepts this cargo.
	 */
	static CargoAcceptState IsCargoAccepted(IndustryID industry_id, CargoID cargo_id);

	/**
	 * Get the amount of cargo stockpiled for processing.
	 * @param industry_id The index of the industry.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptCargo::IsValidCargo(cargo_id).
	 * @return The amount of cargo that is waiting for processing.
	 */
	static SQInteger GetStockpiledCargo(IndustryID industry_id, CargoID cargo_id);

	/**
	 * Get the total last month's production of the given cargo at an industry.
	 * @param industry_id The index of the industry.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptCargo::IsValidCargo(cargo_id).
	 * @return The last month's production of the given cargo for this industry.
	 */
	static SQInteger GetLastMonthProduction(IndustryID industry_id, CargoID cargo_id);

	/**
	 * Get the total amount of cargo transported from an industry last month.
	 * @param industry_id The index of the industry.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptCargo::IsValidCargo(cargo_id).
	 * @return The amount of given cargo transported from this industry last month.
	 */
	static SQInteger GetLastMonthTransported(IndustryID industry_id, CargoID cargo_id);

	/**
	 * Get the percentage of cargo transported from an industry last month.
	 * @param industry_id The index of the industry.
	 * @param cargo_id The index of the cargo.
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptCargo::IsValidCargo(cargo_id).
	 * @return The percentage of given cargo transported from this industry last month.
	 */
	static SQInteger GetLastMonthTransportedPercentage(IndustryID industry_id, CargoID cargo_id);

	/**
	 * Gets the location of the industry.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return The location of the industry.
	 */
	static TileIndex GetLocation(IndustryID industry_id);

	/**
	 * Get the number of stations around an industry. All stations that can
	 * service the industry are counted, your own stations but also your
	 * opponents stations.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return The number of stations around an industry.
	 */
	static SQInteger GetAmountOfStationsAround(IndustryID industry_id);

	/**
	 * Get the manhattan distance from the tile to the ScriptIndustry::GetLocation()
	 *  of the industry.
	 * @param industry_id The industry to get the distance to.
	 * @param tile The tile to get the distance to.
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptMap::IsValidTile(tile).
	 * @return The distance between industry and tile.
	 */
	static SQInteger GetDistanceManhattanToTile(IndustryID industry_id, TileIndex tile);

	/**
	 * Get the square distance from the tile to the ScriptIndustry::GetLocation()
	 *  of the industry.
	 * @param industry_id The industry to get the distance to.
	 * @param tile The tile to get the distance to.
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptMap::IsValidTile(tile).
	 * @return The distance between industry and tile.
	 */
	static SQInteger GetDistanceSquareToTile(IndustryID industry_id, TileIndex tile);

	/**
	 * Is this industry built on water.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return True when the industry is built on water.
	 */
	static bool IsBuiltOnWater(IndustryID industry_id);

	/**
	 * Does this industry have a heliport?
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return True when the industry has a heliport.
	 */
	static bool HasHeliport(IndustryID industry_id);

	/**
	 * Gets the location of the industry's heliport.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @pre HasHeliport(industry_id).
	 * @return The location of the industry's heliport.
	 */
	static TileIndex GetHeliportLocation(IndustryID industry_id);

	/**
	 * Does this industry have a dock?
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return True when the industry has a dock.
	 */
	static bool HasDock(IndustryID industry_id);

	/**
	 * Gets the location of the industry's dock.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @pre HasDock(industry_id).
	 * @return The location of the industry's dock.
	 */
	static TileIndex GetDockLocation(IndustryID industry_id);

	/**
	 * Get the IndustryType of the industry.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return The IndustryType of the industry.
	 */
	static IndustryType GetIndustryType(IndustryID industry_id);

	/**
	 * Get the last year this industry had any production output.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return Year the industry last had production, 0 if error.
	 * @api -ai
	 */
	static SQInteger GetLastProductionYear(IndustryID industry_id);

	/**
	 * Get the last date this industry accepted any cargo delivery.
	 * @param industry_id The index of the industry.
	 * @param cargo_type The cargo to query, or CT_INVALID to query latest of all accepted cargoes.
	 * @pre IsValidIndustry(industry_id).
	 * @pre IsValidCargo(cargo_type) || cargo_type == CT_INVALID.
	 * @return Date the industry last received cargo from a delivery, or ScriptDate::DATE_INVALID on error.
	 * @api -ai
	 */
	static ScriptDate::Date GetCargoLastAcceptedDate(IndustryID industry_id, CargoID cargo_type);

	/**
	 * Get the current control flags for an industry.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return Bit flags of the IndustryControlFlags enumeration.
	 * @api -ai
	 */
	static SQInteger GetControlFlags(IndustryID industry_id);

	/**
	 * Change the control flags for an industry.
	 * @param industry_id The index of the industry.
	 * @param control_flags New flags as a combination of IndustryControlFlags values.
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptCompanyMode::IsDeity().
	 * @return True if the action succeeded.
	 * @api -ai
	 */
	static bool SetControlFlags(IndustryID industry_id, SQInteger control_flags);

	/**
	 * Find out which company currently has the exclusive rights to deliver cargo to the industry.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return The company that has the exclusive rights. The value
	 *         ScriptCompany::COMPANY_INVALID means that there are currently no
	 *         exclusive rights given out to anyone.
	 */
	static ScriptCompany::CompanyID GetExclusiveSupplier(IndustryID industry_id);

	/**
	 * Sets or resets the company that has exclusive right to deliver cargo to the industry.
	 * @param industry_id The index of the industry.
	 * @param company_id The company to set (ScriptCompany::COMPANY_INVALID to reset).
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptCompanyMode::IsDeity().
	 * @return True if the action succeeded.
	 * @api -ai
	 */
	static bool SetExclusiveSupplier(IndustryID industry_id, ScriptCompany::CompanyID company_id);

	/**
	 * Find out which company currently has the exclusive rights to take cargo from the industry.
	 * @param industry_id The index of the industry.
	 * @pre IsValidIndustry(industry_id).
	 * @return The company that has the exclusive rights. The value
	 *         ScriptCompany::COMPANY_SPECTATOR means that there are currently no
	 *         exclusive rights given out to anyone.
	 */
	static ScriptCompany::CompanyID GetExclusiveConsumer(IndustryID industry_id);

	/**
	 * Sets or resets the company that has exclusive right to take cargo from the industry.
	 * @param industry_id The index of the industry.
	 * @param company_id The company to set (ScriptCompany::COMPANY_INVALID to reset).
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptCompanyMode::IsDeity().
	 * @return True if the action succeeded.
	 * @api -ai
	 */
	static bool SetExclusiveConsumer(IndustryID industry_id, ScriptCompany::CompanyID company_id);

	/**
	 * Gets the current production level of an industry.
	 * @param industry_id The index of the industry.
	 * @api -ai
	 */
	static SQInteger GetProductionLevel(IndustryID industry_id);

	/**
	 * Sets the current production level of an industry.
	 * @note Setting the production level automatically sets the control flag INDCTL_EXTERNAL_PROD_LEVEL if it wasn't already set.
	 *     Normal production behaviour can be restored by clearing the control flag.
	 * @param industry_id The index of the industry.
	 * @param prod_level The production level to set.
	 * @param show_news If set to true and the production changed, generate a production change news message. If set to false, no news message is shown.
	 * @param custom_news Custom news message text to override the default news text with. Pass null to use the default text. Only used if \c show_news is set to true.
	 * @pre IsValidIndustry(industry_id).
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre prod_level >= 4 && prod_level <= 128.
	 * @return True if the action succeeded.
	 * @api -ai
	 */
	static bool SetProductionLevel(IndustryID industry_id, SQInteger prod_level, bool show_news, Text *custom_news);
};

#endif /* SCRIPT_INDUSTRY_HPP */
