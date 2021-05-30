/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file group_sl_compat.h Loading of group chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_GROUP_H
#define SAVELOAD_COMPAT_GROUP_H

#include "../saveload.h"

/** Original field order for _group_desc. */
const SaveLoadCompat _group_sl_compat[] = {
	SLC_VAR("name"),
	SLC_NULL(2, SL_MIN_VERSION, SLV_164),
	SLC_VAR("owner"),
	SLC_VAR("vehicle_type"),
	SLC_VAR("flags"),
	SLC_VAR("livery.in_use"),
	SLC_VAR("livery.colour1"),
	SLC_VAR("livery.colour2"),
	SLC_VAR("parent"),
};

#endif /* SAVELOAD_COMPAT_GROUP_H */
