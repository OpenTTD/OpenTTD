/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_subsidy.cpp Implementation of AISubsidy. */

#include "../../stdafx.h"
#include "ai_subsidy.hpp"
#include "ai_date.hpp"
#include "../../subsidy_base.h"
#include "../../station_base.h"

/* static */ bool AISubsidy::IsValidSubsidy(SubsidyID subsidy_id)
{
	return ::Subsidy::IsValidID(subsidy_id);
}

/* static */ bool AISubsidy::IsAwarded(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return false;

	return ::Subsidy::Get(subsidy_id)->IsAwarded();
}

/* static */ AICompany::CompanyID AISubsidy::GetAwardedTo(SubsidyID subsidy_id)
{
	if (!IsAwarded(subsidy_id)) return AICompany::COMPANY_INVALID;

	return (AICompany::CompanyID)((byte)::Subsidy::Get(subsidy_id)->awarded);
}

/* static */ int32 AISubsidy::GetExpireDate(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return -1;

	int year = AIDate::GetYear(AIDate::GetCurrentDate());
	int month = AIDate::GetMonth(AIDate::GetCurrentDate());

	month += ::Subsidy::Get(subsidy_id)->remaining;

	year += (month - 1) / 12;
	month = ((month - 1) % 12) + 1;

	return AIDate::GetDate(year, month, 1);
}

/* static */ CargoID AISubsidy::GetCargoType(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return CT_INVALID;

	return ::Subsidy::Get(subsidy_id)->cargo_type;
}

/* static */ AISubsidy::SubsidyParticipantType AISubsidy::GetSourceType(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return SPT_INVALID;

	return (SubsidyParticipantType)(uint)::Subsidy::Get(subsidy_id)->src_type;
}

/* static */ int32 AISubsidy::GetSourceIndex(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return INVALID_STATION;

	return ::Subsidy::Get(subsidy_id)->src;
}

/* static */ AISubsidy::SubsidyParticipantType AISubsidy::GetDestinationType(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return SPT_INVALID;

	return (SubsidyParticipantType)(uint)::Subsidy::Get(subsidy_id)->dst_type;
}

/* static */ int32 AISubsidy::GetDestinationIndex(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return INVALID_STATION;

	return ::Subsidy::Get(subsidy_id)->dst;
}
