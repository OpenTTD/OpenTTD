/* $Id$ */

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
	 * Enumeration for corners of tiles.
	 */
	enum NewsType {
		/* Note: these values represent part of the in-game NewsSubtype enum */
		NT_ARRIVAL_COMPANY   = ::NS_ARRIVAL_COMPANY,  ///< Category arrival for own company.
		NT_ARRIVAL_OTHER     = ::NS_ARRIVAL_OTHER,    ///< Category arrival for other companies.
		NT_ACCIDENT          = ::NS_ACCIDENT,         ///< Category accident.
		NT_COMPANY_TROUBLE   = ::NS_COMPANY_TROUBLE,  ///< Category company in trouble.
		NT_COMPANY_MERGER    = ::NS_COMPANY_MERGER,   ///< Category company merger.
		NT_COMPANY_BANKRUPT  = ::NS_COMPANY_BANKRUPT, ///< Category company bankrupt.
		NT_COMPANY_NEW       = ::NS_COMPANY_NEW,      ///< Category company new.
		NT_INDUSTRY_OPEN     = ::NS_INDUSTRY_OPEN,    ///< Category industry open.
		NT_INDUSTRY_CLOSE    = ::NS_INDUSTRY_CLOSE,   ///< Category industry close.
		NT_ECONOMY           = ::NS_ECONOMY,          ///< Category economy.
		NT_INDUSTRY_COMPANY  = ::NS_INDUSTRY_COMPANY, ///< Category industry changes for own company.
		NT_INDUSTRY_OTHER    = ::NS_INDUSTRY_OTHER,   ///< Category industry changes for other companies.
		NT_INDUSTRY_NOBODY   = ::NS_INDUSTRY_NOBODY,  ///< Category industry changes for nobody.
		NT_ADVICE            = ::NS_ADVICE,           ///< Category advice.
		NT_NEW_VEHICLES      = ::NS_NEW_VEHICLES,     ///< Category new vehicle.
		NT_ACCEPTANCE        = ::NS_ACCEPTANCE,       ///< Category acceptance changes.
		NT_SUBSIDIES         = ::NS_SUBSIDIES,        ///< Category subsidies.
		NT_GENERAL           = ::NS_GENERAL,          ///< Category general.
	};

	/**
	 * Create a news messages for a company.
	 * @param type The type of the news.
	 * @param text The text message to show (can be either a raw string, or a ScriptText object).
	 * @param company The company, or COMPANY_INVALID for all companies.
	 * @return True if the action succeeded.
	 * @pre text != NULL.
	 * @pre company == COMPANY_INVALID || ResolveCompanyID(company) != COMPANY_INVALID.
	 */
	static bool Create(NewsType type, Text *text, ScriptCompany::CompanyID company);
};

#endif /* SCRIPT_NEWS_HPP */
