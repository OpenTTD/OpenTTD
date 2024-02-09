/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game_sl_compat.h Loading for game chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_GAME_H
#define SAVELOAD_COMPAT_GAME_H

#include "../saveload.h"

/** Original field order for _game_script_desc. */
const SaveLoadCompat _game_script_sl_compat[] = {
	SLC_VAR("name"),
	SLC_VAR("settings"),
	SLC_VAR("version"),
	SLC_NULL(1, SL_MIN_VERSION, SLV_AI_LOCAL_CONFIG),
};

/** Original field order for SlGameLanguageString. */
const SaveLoadCompat _game_language_string_sl_compat[] = {
	SLC_VAR("string"),
};

/** Original field order for _game_language_desc. */
const SaveLoadCompat _game_language_sl_compat[] = {
	SLC_VAR("language"),
	SLC_VAR("count"),
	SLC_VAR("strings"),
};

#endif /* SAVELOAD_COMPAT_GAME_H */
