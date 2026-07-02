/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file endian_func.hpp Function to handling different endian machines. */

#ifndef ENDIAN_FUNC_HPP
#define ENDIAN_FUNC_HPP

#include "bitmath_func.hpp"

static constexpr uint16_t FROM_BE16(uint16_t x)
{
	if constexpr (std::endian::native == std::endian::big) return x;
	return std::byteswap(x);
}

static constexpr uint32_t FROM_BE32(uint32_t x)
{
	if constexpr (std::endian::native == std::endian::big) return x;
	return std::byteswap(x);
}

static constexpr uint16_t TO_BE16(uint16_t x)
{
	if constexpr (std::endian::native == std::endian::big) return x;
	return std::byteswap(x);
}

static constexpr uint32_t TO_BE32(uint32_t x)
{
	if constexpr (std::endian::native == std::endian::big) return x;
	return std::byteswap(x);
}

static constexpr uint16_t FROM_LE16(uint16_t x)
{
	if constexpr (std::endian::native == std::endian::little) return x;
	return std::byteswap(x);
}

static constexpr uint32_t FROM_LE32(uint32_t x)
{
	if constexpr (std::endian::native == std::endian::little) return x;
	return std::byteswap(x);
}

static constexpr uint16_t TO_LE16(uint16_t x)
{
	if constexpr (std::endian::native == std::endian::little) return x;
	return std::byteswap(x);
}

static constexpr uint32_t TO_LE32(uint32_t x)
{
	if constexpr (std::endian::native == std::endian::little) return x;
	return std::byteswap(x);
}

/**
 * Convert 4-length label identifier (+ null terminator) into an integer.
 * Matches behaviour of "multi-character characters", which are always the same order regardless of endianness.
 * @param label Fixed length character array to convert.
 * @return Identifier in 32-bit integer form.
 */
static constexpr uint32_t LabelToNum(const char (&label)[5])
{
	return label[0] << 24 | label[1] << 16 | label[2] << 8 | label[3];
}

#endif /* ENDIAN_FUNC_HPP */
