/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file subsidy_type.h basic types related to subsidies */

#ifndef SUBSIDY_TYPE_H
#define SUBSIDY_TYPE_H

#include "core/enum_type.hpp"

/** What part of a subsidy is something? */
enum PartOfSubsidy : byte {
	POS_NONE =     0, ///< nothing
	POS_SRC = 1 << 0, ///< bit 0 set -> town/industry is source of subsidised path
	POS_DST = 1 << 1, ///< bit 1 set -> town/industry is destination of subsidised path
};
DECLARE_ENUM_AS_BIT_SET(PartOfSubsidy)

typedef uint16_t SubsidyID; ///< ID of a subsidy
struct Subsidy;

#endif /* SUBSIDY_TYPE_H */
