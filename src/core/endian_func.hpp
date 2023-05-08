/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file endian_func.hpp Function to handling different endian machines. */

#ifndef ENDIAN_FUNC_HPP
#define ENDIAN_FUNC_HPP

#include "endian_type.hpp"
#include "bitmath_func.hpp"

/* Setup alignment and conversion macros */
#if TTD_ENDIAN == TTD_BIG_ENDIAN
#	define FROM_BE16(x) (x)
#	define FROM_BE32(x) (x)
#	define TO_BE16(x)   (x)
#	define TO_BE32(x)   (x)
#	define TO_BE32X(x)  (x)
#	define FROM_LE16(x) BSWAP16(x)
#	define FROM_LE32(x) BSWAP32(x)
#	define TO_LE16(x)   BSWAP16(x)
#	define TO_LE32(x)   BSWAP32(x)
#	define TO_LE32X(x)  BSWAP32(x)
#else
#	define FROM_BE16(x) BSWAP16(x)
#	define FROM_BE32(x) BSWAP32(x)
#	define TO_BE16(x)   BSWAP16(x)
#	define TO_BE32(x)   BSWAP32(x)
#	define TO_BE32X(x)  BSWAP32(x)
#	define FROM_LE16(x) (x)
#	define FROM_LE32(x) (x)
#	define TO_LE16(x)   (x)
#	define TO_LE32(x)   (x)
#	define TO_LE32X(x)  (x)
#endif /* TTD_ENDIAN == TTD_BIG_ENDIAN */

static inline uint16_t ReadLE16Aligned(const void *x)
{
	return FROM_LE16(*(const uint16_t*)x);
}

static inline uint16_t ReadLE16Unaligned(const void *x)
{
#if OTTD_ALIGNMENT == 1
	return ((const byte*)x)[0] | ((const byte*)x)[1] << 8;
#else
	return FROM_LE16(*(const uint16_t*)x);
#endif /* OTTD_ALIGNMENT == 1 */
}

#endif /* ENDIAN_FUNC_HPP */
