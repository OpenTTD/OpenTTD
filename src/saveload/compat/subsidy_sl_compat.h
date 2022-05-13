/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file subsidy_sl_compat.h Loading of subsidy chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_SUBSIDY_H
#define SAVELOAD_COMPAT_SUBSIDY_H

#include "../saveload.h"

/** Original field order for _subsidies_desc. */
const SaveLoadCompat _subsidies_sl_compat[] = {
	SLC_VAR("cargo_type"),
	SLC_VAR("remaining"),
	SLC_VAR("awarded"),
	SLC_VAR("src_type"),
	SLC_VAR("dst_type"),
	SLC_VAR("src"),
	SLC_VAR("dst"),
};

#endif /* SAVELOAD_COMPAT_SUBSIDY_H */
