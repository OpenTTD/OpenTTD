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
#	define FROM_LE16(x) std::byteswap(x)
#	define FROM_LE32(x) std::byteswap(x)
#	define TO_LE16(x)   std::byteswap(x)
#	define TO_LE32(x)   std::byteswap(x)
#	define TO_LE32X(x)  std::byteswap(x)
#else
#	define FROM_BE16(x) std::byteswap(x)
#	define FROM_BE32(x) std::byteswap(x)
#	define TO_BE16(x)   std::byteswap(x)
#	define TO_BE32(x)   std::byteswap(x)
#	define TO_BE32X(x)  std::byteswap(x)
#	define FROM_LE16(x) (x)
#	define FROM_LE32(x) (x)
#	define TO_LE16(x)   (x)
#	define TO_LE32(x)   (x)
#	define TO_LE32X(x)  (x)
#endif /* TTD_ENDIAN == TTD_BIG_ENDIAN */

#endif /* ENDIAN_FUNC_HPP */
