/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargopacket_sl_compat.h Loading for cargopacket chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_CARGOPACKET_H
#define SAVELOAD_COMPAT_CARGOPACKET_H

#include "../saveload.h"

/** Original field order for _cargopacket_desc. */
const SaveLoadCompat _cargopacket_sl_compat[] = {
	SLC_VAR("source"),
	SLC_VAR("source_xy"),
	SLC_NULL(4, SL_MIN_VERSION, SLV_REMOVE_LOADED_AT_XY),
	SLC_VAR("count"),
	SLC_VAR("days_in_transit"),
	SLC_VAR("feeder_share"),
	SLC_VAR("source_type"),
	SLC_VAR("source_id"),
	SLC_NULL(1, SL_MIN_VERSION, SLV_121),
};

#endif /* SAVELOAD_COMPAT_CARGOPACKET_H */
