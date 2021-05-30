/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file station_sl_compat.h Loading of station chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_STATION_H
#define SAVELOAD_COMPAT_STATION_H

#include "../saveload.h"

/** Original field order for _roadstop_desc. */
const SaveLoadCompat _roadstop_sl_compat[] = {
	SLC_VAR("xy"),
	SLC_NULL(1, SL_MIN_VERSION, SLV_45),
	SLC_VAR("status"),
	SLC_NULL(4, SL_MIN_VERSION, SLV_9),
	SLC_NULL(2, SL_MIN_VERSION, SLV_45),
	SLC_NULL(1, SL_MIN_VERSION, SLV_26),
	SLC_VAR("next"),
	SLC_NULL(2, SL_MIN_VERSION, SLV_45),
	SLC_NULL(4, SL_MIN_VERSION, SLV_25),
	SLC_NULL(1, SLV_25, SLV_26),
};

#endif /* SAVELOAD_COMPAT_STATION_H */
