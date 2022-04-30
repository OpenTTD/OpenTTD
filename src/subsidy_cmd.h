/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file subsidy_cmd.h Command definitions related to subsidies. */

#ifndef SUBSIDY_CMD_H
#define SUBSIDY_CMD_H

#include "command_type.h"
#include "cargo_type.h"

CommandCost CmdCreateSubsidy(DoCommandFlag flags, CargoID cid, SourceType src_type, SourceID src, SourceType dst_type, SourceID dst);

DEF_CMD_TRAIT(CMD_CREATE_SUBSIDY, CmdCreateSubsidy, CMD_DEITY, CMDT_OTHER_MANAGEMENT)

#endif /* SUBSIDY_CMD_H */
