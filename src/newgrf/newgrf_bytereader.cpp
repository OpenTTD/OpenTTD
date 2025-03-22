/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_bytereader.cpp NewGRF byte buffer reader implementation. */

#include "../stdafx.h"
#include "../core/backup_type.hpp"
#include "../string_func.h"
#include "newgrf_bytereader.h"

#include "../safeguards.h"

/**
 * Read a single DWord (32 bits).
 * @note The buffer is NOT advanced.
 * @returns Value read from buffer.
 */
uint32_t ByteReader::PeekDWord()
{
	AutoRestoreBackup backup(this->data, this->data);
	return this->ReadDWord();
}

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

/**
 * Read a string.
 * @returns Sting read from the buffer.
 */
std::string_view ByteReader::ReadString()
{
	const char *string = reinterpret_cast<const char *>(this->data);
	size_t string_length = ttd_strnlen(string, this->Remaining());

	/* Skip past the terminating NUL byte if it is present, but not more than remaining. */
	this->Skip(std::min(string_length + 1, this->Remaining()));

	return std::string_view(string, string_length);
}
