/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_news.hpp Everything to handle news messages. */

#ifndef SCRIPT_NEWS_HPP
#define SCRIPT_NEWS_HPP

#include "script_company.hpp"
#include "../../news_type.h"

/**
 * Class that handles news messages.
 * @api game
 */
class ScriptNews : public ScriptObject {
public:
	/**
	 * Enumeration for the news types that a script can create news for.
	 */
	enum NewsType {
		/* Arbitrary selection of NewsTypes which might make sense for scripts */
		NT_ACCIDENT          = to_underlying(::NewsType::Accident),         ///< Category accidents.
		NT_COMPANY_INFO      = to_underlying(::NewsType::CompanyInfo),      ///< Category company info.
		NT_ECONOMY           = to_underlying(::NewsType::Economy),          ///< Category economy.
		NT_ADVICE            = to_underlying(::NewsType::Advice),           ///< Category vehicle advice.
		NT_ACCEPTANCE        = to_underlying(::NewsType::Acceptance),       ///< Category acceptance changes.
		NT_SUBSIDIES         = to_underlying(::NewsType::Subsidies),        ///< Category subsidies.
		NT_GENERAL           = to_underlying(::NewsType::General),          ///< Category general.
	};

	/**
	 * Reference to a game element.
	 */
	enum NewsReferenceType {
		/* Selection of useful game elements to refer to. */
		NR_NONE, ///< No reference supplied.
		NR_TILE, ///< Reference location, scroll to the location when clicking on the news.
		NR_STATION, ///< Reference station, scroll to the station when clicking on the news. Delete news when the station is deleted.
		NR_INDUSTRY, ///< Reference industry, scrolls to the industry when clicking on the news. Delete news when the industry is deleted.
		NR_TOWN, ///< Reference town, scroll to the town when clicking on the news.
	};

	/**
	 * Create a news message for everybody, or for one company.
	 * @param type The type of the news.
	 * @param text The text message to show (can be either a raw string, or a ScriptText object).
	 * @param company The company, or COMPANY_INVALID for all companies.
	 * @param ref_type Type of referred game element.
	 * @param reference The referenced game element of \a ref_type.
	 *  - For #NR_NONE this parameter is ignored.
	 *  - For #NR_TILE this parameter should be a valid location (ScriptMap::IsValidTile).
	 *  - For #NR_STATION this parameter should be a valid stationID (ScriptStation::IsValidStation).
	 *  - For #NR_INDUSTRY this parameter should be a valid industryID (ScriptIndustry::IsValidIndustry).
	 *  - For #NR_TOWN this parameter should be a valid townID (ScriptTown::IsValidTown).
	 * @return True if the action succeeded.
	 * @pre type must be #NewsType::Economy, #NewsType::Subsidies, or #NewsType::General.
	 * @pre text != null.
	 * @pre company == COMPANY_INVALID || ResolveCompanyID(company) != COMPANY_INVALID.
	 * @pre The \a reference condition must be fulfilled.
	 * @pre ScriptCompanyMode::IsDeity().
	 */
	static bool Create(NewsType type, Text *text, ScriptCompany::CompanyID company, NewsReferenceType ref_type, SQInteger reference);
};

#endif /* SCRIPT_NEWS_HPP */
