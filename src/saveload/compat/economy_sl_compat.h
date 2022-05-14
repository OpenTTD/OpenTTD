/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file economy_sl_compat.h Loading for economy chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_ECONOMY_H
#define SAVELOAD_COMPAT_ECONOMY_H

#include "../saveload.h"

/** Original field order for _economy_desc. */
const SaveLoadCompat _economy_sl_compat[] = {
	SLC_NULL(4, SL_MIN_VERSION, SLV_65),
	SLC_NULL(8, SLV_65, SLV_144),
	SLC_VAR("old_max_loan_unround"),
	SLC_VAR("old_max_loan_unround_fract"),
	SLC_VAR("inflation_prices"),
	SLC_VAR("inflation_payment"),
	SLC_VAR("fluct"),
	SLC_VAR("interest_rate"),
	SLC_VAR("infl_amount"),
	SLC_VAR("infl_amount_pr"),
	SLC_VAR("industry_daily_change_counter"),
};

/** Original field order for _cargopayment_desc. */
const SaveLoadCompat _cargopayment_sl_compat[] = {
	SLC_VAR("front"),
	SLC_VAR("route_profit"),
	SLC_VAR("visual_profit"),
	SLC_VAR("visual_transfer"),
};

#endif /* SAVELOAD_COMPAT_ECONOMY_H */
