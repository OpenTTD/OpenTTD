/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file signs_sl_compat.h Loading of signs chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_SIGNS_H
#define SAVELOAD_COMPAT_SIGNS_H

#include "../saveload.h"

/** Original field order for _sign_desc. */
const SaveLoadCompat _sign_sl_compat[] = {
	SLC_VAR("name"),
	SLC_VAR("x"),
	SLC_VAR("y"),
	SLC_VAR("owner"),
	SLC_VAR("z"),
};

#endif /* SAVELOAD_COMPAT_SIGNS_H */
