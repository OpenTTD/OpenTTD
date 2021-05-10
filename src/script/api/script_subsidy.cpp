/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_subsidy.cpp Implementation of ScriptSubsidy. */

#include "../../stdafx.h"
#include "script_subsidy.hpp"
#include "script_date.hpp"
#include "script_industry.hpp"
#include "script_town.hpp"
#include "script_error.hpp"
#include "../../subsidy_base.h"
#include "../../station_base.h"

#include "../../safeguards.h"

/* static */ bool ScriptSubsidy::IsValidSubsidy(SubsidyID subsidy_id)
{
	return ::Subsidy::IsValidID(subsidy_id);
}

/* static */ bool ScriptSubsidy::IsAwarded(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return false;

	return ::Subsidy::Get(subsidy_id)->IsAwarded();
}

/* static */ bool ScriptSubsidy::Create(CargoID cargo_type, SubsidyParticipantType from_type, uint16 from_id, SubsidyParticipantType to_type, uint16 to_id)
{
	EnforcePrecondition(false, ScriptCargo::IsValidCargo(cargo_type));
	EnforcePrecondition(false, from_type == SPT_INDUSTRY || from_type == SPT_TOWN);
	EnforcePrecondition(false, to_type == SPT_INDUSTRY || to_type == SPT_TOWN);
	EnforcePrecondition(false, (from_type == SPT_INDUSTRY && ScriptIndustry::IsValidIndustry(from_id)) || (from_type == SPT_TOWN && ScriptTown::IsValidTown(from_id)));
	EnforcePrecondition(false, (to_type == SPT_INDUSTRY && ScriptIndustry::IsValidIndustry(to_id)) || (to_type == SPT_TOWN && ScriptTown::IsValidTown(to_id)));

	return ScriptObject::DoCommand(0, from_type | (from_id << 8) | (cargo_type << 24), to_type | (to_id << 8), CMD_CREATE_SUBSIDY);
}

/* static */ ScriptCompany::CompanyID ScriptSubsidy::GetAwardedTo(SubsidyID subsidy_id)
{
	if (!IsAwarded(subsidy_id)) return ScriptCompany::COMPANY_INVALID;

	return (ScriptCompany::CompanyID)((byte)::Subsidy::Get(subsidy_id)->awarded);
}

/* static */ ScriptDate::Date ScriptSubsidy::GetExpireDate(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return ScriptDate::DATE_INVALID;

	int year = ScriptDate::GetYear(ScriptDate::GetCurrentDate());
	int month = ScriptDate::GetMonth(ScriptDate::GetCurrentDate());

	month += ::Subsidy::Get(subsidy_id)->remaining;

	year += (month - 1) / 12;
	month = ((month - 1) % 12) + 1;

	return ScriptDate::GetDate(year, month, 1);
}

/* static */ CargoID ScriptSubsidy::GetCargoType(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return CT_INVALID;

	return ::Subsidy::Get(subsidy_id)->cargo_type;
}

/* static */ ScriptSubsidy::SubsidyParticipantType ScriptSubsidy::GetSourceType(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return SPT_INVALID;

	return (SubsidyParticipantType)(uint)::Subsidy::Get(subsidy_id)->src_type;
}

/* static */ int32 ScriptSubsidy::GetSourceIndex(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return INVALID_STATION;

	return ::Subsidy::Get(subsidy_id)->src;
}

/* static */ ScriptSubsidy::SubsidyParticipantType ScriptSubsidy::GetDestinationType(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return SPT_INVALID;

	return (SubsidyParticipantType)(uint)::Subsidy::Get(subsidy_id)->dst_type;
}

/* static */ int32 ScriptSubsidy::GetDestinationIndex(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return INVALID_STATION;

	return ::Subsidy::Get(subsidy_id)->dst;
}
