/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file livery_cmd.h Livery network command (de)serializer. */

#ifndef LIVERY_CMD_H
#define LIVERY_CMD_H

#include "livery.h"
#include "misc/endian_buffer.hpp"

/**
 * Write a Livery instance to an endian-aware buffer.
 * @tparam Tcont The buffer container type.
 * @tparam Titer The buffer iterator type.
 * @param buffer The endian-aware buffer.
 * @param livery The livery to write.
 * @return The endian-aware buffer.
 */
template <typename Tcont, typename Titer>
inline EndianBufferWriter<Tcont, Titer> &operator <<(EndianBufferWriter<Tcont, Titer> &buffer, const Livery &livery)
{
	return buffer << livery.in_use << livery.colour1 << livery.colour2;
}

/**
 * Read a Livery instance from an endian-aware buffer.
 * @param buffer The endian-aware buffer.
 * @param livery The livery to read into.
 * @return The endian-aware buffer.
 */
inline EndianBufferReader &operator >>(EndianBufferReader &buffer, Livery &livery)
{
	return buffer >> livery.in_use >> livery.colour1 >> livery.colour2;
}

#endif /* LIVERY_CMD_H */
