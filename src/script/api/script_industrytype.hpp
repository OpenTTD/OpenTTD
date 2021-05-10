/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_industrytype.hpp Everything to query and build industries. */

#ifndef SCRIPT_INDUSTRYTYPE_HPP
#define SCRIPT_INDUSTRYTYPE_HPP

#include "script_list.hpp"

/**
 * Class that handles all industry-type related functions.
 * @api ai game
 */
class ScriptIndustryType : public ScriptObject {
public:
	/**
	 * Special IndustryTypes.
	 */
	enum SpecialIndustryType {
		INDUSTRYTYPE_UNKNOWN = 0xFE, ///< Unknown/unspecific industrytype. (Usable for ScriptRail::BuildNewGRFRailStation())
		INDUSTRYTYPE_TOWN    = 0xFF, ///< No industry, but town. (Usable for ScriptRail::BuildNewGRFRailStation())
	};

	/**
	 * Checks whether the given industry-type is valid.
	 * @param industry_type The type check.
	 * @return True if and only if the industry-type is valid.
	 */
	static bool IsValidIndustryType(IndustryType industry_type);

	/**
	 * Get the name of an industry-type.
	 * @param industry_type The type to get the name for.
	 * @pre IsValidIndustryType(industry_type).
	 * @return The name of an industry.
	 */
	static char *GetName(IndustryType industry_type);

	/**
	 * Get a list of CargoID possible produced by this industry-type.
	 * @warning This function only returns the default cargoes of the industry type.
	 *          Industries can specify new cargotypes on construction.
	 * @param industry_type The type to get the CargoIDs for.
	 * @pre IsValidIndustryType(industry_type).
	 * @return The CargoIDs of all cargotypes this industry could produce.
	 */
	static ScriptList *GetProducedCargo(IndustryType industry_type);

	/**
	 * Get a list of CargoID accepted by this industry-type.
	 * @warning This function only returns the default cargoes of the industry type.
	 *          Industries can specify new cargotypes on construction.
	 * @param industry_type The type to get the CargoIDs for.
	 * @pre IsValidIndustryType(industry_type).
	 * @return The CargoIDs of all cargotypes this industry accepts.
	 */
	static ScriptList *GetAcceptedCargo(IndustryType industry_type);

	/**
	 * Is this industry type a raw industry?
	 * Raw industries usually produce cargo without any prerequisites.
	 * ("Usually" means that advanced NewGRF industry concepts might not fit the "raw"/"processing"
	 * classification, so it's up to the interpretation of the NewGRF author.)
	 * @param industry_type The type of the industry.
	 * @pre IsValidIndustryType(industry_type).
	 * @return True if it should be handled as a raw industry.
	 * @note Industries might be neither raw nor processing.
	 *       This is usually the case for industries which produce nothing (e.g. power plants),
	 *       but also for weird industries like temperate banks and tropic lumber mills.
	 */
	static bool IsRawIndustry(IndustryType industry_type);

	/**
	 * Is this industry type a processing industry?
	 * Processing industries usually produce cargo when delivered with input cargo.
	 * ("Usually" means that advanced NewGRF industry concepts might not fit the "raw"/"processing"
	 * classification, so it's up to the interpretation of the NewGRF author.)
	 * @param industry_type The type of the industry.
	 * @pre IsValidIndustryType(industry_type).
	 * @return True if it is a processing industry.
	 * @note Industries might be neither raw nor processing.
	 *       This is usually the case for industries which produce nothing (e.g. power plants),
	 *       but also for weird industries like temperate banks and tropic lumber mills.
	 */
	static bool IsProcessingIndustry(IndustryType industry_type);

	/**
	 * Can the production of this industry increase?
	 * @param industry_type The type of the industry.
	 * @pre IsValidIndustryType(industry_type).
	 * @return True if the production of this industry can increase.
	 */
	static bool ProductionCanIncrease(IndustryType industry_type);

	/**
	 * Get the cost for building this industry-type.
	 * @param industry_type The type of the industry.
	 * @pre IsValidIndustryType(industry_type).
	 * @return The cost for building this industry-type.
	 */
	static Money GetConstructionCost(IndustryType industry_type);

	/**
	 * Can you build this type of industry?
	 * @param industry_type The type of the industry.
	 * @pre IsValidIndustryType(industry_type).
	 * @return True if you can build this type of industry at locations of your choice.
	 * @ai @note Returns false if you can only prospect this type of industry, or not build it at all.
	 * @game @note If no valid ScriptCompanyMode active in scope, this method returns false if you can
	 * @game only prospect this type of industry, or not build it at all.
	 * @game @note If no valid ScriptCompanyMode active in scope, the script can
	 * @game build as long as the industry type can be built. (a NewGRF can for example
	 * @game reject construction based on current year)
	 */
	static bool CanBuildIndustry(IndustryType industry_type);

	/**
	 * Can you prospect this type of industry?
	 * @param industry_type The type of the industry.
	 * @pre IsValidIndustryType(industry_type).
	 * @return True if you can prospect this type of industry.
	 * @ai @note If the setting "Manual primary industry construction method" is set
	 * @ai to either "None" or "as other industries" this function always returns false.
	 * @game @note If no valid ScriptCompanyMode is active in scope, and if the setting
	 * @game "Manual primary industry construction method" is set to either "None" or
	 * @game "as other industries" this function always returns false.
	 * @game @note If no valid ScriptCompanyMode active in scope, the script can
	 * @game prospect as long as the industry type can be built. (a NewGRF can for
	 * @game example reject construction based on current year)
	 */
	static bool CanProspectIndustry(IndustryType industry_type);

	/**
	 * Build an industry of the specified type.
	 * @param industry_type The type of the industry to build.
	 * @param tile The tile to build the industry on.
	 * @pre CanBuildIndustry(industry_type).
	 * @return True if the industry was successfully build.
	 */
	static bool BuildIndustry(IndustryType industry_type, TileIndex tile);

	/**
	 * Prospect an industry of this type. Prospecting an industries let the game try to create
	 * an industry on a random place on the map.
	 * @param industry_type The type of the industry.
	 * @pre CanProspectIndustry(industry_type).
	 * @return True if no error occurred while trying to prospect.
	 * @note Even if true is returned there is no guarantee a new industry is build.
	 * @note If true is returned the money is paid, whether a new industry was build or not.
	 * @game @note if no valid ScriptCompanyMode exist in scope, prospection will not fail
	 * @game due to the general chance that prospection may fail. However prospection can still
	 * @game fail if OpenTTD is unable to find a suitable location to place the industry.
	 */
	static bool ProspectIndustry(IndustryType industry_type);

	/**
	 * Is this type of industry built on water.
	 * @param industry_type The type of the industry.
	 * @pre IsValidIndustryType(industry_type).
	 * @return True when this type is built on water.
	 */
	static bool IsBuiltOnWater(IndustryType industry_type);

	/**
	 * Does this type of industry have a heliport?
	 * @param industry_type The type of the industry.
	 * @pre IsValidIndustryType(industry_type).
	 * @return True when this type has a heliport.
	 */
	static bool HasHeliport(IndustryType industry_type);

	/**
	 * Does this type of industry have a dock?
	 * @param industry_type The type of the industry.
	 * @pre IsValidIndustryType(industry_type).
	 * @return True when this type has a dock.
	 */
	static bool HasDock(IndustryType industry_type);
};

#endif /* SCRIPT_INDUSTRYTYPE_HPP */
