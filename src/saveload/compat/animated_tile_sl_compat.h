/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file animated_tile_sl_compat.h Loading for animated_tile chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_ANIMATED_TILE_H
#define SAVELOAD_COMPAT_ANIMATED_TILE_H

#include "../saveload.h"

/** Original field order for _animated_tile_desc. */
const SaveLoadCompat _animated_tile_sl_compat[] = {
	SLC_VAR("tiles"),
};

#endif /* SAVELOAD_COMPAT_ANIMATED_TILE_H */
