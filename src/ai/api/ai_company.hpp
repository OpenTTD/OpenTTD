/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_company.hpp Everything to query a company's financials and statistics or build company related buildings. */

#ifndef AI_COMPANY_HPP
#define AI_COMPANY_HPP

#include "ai_object.hpp"

/**
 * Class that handles all company related functions.
 */
class AICompany : public AIObject {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AICompany"; }

	/** The range of possible quarters to get company information of. */
	enum Quarter {
		CURRENT_QUARTER = 0,                     ///< The current quarter.
		EARLIEST_QUARTER = MAX_HISTORY_QUARTERS, ///< The earliest quarter company information is available for.
	};

	/** Different constants related to CompanyID. */
	enum CompanyID {
		COMPANY_FIRST   = 0,               ///< The first available company.
		COMPANY_LAST    = ::MAX_COMPANIES, ///< The last available company.
		COMPANY_SELF    = 254,             ///< Constant that gets resolved to the correct company index for your company.
		COMPANY_INVALID = -1,              ///< An invalid company.
	};

	/** Possible genders for company presidents. */
	enum Gender {
		GENDER_MALE,         ///< A male person.
		GENDER_FEMALE,       ///< A female person.
		GENDER_INVALID = -1, ///< An invalid gender.
	};

	/**
	 * Resolved the given company index to the correct index for the company. If
	 *  the company index was COMPANY_SELF it will be resolved to the index of
	 *  your company. If the company with the given index does not exist it will
	 *  return COMPANY_INVALID.
	 * @param company The company index to resolve.
	 * @return The resolved company index.
	 */
	static CompanyID ResolveCompanyID(CompanyID company);

	/**
	 * Check if a CompanyID is your CompanyID, to ease up checks.
	 * @param company The company index to check.
	 * @return True if and only if this company is your CompanyID.
	 */
	static bool IsMine(CompanyID company);

	/**
	 * Set the name of your company.
	 * @param name The new name of the company.
	 * @pre 'name' must have at least one character.
	 * @pre 'name' must have at most 30 characters.
	 * @exception AIError::ERR_NAME_IS_NOT_UNIQUE
	 * @return True if the name was changed.
	 */
	static bool SetName(const char *name);

	/**
	 * Get the name of the given company.
	 * @param company The company to get the name for.
	 * @pre ResolveCompanyID(company) != COMPANY_INVALID.
	 * @return The name of the given company.
	 */
	static char *GetName(CompanyID company);

	/**
	 * Set the name of your president.
	 * @param name The new name of the president.
	 * @pre 'name' must have at least one character.
	 * @exception AIError::ERR_NAME_IS_NOT_UNIQUE
	 * @return True if the name was changed.
	 */
	static bool SetPresidentName(const char *name);

	/**
	 * Get the name of the president of the given company.
	 * @param company The company to get the president's name for.
	 * @pre ResolveCompanyID(company) != COMPANY_INVALID.
	 * @return The name of the president of the given company.
	 */
	static char *GetPresidentName(CompanyID company);

	/**
	 * Set the gender of the president of your company.
	 * @param gender The new gender for your president.
	 * @pre GetPresidentGender(AICompany.COMPANY_SELF) != gender.
	 * @return True if the gender was changed.
	 * @note When successful a random face will be created.
	 */
	static bool SetPresidentGender(Gender gender);

	/**
	 * Get the gender of the president of the given company.
	 * @param company The company to get the presidents gender off.
	 * @return The gender of the president.
	 */
	static Gender GetPresidentGender(CompanyID company);

	/**
	 * Sets the amount to loan.
	 * @param loan The amount to loan (multiplier of GetLoanInterval()).
	 * @pre 'loan' must be non-negative.
	 * @pre GetLoanInterval() must be a multiplier of 'loan'.
	 * @pre 'loan' must be below GetMaxLoanAmount().
	 * @pre 'loan' - GetLoanAmount() + GetBankBalance() must be non-negative.
	 * @return True if the loan could be set to your requested amount.
	 */
	static bool SetLoanAmount(int32 loan);

	/**
	 * Sets the minimum amount to loan, i.e. the given amount of loan rounded up.
	 * @param loan The amount to loan (any positive number).
	 * @pre 'loan' must be non-negative.
	 * @pre 'loan' must be below GetMaxLoanAmount().
	 * @return True if we could allocate a minimum of 'loan' loan.
	 */
	static bool SetMinimumLoanAmount(int32 loan);

	/**
	 * Gets the amount your company have loaned.
	 * @return The amount loaned money.
	 * @post The return value is always non-negative.
	 * @post GetLoanInterval() is always a multiplier of the return value.
	 */
	static Money GetLoanAmount();

	/**
	 * Gets the maximum amount your company can loan.
	 * @return The maximum amount your company can loan.
	 * @post The return value is always non-negative.
	 * @post GetLoanInterval() is always a multiplier of the return value.
	 */
	static Money GetMaxLoanAmount();

	/**
	 * Gets the interval/loan step.
	 * @return The loan step.
	 * @post Return value is always positive.
	 */
	static Money GetLoanInterval();

	/**
	 * Gets the bank balance. In other words, the amount of money the given company can spent.
	 * @param company The company to get the bank balance of.
	 * @pre ResolveCompanyID(company) != COMPANY_INVALID.
	 * @return The actual bank balance.
	 */
	static Money GetBankBalance(CompanyID company);

