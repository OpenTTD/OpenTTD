/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehiclelist_cmd.h Functions and type for serializing vehicle lists. */

#ifndef VEHICLELIST_CMD_H
#define VEHICLELIST_CMD_H

#include "command_func.h"
#include "vehiclelist.h"

template <typename Tcont, typename Titer>
inline EndianBufferWriter<Tcont, Titer> &operator <<(EndianBufferWriter<Tcont, Titer> &buffer, const VehicleListIdentifier &vli)
{
	return buffer << vli.type << vli.vtype << vli.company << vli.index;
}

inline EndianBufferReader &operator >>(EndianBufferReader &buffer, VehicleListIdentifier &vli)
{
	return buffer >> vli.type >> vli.vtype >> vli.company >> vli.index;
}

#endif /* VEHICLELIST_CMD_H */
