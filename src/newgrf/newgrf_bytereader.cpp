/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_bytereader.cpp NewGRF byte buffer reader implementation. */

#include "../stdafx.h"
#include "../string_func.h"
#include "newgrf_bytereader.h"

#include "../safeguards.h"

/**
 * Read a value of the given number of bytes.
 * @returns Value read from buffer.
 */
uint32_t ByteReader::ReadVarSize(uint8_t size)
{
	switch (size) {
		case 1: return this->ReadByte();
		case 2: return this->ReadWord();
		case 4: return this->ReadDWord();
		default:
			NOT_REACHED();
			return 0;
	}
}
