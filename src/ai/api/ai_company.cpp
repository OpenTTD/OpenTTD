/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_company.cpp Implementation of AICompany. */

#include "../../stdafx.h"
#include "ai_company.hpp"
#include "ai_error.hpp"
#include "../../command_func.h"
#include "../../company_func.h"
#include "../../company_base.h"
#include "../../company_manager_face.h"
#include "../../economy_func.h"
#include "../../object_type.h"
#include "../../strings_func.h"
#include "../../tile_map.h"
#include "../../string_func.h"
#include "../../settings_func.h"
#include "table/strings.h"

/* static */ AICompany::CompanyID AICompany::ResolveCompanyID(AICompany::CompanyID company)
{
	if (company == COMPANY_SELF) return (CompanyID)((byte)_current_company);

	return ::Company::IsValidID((::CompanyID)company) ? company : COMPANY_INVALID;
}

/* static */ bool AICompany::IsMine(AICompany::CompanyID company)
{
	return ResolveCompanyID(company) == ResolveCompanyID(COMPANY_SELF);
}

/* static */ bool AICompany::SetName(const char *name)
{
	EnforcePrecondition(false, !::StrEmpty(name));
	EnforcePreconditionCustomError(false, ::Utf8StringLength(name) < MAX_LENGTH_COMPANY_NAME_CHARS, AIError::ERR_PRECONDITION_STRING_TOO_LONG);

	return AIObject::DoCommand(0, 0, 0, CMD_RENAME_COMPANY, name);
}

/* static */ char *AICompany::GetName(AICompany::CompanyID company)
{
	company = ResolveCompanyID(company);
	if (company == COMPANY_INVALID) return NULL;

	static const int len = 64;
	char *company_name = MallocT<char>(len);

	::SetDParam(0, company);
	::GetString(company_name, STR_COMPANY_NAME, &company_name[len - 1]);
	return company_name;
}

/* static */ bool AICompany::SetPresidentName(const char *name)
{
	EnforcePrecondition(false, !::StrEmpty(name));

	return AIObject::DoCommand(0, 0, 0, CMD_RENAME_PRESIDENT, name);
}

/* static */ char *AICompany::GetPresidentName(AICompany::CompanyID company)
{
	company = ResolveCompanyID(company);

	static const int len = 64;
	char *president_name = MallocT<char>(len);
	if (company != COMPANY_INVALID) {
		::SetDParam(0, company);
		::GetString(president_name, STR_PRESIDENT_NAME, &president_name[len - 1]);
	} else {
		*president_name = '\0';
	}

	return president_name;
}

/* static */ bool AICompany::SetPresidentGender(Gender gender)
{
	EnforcePrecondition(false, gender == GENDER_MALE || gender == GENDER_FEMALE);
	EnforcePrecondition(false, GetPresidentGender(AICompany::COMPANY_SELF) != gender);

	CompanyManagerFace cmf;
	GenderEthnicity ge = (GenderEthnicity)((gender == GENDER_FEMALE ? (1 << ::GENDER_FEMALE) : 0) | (::InteractiveRandom() & (1 << ETHNICITY_BLACK)));
	RandomCompanyManagerFaceBits(cmf, ge, false);

	return AIObject::DoCommand(0, 0, cmf, CMD_SET_COMPANY_MANAGER_FACE);
}

/* static */ AICompany::Gender AICompany::GetPresidentGender(CompanyID company)
{
	company = ResolveCompanyID(company);
	if (company == COMPANY_INVALID) return GENDER_INVALID;

	GenderEthnicity ge = (GenderEthnicity)GetCompanyManagerFaceBits(Company::Get(company)->face, CMFV_GEN_ETHN, GE_WM);
	return HasBit(ge, ::GENDER_FEMALE) ? GENDER_FEMALE : GENDER_MALE;
}

/* static */ Money AICompany::GetCompanyValue(AICompany::CompanyID company)
{
	company = ResolveCompanyID(company);
	if (company == COMPANY_INVALID) return -1;

	return ::CalculateCompanyValue(::Company::Get((CompanyID)company));
}

