/* $Id$ */

/** @file macros.h */

#ifndef MACROS_H
#define MACROS_H

#include "core/bitmath_func.hpp"
#include "core/math_func.hpp"

#define GENERAL_SPRITE_COLOR(color) ((color) + PALETTE_RECOLOR_START)
#define PLAYER_SPRITE_COLOR(owner) (GENERAL_SPRITE_COLOR(_player_colors[owner]))

/**
 * Whether a sprite comes from the original graphics files or a new grf file
 * (either supplied by OpenTTD or supplied by the user).
 *
 * @param sprite The sprite to check
 * @return True if it is a new sprite, or false if it is original.
 */
#define IS_CUSTOM_SPRITE(sprite) ((sprite) >= SPR_SIGNALS_BASE)


#define for_each_bit(_i, _b)            \
	for (_i = 0; _b != 0; _i++, _b >>= 1) \
		if (_b & 1)


static inline uint16 ReadLE16Aligned(const void* x)
{
	return FROM_LE16(*(const uint16*)x);
}

static inline uint16 ReadLE16Unaligned(const void* x)
{
#ifdef OTTD_ALIGNMENT
	return ((const byte*)x)[0] | ((const byte*)x)[1] << 8;
#else
	return FROM_LE16(*(const uint16*)x);
#endif
}


/** return the largest value that can be entered in a variable.
 */
#define MAX_UVALUE(type) ((type)~(type)0)

#endif /* MACROS_H */
