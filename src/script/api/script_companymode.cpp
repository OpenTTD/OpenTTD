/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_companymode.cpp Implementation of ScriptCompanyMode. */

#include "../../stdafx.h"
#include "../../company_base.h"
#include "script_companymode.h"

#include "../../safeguards.h"

ScriptCompanyMode::ScriptCompanyMode(SQInteger company)
{
	if (company < OWNER_BEGIN || company >= MAX_COMPANIES) company = INVALID_COMPANY;
	if (!::Company::IsValidID(company)) company = INVALID_COMPANY;

	this->last_company = ScriptObject::GetCompany();
	ScriptObject::SetCompany((::CompanyID)company);
}

ScriptCompanyMode::~ScriptCompanyMode()
{
	ScriptObject::SetCompany(this->last_company);
}

/* static */ bool ScriptCompanyMode::IsValid()
{
	return ::Company::IsValidID(ScriptObject::GetCompany());
}

/* static */ bool ScriptCompanyMode::IsDeity()
{
	return ScriptObject::GetCompany() == OWNER_DEITY;
}
