/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file company_sl_compat.h Loading of company chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_COMPANY_H
#define SAVELOAD_COMPAT_COMPANY_H

#include "../saveload.h"

/** Original field order for SlCompanyOldAIBuildRec. */
const SaveLoadCompat _company_old_ai_buildrec_compat[] = {
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
	SLC_NULL(4, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::NoAI),
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
	SLC_NULL(4, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::NoAI),
	SLC_NULL(8, SaveLoadVersion::MinVersion, SaveLoadVersion::NoAI),
};

/** Original field order for SlCompanyOldAI. */
const SaveLoadCompat _company_old_ai_compat[] = {
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::NoAI),
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::LargerAIStateCounter),
	SLC_NULL(4, SaveLoadVersion::LargerAIStateCounter, SaveLoadVersion::NoAI),
	SLC_NULL(8, SaveLoadVersion::MinVersion, SaveLoadVersion::NoAI),
	SLC_VAR("num_build_rec"),
	SLC_NULL(3, SaveLoadVersion::MinVersion, SaveLoadVersion::NoAI),

	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
	SLC_NULL(4, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::NoAI),
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
	SLC_NULL(4, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::NoAI),
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::NoAI),

	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
	SLC_NULL(4, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::NoAI),
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
	SLC_NULL(4, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::NoAI),
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::NoAI),

	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::MoreCargoPackets),
	SLC_NULL(4, SaveLoadVersion::MoreCargoPackets, SaveLoadVersion::NoAI),

	SLC_NULL(18, SaveLoadVersion::MinVersion, SaveLoadVersion::NoAI),
	SLC_NULL(20, SaveLoadVersion::MinVersion, SaveLoadVersion::NoAI),
	SLC_NULL(32, SaveLoadVersion::MinVersion, SaveLoadVersion::NoAI),

	SLC_NULL(64, SaveLoadVersion::VehicleCurrencyStationChanges, SaveLoadVersion::NoAI),
	SLC_VAR("buildrec"),
};

/** Original field order for SlCompanySettings. */
const SaveLoadCompat _company_settings_compat[] = {
	SLC_NULL(512, SaveLoadVersion::EngineRenew, SaveLoadVersion::EngineRenewPool),
	SLC_VAR("engine_renew_list"),
	SLC_VAR("settings.engine_renew"),
	SLC_VAR("settings.engine_renew_months"),
	SLC_VAR("settings.engine_renew_money"),
	SLC_VAR("settings.renew_keep_length"),
	SLC_VAR("settings.vehicle.servint_ispercent"),
	SLC_VAR("settings.vehicle.servint_trains"),
	SLC_VAR("settings.vehicle.servint_roadveh"),
	SLC_VAR("settings.vehicle.servint_aircraft"),
	SLC_VAR("settings.vehicle.servint_ships"),
	SLC_NULL(63, SaveLoadVersion::VehicleCurrencyStationChanges, SaveLoadVersion::ReorderUnmovableRemoveReserved),
};

/** Original field order for SlCompanyEconomy. */
const SaveLoadCompat _company_economy_compat[] = {
	SLC_VAR("income"),
	SLC_VAR("expenses"),
	SLC_VAR("company_value"),
	SLC_VAR("delivered_cargo[NUM_CARGO - 1]"),
	SLC_VAR("delivered_cargo"),
	SLC_VAR("performance_history"),
};

/** Original field order for SlCompanyLiveries. */
const SaveLoadCompat _company_liveries_compat[] = {
	SLC_VAR("in_use"),
	SLC_VAR("colour1"),
	SLC_VAR("colour2"),
};

/** Original field order for company_desc. */
const SaveLoadCompat _company_sl_compat[] = {
	SLC_VAR("name_2"),
	SLC_VAR("name_1"),
	SLC_VAR("name"),
	SLC_VAR("president_name_1"),
	SLC_VAR("president_name_2"),
	SLC_VAR("president_name"),
	SLC_VAR("face"),
	SLC_VAR("money"),
	SLC_VAR("current_loan"),
	SLC_VAR("colour"),
	SLC_VAR("money_fraction"),
	SLC_NULL(1, SaveLoadVersion::MinVersion, SaveLoadVersion::VeryLowTownIndustryNumber),
	SLC_VAR("block_preview"),
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::FixCompanyCargoTypes),
	SLC_NULL(4, SaveLoadVersion::FixCompanyCargoTypes, SaveLoadVersion::CountIndividualCargoes),
	SLC_VAR("location_of_HQ"),
	SLC_VAR("last_build_coordinate"),
	SLC_VAR("inaugurated_year"),
	SLC_NULL(4, SaveLoadVersion::MinVersion, SaveLoadVersion::TableChunks),
	SLC_VAR("num_valid_stat_ent"),
	SLC_VAR("months_of_bankruptcy"),
	SLC_VAR("bankrupt_asked"),
	SLC_VAR("bankrupt_timeout"),
	SLC_VAR("bankrupt_value"),
	SLC_VAR("yearly_expenses"),
	SLC_VAR("is_ai"),
	SLC_NULL(1, SaveLoadVersion::NoAI, SaveLoadVersion::SplitHQ),
	SLC_NULL(1, SaveLoadVersion::TownTolerancePauseMode, SaveLoadVersion::Yapp),
	SLC_VAR("terraform_limit"),
	SLC_VAR("clear_limit"),
	SLC_VAR("tree_limit"),
	SLC_VAR("settings"),
	SLC_VAR("old_ai"),
	SLC_VAR("cur_economy"),
	SLC_VAR("old_economy"),
	SLC_VAR("liveries"),
};

#endif /* SAVELOAD_COMPAT_COMPANY_H */
