/* $Id$ */

/** @file endian_func.hpp Function to handling different endian machines. */

#ifndef ENDIAN_FUNC_H
#define ENDIAN_FUNC_H

#include "endian_type.hpp"
#include "bitmath_func.hpp"

/* Setup alignment and conversion macros */
#if TTD_ENDIAN == TTD_BIG_ENDIAN
	#define FROM_BE16(x) (x)
	#define FROM_BE32(x) (x)
	#define TO_BE16(x)   (x)
	#define TO_BE32(x)   (x)
	#define TO_BE32X(x)  (x)
	#define FROM_LE16(x) BSWAP16(x)
	#define FROM_LE32(x) BSWAP32(x)
	#define TO_LE16(x)   BSWAP16(x)
	#define TO_LE32(x)   BSWAP32(x)
	#define TO_LE32X(x)  BSWAP32(x)
#else
	#define FROM_BE16(x) BSWAP16(x)
	#define FROM_BE32(x) BSWAP32(x)
	#define TO_BE16(x)   BSWAP16(x)
	#define TO_BE32(x)   BSWAP32(x)
	#define TO_BE32X(x)  BSWAP32(x)
	#define FROM_LE16(x) (x)
	#define FROM_LE32(x) (x)
	#define TO_LE16(x)   (x)
	#define TO_LE32(x)   (x)
	#define TO_LE32X(x)  (x)
#endif /* TTD_ENDIAN == TTD_BIG_ENDIAN */

static inline uint16 ReadLE16Aligned(const void *x)
{
	return FROM_LE16(*(const uint16*)x);
}

static inline uint16 ReadLE16Unaligned(const void *x)
{
#if OTTD_ALIGNMENT == 1
	return ((const byte*)x)[0] | ((const byte*)x)[1] << 8;
#else
	return FROM_LE16(*(const uint16*)x);
#endif /* OTTD_ALIGNMENT == 1 */
}

#endif /* ENDIAN_FUNC_HPP */