/* static */ Money AICompany::GetBankBalance(AICompany::CompanyID company)
{
	company = ResolveCompanyID(company);
	if (company == COMPANY_INVALID) return -1;

	return ::Company::Get((CompanyID)company)->money;
}

/* static */ Money AICompany::GetLoanAmount()
{
	return ::Company::Get(_current_company)->current_loan;
}

/* static */ Money AICompany::GetMaxLoanAmount()
{
	return _economy.max_loan;
}

/* static */ Money AICompany::GetLoanInterval()
{
	return LOAN_INTERVAL;
}

/* static */ bool AICompany::SetLoanAmount(int32 loan)
{
	EnforcePrecondition(false, loan >= 0);
	EnforcePrecondition(false, (loan % GetLoanInterval()) == 0);
	EnforcePrecondition(false, loan <= GetMaxLoanAmount());
	EnforcePrecondition(false, (loan - GetLoanAmount() + GetBankBalance(COMPANY_SELF)) >= 0);

	if (loan == GetLoanAmount()) return true;

	return AIObject::DoCommand(0,
			abs(loan - GetLoanAmount()), 2,
			(loan > GetLoanAmount()) ? CMD_INCREASE_LOAN : CMD_DECREASE_LOAN);
}

/* static */ bool AICompany::SetMinimumLoanAmount(int32 loan)
{
	EnforcePrecondition(false, loan >= 0);

	int32 over_interval = loan % GetLoanInterval();
	if (over_interval != 0) loan += GetLoanInterval() - over_interval;

	EnforcePrecondition(false, loan <= GetMaxLoanAmount());

	SetLoanAmount(loan);

	return GetLoanAmount() == loan;
}

/* static */ bool AICompany::BuildCompanyHQ(TileIndex tile)
{
	EnforcePrecondition(false, ::IsValidTile(tile));

	return AIObject::DoCommand(tile, OBJECT_HQ, 0, CMD_BUILD_OBJECT);
}

/* static */ TileIndex AICompany::GetCompanyHQ(CompanyID company)
{
	company = ResolveCompanyID(company);
	if (company == COMPANY_INVALID) return INVALID_TILE;

	TileIndex loc = ::Company::Get((CompanyID)company)->location_of_HQ;
	return (loc == 0) ? INVALID_TILE : loc;
}

/* static */ bool AICompany::SetAutoRenewStatus(bool autorenew)
{
	return AIObject::DoCommand(0, ::GetCompanySettingIndex("company.engine_renew"), autorenew ? 1 : 0, CMD_CHANGE_COMPANY_SETTING);
}

/* static */ bool AICompany::GetAutoRenewStatus(CompanyID company)
{
	company = ResolveCompanyID(company);
	if (company == COMPANY_INVALID) return false;

	return ::Company::Get((CompanyID)company)->settings.engine_renew;
}

/* static */ bool AICompany::SetAutoRenewMonths(int16 months)
{
	return AIObject::DoCommand(0, ::GetCompanySettingIndex("company.engine_renew_months"), months, CMD_CHANGE_COMPANY_SETTING);
}

/* static */ int16 AICompany::GetAutoRenewMonths(CompanyID company)
{
	company = ResolveCompanyID(company);
	if (company == COMPANY_INVALID) return 0;

	return ::Company::Get((CompanyID)company)->settings.engine_renew_months;
}

/* static */ bool AICompany::SetAutoRenewMoney(uint32 money)
{
	return AIObject::DoCommand(0, ::GetCompanySettingIndex("company.engine_renew_money"), money, CMD_CHANGE_COMPANY_SETTING);
}

/* static */ uint32 AICompany::GetAutoRenewMoney(CompanyID company)
{
	company = ResolveCompanyID(company);
	if (company == COMPANY_INVALID) return 0;

	return ::Company::Get((CompanyID)company)->settings.engine_renew_money;
}
