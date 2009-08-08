/* $Id$ */

/** @file ai_subsidy.cpp Implementation of AISubsidy. */

#include "ai_subsidy.hpp"
#include "ai_date.hpp"
#include "ai_log.hpp"
#include "../../subsidy_base.h"
#include "../../station_base.h"
#include "../../cargotype.h"

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

/* static */ bool AISubsidy::SourceIsTown(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id) || IsAwarded(subsidy_id)) return false;

	return ::Subsidy::Get(subsidy_id)->src_type == ST_TOWN;
}

/* static */ int32 AISubsidy::GetSource(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return INVALID_STATION;

	if (IsAwarded(subsidy_id)) {
		AILog::Error("AISubsidy::GetSource returned INVALID_STATION due to internal changes in the Subsidy logic.");
		return INVALID_STATION;
	}

	return ::Subsidy::Get(subsidy_id)->src;
}

/* static */ bool AISubsidy::DestinationIsTown(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id) || IsAwarded(subsidy_id)) return false;

	return ::Subsidy::Get(subsidy_id)->dst_type == ST_TOWN;
}

/* static */ int32 AISubsidy::GetDestination(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return INVALID_STATION;

	if (IsAwarded(subsidy_id)) {
		AILog::Error("AISubsidy::GetDestination returned INVALID_STATION due to internal changes in the Subsidy logic.");
		return INVALID_STATION;
	}

	return ::Subsidy::Get(subsidy_id)->dst;
}
