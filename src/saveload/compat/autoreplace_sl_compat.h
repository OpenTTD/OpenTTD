/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file autoreplace_sl_compat.h Loading for autoreplace chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_AUTOREPLACE_H
#define SAVELOAD_COMPAT_AUTOREPLACE_H

#include "../saveload.h"

/** Original field order for _engine_renew_desc. */
const SaveLoadCompat _engine_renew_sl_compat[] = {
	SLC_VAR("from"),
	SLC_VAR("to"),
	SLC_VAR("next"),
	SLC_VAR("group_id"),
	SLC_VAR("replace_when_old"),
};

#endif /* SAVELOAD_COMPAT_AUTOREPLACE_H */
