/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_companymode.hpp Switch the company. */

#ifndef SCRIPT_COMPANYMODE_HPP
#define SCRIPT_COMPANYMODE_HPP

#include "script_object.hpp"

/**
 * Class to switch the current company.
 * If you create an instance of this class, the company will be switched.
 *  The original company is stored and recovered from when ever the
 *  instance is destroyed.
 * All actions performed within the scope of this mode, will be executed
 *  on behalf of the company you switched to. This includes any costs
 *  attached to the action performed. If the company does not have the
 *  funds the action will be aborted. In other words, this is like the
 *  real player is executing the commands.
 * If the company is not valid during an action, the error
 *  ERR_PRECONDITION_INVALID_COMPANY will be returned. You can switch to
 *  invalid companies, or a company can become invalid (bankrupt) while you
 *  are switched to it.
 * @api game
 */
class ScriptCompanyMode : public ScriptObject {
private:
	CompanyID last_company; ///< The previous company we were in.

public:
	/**
	 * Creating instance of this class switches the company used for queries
	 *  and commands.
	 * @param company The new company to switch to.
	 * @note When the instance is destroyed, he restores the company that was
	 *   current when the instance was created!
	 */
	ScriptCompanyMode(int company);

	/**
	 * Destroying this instance reset the company to that what it was
	 *   in when the instance was created.
	 */
	~ScriptCompanyMode();
};

#endif /* SCRIPT_COMPANYMODE_HPP */
