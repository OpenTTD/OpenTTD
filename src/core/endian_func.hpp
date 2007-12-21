/* $Id$ */

/** @file endian_func.hpp */

#ifndef ENDIAN_FUNC_H
#define ENDIAN_FUNC_H

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

#endif /* ENDIAN_FUNC_H */
