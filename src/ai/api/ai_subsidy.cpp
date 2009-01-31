/* $Id$ */

/** @file ai_subsidy.cpp Implementation of AISubsidy. */

#include "ai_subsidy.hpp"
#include "ai_date.hpp"
#include "../../economy_func.h"
#include "../../station_base.h"
#include "../../cargotype.h"

/* static */ bool AISubsidy::IsValidSubsidy(SubsidyID subsidy_id)
{
	return subsidy_id < lengthof(_subsidies) && _subsidies[subsidy_id].cargo_type != CT_INVALID;
}

/* static */ bool AISubsidy::IsAwarded(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return false;

	return _subsidies[subsidy_id].age >= 12;
}

/* static */ AICompany::CompanyID AISubsidy::GetAwardedTo(SubsidyID subsidy_id)
{
	if (!IsAwarded(subsidy_id)) return AICompany::COMPANY_INVALID;

	return (AICompany::CompanyID)((byte)GetStation(_subsidies[subsidy_id].from)->owner);
}

/* static */ int32 AISubsidy::GetExpireDate(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return -1;

	int year = AIDate::GetYear(AIDate::GetCurrentDate());
	int month = AIDate::GetMonth(AIDate::GetCurrentDate());

	if (IsAwarded(subsidy_id)) {
		month += 24 - _subsidies[subsidy_id].age;
	} else {
		month += 12 - _subsidies[subsidy_id].age;
	}

	year += (month - 1) / 12;
	month = ((month - 1) % 12) + 1;

	return AIDate::GetDate(year, month, 1);
}

/* static */ CargoID AISubsidy::GetCargoType(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return CT_INVALID;

	return _subsidies[subsidy_id].cargo_type;
}

/* static */ bool AISubsidy::SourceIsTown(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id) || IsAwarded(subsidy_id)) return false;

	return GetCargo(GetCargoType(subsidy_id))->town_effect == TE_PASSENGERS ||
	       GetCargo(GetCargoType(subsidy_id))->town_effect == TE_MAIL;
}

/* static */ int32 AISubsidy::GetSource(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return INVALID_STATION;

	return _subsidies[subsidy_id].from;
}

/* static */ bool AISubsidy::DestinationIsTown(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id) || IsAwarded(subsidy_id)) return false;

	switch (GetCargo(GetCargoType(subsidy_id))->town_effect) {
		case TE_PASSENGERS:
		case TE_MAIL:
		case TE_GOODS:
		case TE_FOOD:
			return true;
		default:
			return false;
	}
}

/* static */ int32 AISubsidy::GetDestination(SubsidyID subsidy_id)
{
	if (!IsValidSubsidy(subsidy_id)) return INVALID_STATION;

	return _subsidies[subsidy_id].to;
}
