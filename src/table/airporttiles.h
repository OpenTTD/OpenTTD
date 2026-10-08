/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file airporttiles.h Tables with airporttile defaults. */

#ifndef AIRPORTTILES_H
#define AIRPORTTILES_H

/**
 * All default airport tiles.
 * @see AirportTiles for a list of names.
 */
static const AirportTileSpec _origin_airporttile_specs[] = {
	/* 0..9 */
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},

	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},

	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},

	{},
	{{11, AnimationStatus::Looping, 2, {}}}, // APT_RADAR_GRASS_FENCE_SW
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{{3, AnimationStatus::Looping, 1, {}}}, // APT_GRASS_FENCE_NE_FLAG

	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},

	{},
	{{11, AnimationStatus::Looping, 2, {}}}, // APT_RADAR_FENCE_SW
	{{11, AnimationStatus::Looping, 2, {}}}, // APT_RADAR_FENCE_NE
	{},
	{},
	{},
	{},
	{},
	{},
	{},

	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},

	{},
	{},
	{},
	{{3, AnimationStatus::Looping, 1, {}}}, // APT_GRASS_FENCE_NE_FLAG_2
};

static_assert(NEW_AIRPORTTILE_OFFSET == lengthof(_origin_airporttile_specs));

#endif /* AIRPORTTILES_H */
