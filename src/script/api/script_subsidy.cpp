/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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
#include "../../subsidy_cmd.h"

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

/* static */ bool ScriptSubsidy::Create(CargoType cargo_type, SubsidyParticipantType from_type, SQInteger from_id, SubsidyParticipantType to_type, SQInteger to_id)
{
	EnforceDeityMode(false);
	EnforcePrecondition(false, ScriptCargo::IsValidCargo(cargo_type));
	EnforcePrecondition(false, from_type == SPT_INDUSTRY || from_type == SPT_TOWN);
	EnforcePrecondition(false, to_type == SPT_INDUSTRY || to_type == SPT_TOWN);
	EnforcePrecondition(false, (from_type == SPT_INDUSTRY && ScriptIndustry::IsValidIndustry(static_cast<IndustryID>(from_id))) || (from_type == SPT_TOWN && ScriptTown::IsValidTown(static_cast<TownID>(from_id))));
	EnforcePrecondition(false, (to_type == SPT_INDUSTRY && ScriptIndustry::IsValidIndustry(static_cast<IndustryID>(to_id))) || (to_type == SPT_TOWN && ScriptTown::IsValidTown(static_cast<TownID>(to_id))));

	Source from{static_cast<SourceID>(from_id), static_cast<SourceType>(from_type)};
	Source to{static_cast<SourceID>(to_id), static_cast<SourceType>(to_type)};

	return ScriptObject::Command<CMD_CREATE_SUBSIDY>::Do(cargo_type, from, to);
}

/* static */ ScriptCompany::CompanyID ScriptSubsidy::GetAwardedTo(SubsidyID subsidy_id)
{
	if (!IsAwarded(subsidy_id)) return ScriptCompany::COMPANY_INVALID;

	return ScriptCompany::ToScriptCompanyID(::Subsidy::Get(subsidy_id)->awarded);
}

/* static */ ScriptDate::Date ScriptSubsidy::GetExpireDate(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return ScriptDate::DATE_INVALID;

	TimerGameEconomy::YearMonthDay ymd = TimerGameEconomy::ConvertDateToYMD(TimerGameEconomy::date);
	ymd.day = 1;
	auto m = ymd.month + ::Subsidy::Get(subsidy_id)->remaining;
	ymd.month = m % 12;
	ymd.year += TimerGameEconomy::Year{m / 12};

	return (ScriptDate::Date)TimerGameEconomy::ConvertYMDToDate(ymd.year, ymd.month, ymd.day).base();
}

/* static */ CargoType ScriptSubsidy::GetCargoType(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return INVALID_CARGO;

	return ::Subsidy::Get(subsidy_id)->cargo_type;
}

/* static */ ScriptSubsidy::SubsidyParticipantType ScriptSubsidy::GetSourceType(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return SPT_INVALID;

	return static_cast<SubsidyParticipantType>(::Subsidy::Get(subsidy_id)->src.type);
}

/* static */ SQInteger ScriptSubsidy::GetSourceIndex(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return Source::Invalid;

	return ::Subsidy::Get(subsidy_id)->src.id;
}

/* static */ ScriptSubsidy::SubsidyParticipantType ScriptSubsidy::GetDestinationType(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return SPT_INVALID;

	return static_cast<SubsidyParticipantType>(::Subsidy::Get(subsidy_id)->dst.type);
}

/* static */ SQInteger ScriptSubsidy::GetDestinationIndex(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return Source::Invalid;

	return ::Subsidy::Get(subsidy_id)->dst.id;
}
