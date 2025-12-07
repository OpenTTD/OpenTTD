/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file subsidy_type.h basic types related to subsidies */

#ifndef SUBSIDY_TYPE_H
#define SUBSIDY_TYPE_H

#include "core/enum_type.hpp"
#include "core/pool_type.hpp"

/** What part of a subsidy is something? */
enum class PartOfSubsidy : uint8_t {
	Source, ///< town/industry is source of subsidised path
	Destination, ///< town/industry is destination of subsidised path
};

using PartsOfSubsidy = EnumBitSet<PartOfSubsidy, uint8_t>;

using SubsidyID = PoolID<uint16_t, struct SubsidyIDTag, 256, 0xFFFF>; ///< ID of a subsidy
struct Subsidy;

#endif /* SUBSIDY_TYPE_H */
