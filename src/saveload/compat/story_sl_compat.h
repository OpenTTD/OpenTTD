/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file story_sl_compat.h Loading for story chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_STORE_H
#define SAVELOAD_COMPAT_STORE_H

#include "../saveload.h"

/** Original field order for _story_page_elements_desc. */
const SaveLoadCompat _story_page_elements_sl_compat[] = {
	SLC_VAR("sort_value"),
	SLC_VAR("page"),
	SLC_VAR("type"),
	SLC_VAR("referenced_id"),
	SLC_VAR("text"),
};

/** Original field order for _story_pages_desc. */
const SaveLoadCompat _story_pages_sl_compat[] = {
	SLC_VAR("sort_value"),
	SLC_VAR("date"),
	SLC_VAR("company"),
	SLC_VAR("title"),
};

#endif /* SAVELOAD_COMPAT_STORE_H */