	/**
	 * Get the income of the company in the given quarter.
	 * @param company The company to get the quarterly income of.
	 * @param quarter The quarter to get the income of.
	 * @pre ResolveCompanyID(company) != COMPANY_INVALID.
	 * @pre quarter <= EARLIEST_QUARTER.
	 * @return The bruto income of the company in the given quarter.
	 */
	static Money GetQuarterlyIncome(CompanyID company, uint32 quarter);

	/**
	 * Get the expenses of the company in the given quarter.
	 * @param company The company to get the quarterly expenses of.
	 * @param quarter The quarter to get the expenses of.
	 * @pre ResolveCompanyID(company) != COMPANY_INVALID.
	 * @pre quarter <= EARLIEST_QUARTER.
	 * @return The expenses of the company in the given quarter.
	 */
	static Money GetQuarterlyExpenses(CompanyID company, uint32 quarter);

	/**
	 * Get the amount of cargo delivered by the given company in the given quarter.
	 * @param company The company to get the amount of delivered cargo of.
	 * @param quarter The quarter to get the amount of delivered cargo of.
	 * @pre ResolveCompanyID(company) != COMPANY_INVALID.
	 * @pre quarter <= EARLIEST_QUARTER.
	 * @return The amount of cargo delivered by the given company in the given quarter.
	 */
	static int32 GetQuarterlyCargoDelivered(CompanyID company, uint32 quarter);

	/**
	 * Get the performance rating of the given company in the given quarter.
	 * @param company The company to get the performance rating of.
	 * @param quarter The quarter to get the performance rating of.
	 * @pre ResolveCompanyID(company) != COMPANY_INVALID.
	 * @pre quarter <= EARLIEST_QUARTER.
	 * @pre quarter != CURRENT_QUARTER.
	 * @note The performance rating is calculated after every quarter, so the value for CURRENT_QUARTER is undefined.
	 * @return The performance rating of the given company in the given quarter.
	 */
	static int32 GetQuarterlyPerformanceRating(CompanyID company, uint32 quarter);

	/**
	 * Get the value of the company in the given quarter.
	 * @param company The company to get the value of.
	 * @param quarter The quarter to get the value of.
	 * @pre ResolveCompanyID(company) != COMPANY_INVALID.
	 * @pre quarter <= EARLIEST_QUARTER.
	 * @return The value of the company in the given quarter.
	 */
	static Money GetQuarterlyCompanyValue(CompanyID company, uint32 quarter);

	/**
	 * Build your company's HQ on the given tile.
	 * @param tile The tile to build your HQ on, this tile is the most nothern tile of your HQ.
	 * @pre AIMap::IsValidTile(tile).
	 * @exception AIError::ERR_AREA_NOT_CLEAR
	 * @exception AIError::ERR_FLAT_LAND_REQUIRED
	 * @return True if the HQ could be build.
	 * @note An HQ can not be removed, only by water or rebuilding; If an HQ is
	 *  build again, the old one is removed.
	 */
	static bool BuildCompanyHQ(TileIndex tile);

	/**
	 * Return the location of a company's HQ.
	 * @param company The company the get the HQ of.
	 * @pre ResolveCompanyID(company) != COMPANY_INVALID.
	 * @return The tile of the company's HQ, this tile is the most nothern tile
	 *  of that HQ, or AIMap::TILE_INVALID if there is no HQ yet.
	 */
	static TileIndex GetCompanyHQ(CompanyID company);

	/**
	 * Set whether autorenew is enabled for your company.
	 * @param autorenew The new autorenew status.
	 * @return True if autorenew status has been modified.
	 */
	static bool SetAutoRenewStatus(bool autorenew);

	/**
	 * Return whether autorenew is enabled for a company.
	 * @param company The company to get the autorenew status of.
	 * @pre ResolveCompanyID(company) != COMPANY_INVALID.
	 * @return True if autorenew is enabled.
	 */
	static bool GetAutoRenewStatus(CompanyID company);

	/**
	 * Set the number of months before/after max age to autorenew an engine for your company.
	 * @param months The new months between autorenew.
	 * @return True if autorenew months has been modified.
	 */
	static bool SetAutoRenewMonths(int16 months);

	/**
	 * Return the number of months before/after max age to autorenew an engine for a company.
	 * @param company The company to get the autorenew months of.
	 * @pre ResolveCompanyID(company) != COMPANY_INVALID.
	 * @return The months before/after max age of engine.
	 */
	static int16 GetAutoRenewMonths(CompanyID company);

	/**
	 * Set the minimum money needed to autorenew an engine for your company.
	 * @param money The new minimum required money for autorenew to work.
	 * @return True if autorenew money has been modified.
	 */
	static bool SetAutoRenewMoney(uint32 money);

	/**
	 * Return the minimum money needed to autorenew an engine for a company.
	 * @param company The company to get the autorenew money of.
	 * @pre ResolveCompanyID(company) != COMPANY_INVALID.
	 * @return The minimum required money for autorenew to work.
	 */
	static uint32 GetAutoRenewMoney(CompanyID company);
};

DECLARE_POSTFIX_INCREMENT(AICompany::CompanyID)

#endif /* AI_COMPANY_HPP */
