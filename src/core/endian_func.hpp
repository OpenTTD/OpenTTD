/* $Id$ */

/** @file endian_func.hpp Function to handling different endian machines. */

#ifndef ENDIAN_FUNC_H
#define ENDIAN_FUNC_H

#include "bitmath_func.hpp"

#if defined(ARM) || defined(__arm__) || defined(__alpha__)
	#define OTTD_ALIGNMENT
#endif

/* Windows has always LITTLE_ENDIAN */
#if defined(WIN32) || defined(__OS2__) || defined(WIN64)
	#define TTD_LITTLE_ENDIAN
#elif !defined(TESTING)
	/* Else include endian[target/host].h, which has the endian-type, autodetected by the Makefile */
	#if defined(STRGEN)
		#include "endian_host.h"
	#else
		#include "endian_target.h"
	#endif
#endif /* WIN32 || __OS2__ || WIN64 */

/* Setup alignment and conversion macros */
#if defined(TTD_BIG_ENDIAN)
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
#endif /* TTD_BIG_ENDIAN */

static inline uint16 ReadLE16Aligned(const void *x)
{
	return FROM_LE16(*(const uint16*)x);
}

static inline uint16 ReadLE16Unaligned(const void *x)
{
#ifdef OTTD_ALIGNMENT
	return ((const byte*)x)[0] | ((const byte*)x)[1] << 8;
#else
	return FROM_LE16(*(const uint16*)x);
#endif
}

#endif /* ENDIAN_FUNC_HPP */
