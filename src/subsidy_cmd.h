/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file subsidy_cmd.h Command definitions related to subsidies. */

#ifndef SUBSIDY_CMD_H
#define SUBSIDY_CMD_H

#include "command_type.h"
#include "cargo_type.h"
#include "source_type.h"
#include "misc/endian_buffer.hpp"

CommandCost CmdCreateSubsidy(DoCommandFlags flags, CargoType cargo_type, Source src, Source dst);

DEF_CMD_TRAIT(CMD_CREATE_SUBSIDY, CmdCreateSubsidy, CommandFlag::Deity, CommandType::OtherManagement)


template <typename Tcont, typename Titer>
inline EndianBufferWriter<Tcont, Titer> &operator <<(EndianBufferWriter<Tcont, Titer> &buffer, const Source &source)
{
	return buffer << source.id << source.type;
}

inline EndianBufferReader &operator >>(EndianBufferReader &buffer, Source &source)
{
	return buffer >> source.id >> source.type;
}

#endif /* SUBSIDY_CMD_H */
