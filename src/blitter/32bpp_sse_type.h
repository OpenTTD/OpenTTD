/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_sse_type.h Types related to SSE 32 bpp blitter. */

#ifndef BLITTER_32BPP_SSE_TYPE_H
#define BLITTER_32BPP_SSE_TYPE_H

#ifdef WITH_SSE

#include "32bpp_simple.hpp"
#if (SSE_VERSION == 2)
#include <emmintrin.h>
#elif (SSE_VERSION == 3)
#include <tmmintrin.h>
#elif (SSE_VERSION == 4)
#include <smmintrin.h>
#endif

#define META_LENGTH 2 ///< Number of uint32_t inserted before each line of pixels in a sprite.
#define MARGIN_NORMAL_THRESHOLD (zoom == ZOOM_LVL_OUT_32X ? 8 : 4) ///< Minimum width to use margins with BM_NORMAL.
#define MARGIN_REMAP_THRESHOLD 4 ///< Minimum width to use margins with BM_COLOUR_REMAP.

#undef ALIGN

#ifdef _MSC_VER
	#define ALIGN(n) __declspec(align(n))
#else
	#define ALIGN(n) __attribute__ ((aligned (n)))
#endif

typedef union ALIGN(16) um128i {
	__m128i m128i;
	uint8_t m128i_u8[16];
	uint16_t m128i_u16[8];
	uint32_t m128i_u32[4];
	uint64_t m128i_u64[2];
} um128i;

#define CLEAR_HIGH_BYTE_MASK        _mm_setr_epi8(-1,  0, -1,  0, -1,  0, -1,  0, -1,  0, -1,  0, -1,  0, -1,  0)
#define ALPHA_CONTROL_MASK          _mm_setr_epi8( 6,  7,  6,  7,  6,  7, -1, -1, 14, 15, 14, 15, 14, 15, -1, -1)
#define PACK_LOW_CONTROL_MASK       _mm_setr_epi8( 0,  2,  4,  6,  8, 10, 12, 14, -1, -1, -1, -1, -1, -1, -1, -1)
#define PACK_HIGH_CONTROL_MASK      _mm_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1,  0,  2,  4, -1,  8, 10, 12, -1)
#define BRIGHTNESS_LOW_CONTROL_MASK _mm_setr_epi8( 1,  2,  1,  2,  1,  2,  0,  2,  3,  2,  3,  2,  3,  2,  0,  2)
#define BRIGHTNESS_DIV_CLEANER      _mm_setr_epi8(-1,  1, -1,  1, -1,  1, -1,  0, -1,  1, -1,  1, -1,  1, -1,  0)
#define OVERBRIGHT_PRESENCE_MASK    _mm_setr_epi8( 1,  0,  1,  0,  1,  0,  0,  0,  1,  0,  1,  0,  1,  0,  0,  0)
#define OVERBRIGHT_VALUE_MASK       _mm_setr_epi8(-1,  0, -1,  0, -1,  0,  0,  0, -1,  0, -1,  0, -1,  0,  0,  0)
#define OVERBRIGHT_CONTROL_MASK     _mm_setr_epi8( 0,  1,  0,  1,  0,  1,  7,  7,  2,  3,  2,  3,  2,  3,  7,  7)
#define TRANSPARENT_NOM_BASE        _mm_setr_epi16(256, 256, 256, 256, 256, 256, 256, 256)
#define ALPHA_AND_MASK              _mm_setr_epi16(  0,   0,   0,  -1,   0,   0,   0,  -1)

#endif /* WITH_SSE */
#endif /* BLITTER_32BPP_SSE_TYPE_H */
