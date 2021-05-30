/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file goal_sl_compat.h Loading of goal chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_GOAL_H
#define SAVELOAD_COMPAT_GOAL_H

#include "../saveload.h"

/** Original field order for _goals_desc. */
const SaveLoadCompat _goals_sl_compat[] = {
	SLC_VAR("company"),
	SLC_VAR("type"),
	SLC_VAR("dst"),
	SLC_VAR("text"),
	SLC_VAR("progress"),
	SLC_VAR("completed"),
};

#endif /* SAVELOAD_COMPAT_GOAL_H */
