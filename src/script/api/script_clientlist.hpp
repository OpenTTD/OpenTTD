/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_clientlist.hpp List all the TODO. */

#ifndef SCRIPT_CLIENTLIST_HPP
#define SCRIPT_CLIENTLIST_HPP

#include "script_list.hpp"
#include "script_company.hpp"


/**
 * Creates a list of clients that are currently in game.
 * @api game
 * @ingroup ScriptList
 */
class ScriptClientList : public ScriptList {
public:
	ScriptClientList();
};

/**
 * Creates a list of clients that are playing in the company.
 * @api game
 * @ingroup ScriptList
 */
class ScriptClientList_Company : public ScriptList {
public:
	/**
	 * @param company_id The company to list clients for.
	 */
	ScriptClientList_Company(ScriptCompany::CompanyID company);
};

#endif /* SCRIPT_CIENTLIST_HPP */
