/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_storypagelist.h List all story pages. */

#ifndef SCRIPT_STORYPAGELIST_HPP
#define SCRIPT_STORYPAGELIST_HPP

#include "script_list.h"
#include "script_company.h"

/**
 * Create a list of all story pages.
 * @api game
 * @ingroup ScriptList
 */
class ScriptStoryPageList : public ScriptList {
public:
	/**
	 * @param company The company to list story pages for, or ScriptCompany::COMPANY_INVALID to only show global pages. Global pages are always included independent of this parameter.
	 */
	ScriptStoryPageList(ScriptCompany::CompanyID company);
};

#endif /* SCRIPT_STORYPAGELIST_HPP */
