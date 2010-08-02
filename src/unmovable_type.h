/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file unmovable_map.h Types related to unmovable tiles. */

#ifndef UNMOVABLE_TYPE_H
#define UNMOVABLE_TYPE_H

/** Types of unmovable structure */
enum UnmovableType {
	UNMOVABLE_TRANSMITTER = 0,    ///< The large antenna
	UNMOVABLE_LIGHTHOUSE  = 1,    ///< The nice lighthouse
	UNMOVABLE_STATUE      = 2,    ///< Statue in towns
	UNMOVABLE_OWNED_LAND  = 3,    ///< Owned land 'flag'
	UNMOVABLE_HQ          = 4,    ///< HeadQuarter of a player
	UNMOVABLE_MAX,
};

struct UnmovableSpec;

#endif /* UNMOVABLE_MAP_H */
