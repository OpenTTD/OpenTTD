/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_sl_compat.h Loading for ai chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_AI_H
#define SAVELOAD_COMPAT_AI_H

#include "../saveload.h"

/** Original field order for _ai_company_desc. */
const SaveLoadCompat _ai_company_sl_compat[] = {
	SLC_VAR("name"),
	SLC_VAR("settings"),
	SLC_VAR("version"),
	SLC_VAR("is_random"),
};

#endif /* SAVELOAD_COMPAT_AI_H */
