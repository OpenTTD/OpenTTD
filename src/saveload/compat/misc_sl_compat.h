/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file misc_sl_compat.h Loading for misc chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_MISC_H
#define SAVELOAD_COMPAT_MISC_H

#include "../saveload.h"

/** Original field order for _date_desc. */
const SaveLoadCompat _date_sl_compat[] = {
	SLC_VAR("date"),
	SLC_VAR("date_fract"),
	SLC_VAR("tick_counter"),
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::UnifyGroundVehicles),
	SLC_VAR("age_cargo_skip_counter"),
	SLC_NULL(1, SaveLoadVersion::MinVersion, SaveLoadVersion::MoreAirportBlocks),
	SLC_VAR("cur_tileloop_tile"),
	SLC_VAR("next_disaster_start"),
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::CompanyServiceIntervals),
	SLC_VAR("random_state[0]"),
	SLC_VAR("random_state[1]"),
	SLC_NULL(1, SaveLoadVersion::MinVersion, SaveLoadVersion::LargerTownCounter),
	SLC_NULL(4, SaveLoadVersion::LargerTownCounter, SaveLoadVersion::CompanyServiceIntervals),
	SLC_VAR("company_tick_counter"),
	SLC_VAR("next_competitor_start"),
	SLC_VAR("trees_tick_counter"),
	SLC_VAR("pause_mode"),
	SLC_NULL(4, SaveLoadVersion::LargerTownIterator, SaveLoadVersion::CompanyServiceIntervals),
};

/** Original field order for _date_check_desc. */
const SaveLoadCompat _date_check_sl_compat[] = {
	SLC_VAR("date"),
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion), // date_fract
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion), // tick_counter
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::UnifyGroundVehicles),
	SLC_NULL(1, SaveLoadVersion::MinVersion, SaveLoadVersion::NewGRFCustomCargoAging), // age_cargo_skip_counter
	SLC_NULL(1, SaveLoadVersion::MinVersion, SaveLoadVersion::MoreAirportBlocks),
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops), // cur_tileloop_tile
	SLC_NULL(4, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion), // cur_tileloop_tile
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion), // disaster_delay
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::CompanyServiceIntervals),
	SLC_NULL(4, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion), // random.state[0]
	SLC_NULL(4, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion), // random.state[1]
	SLC_NULL(1, SaveLoadVersion::MinVersion, SaveLoadVersion::LargerTownCounter),
	SLC_NULL(4, SaveLoadVersion::LargerTownCounter, SaveLoadVersion::CompanyServiceIntervals),
	SLC_NULL(1, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion), // cur_company_tick_index
	SLC_NULL(2, SaveLoadVersion::MinVersion, SaveLoadVersion::NextCompetitorStartOverflow), // next_competitor_start
	SLC_NULL(4, SaveLoadVersion::NextCompetitorStartOverflow, SaveLoadVersion::MaxVersion), // next_competitor_start
	SLC_NULL(1, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion), // trees_tick_ctr
	SLC_NULL(1, SaveLoadVersion::TownTolerancePauseMode, SaveLoadVersion::MaxVersion), // pause_mode
	SLC_NULL(4, SaveLoadVersion::LargerTownIterator, SaveLoadVersion::CompanyServiceIntervals),
};

/** Original field order for _view_desc. */
const SaveLoadCompat _view_sl_compat[] = {
	SLC_VAR("x"),
	SLC_VAR("y"),
	SLC_VAR("zoom"),
};

#endif /* SAVELOAD_COMPAT_MISC_H */
