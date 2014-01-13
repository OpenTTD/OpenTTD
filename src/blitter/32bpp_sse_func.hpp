/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_sse_base.hpp Functions related to SSE 32 bpp blitter. */

#ifndef BLITTER_32BPP_SSE_BASE_HPP
#define BLITTER_32BPP_SSE_BASE_HPP

#ifdef WITH_SSE

#include "32bpp_simple.hpp"
#if (SSE_VERSION == 2)
#include <emmintrin.h>
#elif (SSE_VERSION == 3)
#include <tmmintrin.h>
#elif (SSE_VERSION == 4)
#include <smmintrin.h>
#endif

#define META_LENGTH 2 ///< Number of uint32 inserted before each line of pixels in a sprite.
#define MARGIN_NORMAL_THRESHOLD (zoom == ZOOM_LVL_OUT_32X ? 8 : 4) ///< Minimum width to use margins with BM_NORMAL.
#define MARGIN_REMAP_THRESHOLD 4 ///< Minimum width to use margins with BM_COLOUR_REMAP.

#ifdef _MSC_VER
	#define ALIGN(n) __declspec(align(n))
#else
	#define ALIGN(n) __attribute__ ((aligned (n)))
#endif

typedef union ALIGN(16) um128i {
	__m128i m128i;
	uint8 m128i_u8[16];
	uint16 m128i_u16[8];
	uint32 m128i_u32[4];
	uint64 m128i_u64[2];
} um128i;

#define CLEAR_HIGH_BYTE_MASK        _mm_setr_epi8(-1,  0, -1,  0, -1,  0, -1,  0, -1,  0, -1,  0, -1,  0, -1,  0)
#define ALPHA_CONTROL_MASK          _mm_setr_epi8( 6,  7,  6,  7,  6,  7, -1, -1, 14, 15, 14, 15, 14, 15, -1, -1)
#define PACK_LOW_CONTROL_MASK       _mm_setr_epi8( 0,  2,  4, -1,  8, 10, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1)
#define PACK_HIGH_CONTROL_MASK      _mm_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1,  0,  2,  4, -1,  8, 10, 12, -1)
#define BRIGHTNESS_LOW_CONTROL_MASK _mm_setr_epi8( 1,  2,  1,  2,  1,  2,  0,  2,  3,  2,  3,  2,  3,  2,  0,  2)
#define BRIGHTNESS_DIV_CLEANER      _mm_setr_epi8(-1,  1, -1,  1, -1,  1, -1,  0, -1,  1, -1,  1, -1,  1, -1,  0)
#define OVERBRIGHT_PRESENCE_MASK    _mm_setr_epi8( 1,  0,  1,  0,  1,  0,  0,  0,  1,  0,  1,  0,  1,  0,  0,  0)
#define OVERBRIGHT_VALUE_MASK       _mm_setr_epi8(-1,  0, -1,  0, -1,  0,  0,  0, -1,  0, -1,  0, -1,  0,  0,  0)
#define OVERBRIGHT_CONTROL_MASK     _mm_setr_epi8( 0,  1,  0,  1,  0,  1,  7,  7,  2,  3,  2,  3,  2,  3,  7,  7)
#define TRANSPARENT_NOM_BASE        _mm_setr_epi16(256, 256, 256, 256, 256, 256, 256, 256)

static inline void InsertFirstUint32(const uint32 value, __m128i &into)
{
#if (SSE_VERSION >= 4)
	into = _mm_insert_epi32(into, value, 0);
#else
	NOT_REACHED();
#endif
}

static inline void InsertSecondUint32(const uint32 value, __m128i &into)
{
#if (SSE_VERSION >= 4)
	into = _mm_insert_epi32(into, value, 1);
#else
	into = _mm_insert_epi16(into, value, 2);
	into = _mm_insert_epi16(into, value >> 16, 3);
#endif
}

static inline void LoadUint64(const uint64 value, __m128i &into)
{
#ifdef _SQ64
	into = _mm_cvtsi64_si128(value);
#else
	#if (SSE_VERSION >= 4)
		into = _mm_cvtsi32_si128(value);
		InsertSecondUint32(value >> 32, into);
	#else
		(*(um128i*) &into).m128i_u64[0] = value;
	#endif
#endif
}

static inline __m128i PackUnsaturated(__m128i from, const __m128i &mask)
{
#if (SSE_VERSION == 2)
	from = _mm_and_si128(from, mask);    // PAND, wipe high bytes to keep low bytes when packing
	return _mm_packus_epi16(from, from); // PACKUSWB, pack 2 colours (with saturation)
#else
	return _mm_shuffle_epi8(from, mask);
#endif
}

static inline __m128i DistributeAlpha(const __m128i from, const __m128i &mask)
{
#if (SSE_VERSION == 2)
	__m128i alphaAB = _mm_shufflelo_epi16(from, 0x3F); // PSHUFLW, put alpha1 in front of each rgb1
	return _mm_shufflehi_epi16(alphaAB, 0x3F);         // PSHUFHW, put alpha2 in front of each rgb2
#else
	return _mm_shuffle_epi8(from, mask);
#endif
}

static inline __m128i AlphaBlendTwoPixels(__m128i src, __m128i dst, const __m128i &distribution_mask, const __m128i &pack_mask)
{
	__m128i srcAB = _mm_unpacklo_epi8(src, _mm_setzero_si128());   // PUNPCKLBW, expand each uint8 into uint16
	__m128i dstAB = _mm_unpacklo_epi8(dst, _mm_setzero_si128());

	__m128i alphaAB = _mm_cmpgt_epi16(srcAB, _mm_setzero_si128()); // PCMPGTW, if (alpha > 0) a++;
	alphaAB = _mm_srli_epi16(alphaAB, 15);
	alphaAB = _mm_add_epi16(alphaAB, srcAB);
	alphaAB = DistributeAlpha(alphaAB, distribution_mask);

	srcAB = _mm_sub_epi16(srcAB, dstAB);     // PSUBW,    (r - Cr)
	srcAB = _mm_mullo_epi16(srcAB, alphaAB); // PMULLW, a*(r - Cr)
	srcAB = _mm_srli_epi16(srcAB, 8);        // PSRLW,  a*(r - Cr)/256
	srcAB = _mm_add_epi16(srcAB, dstAB);     // PADDW,  a*(r - Cr)/256 + Cr
	return PackUnsaturated(srcAB, pack_mask);
}

/* Darken 2 pixels.
 * rgb = rgb * ((256/4) * 4 - (alpha/4)) / ((256/4) * 4)
 */
static inline __m128i DarkenTwoPixels(__m128i src, __m128i dst, const __m128i &distribution_mask, const __m128i &tr_nom_base)
{
	__m128i srcAB = _mm_unpacklo_epi8(src, _mm_setzero_si128());
	__m128i dstAB = _mm_unpacklo_epi8(dst, _mm_setzero_si128());
	__m128i alphaAB = DistributeAlpha(srcAB, distribution_mask);
	alphaAB = _mm_srli_epi16(alphaAB, 2); // Reduce to 64 levels of shades so the max value fits in 16 bits.
	__m128i nom = _mm_sub_epi16(tr_nom_base, alphaAB);
	dstAB = _mm_mullo_epi16(dstAB, nom);
	dstAB = _mm_srli_epi16(dstAB, 8);
	return _mm_packus_epi16(dstAB, dstAB);
}

IGNORE_UNINITIALIZED_WARNING_START
static Colour ReallyAdjustBrightness(Colour colour, uint8 brightness)
{
	uint64 c16 = colour.b | (uint64) colour.g << 16 | (uint64) colour.r << 32;
	c16 *= brightness;
	uint64 c16_ob = c16; // Helps out of order execution.
	c16 /= Blitter_32bppBase::DEFAULT_BRIGHTNESS;
	c16 &= 0x01FF01FF01FF;

	/* Sum overbright (maximum for each rgb is 508, 9 bits, -255 is changed in -256 so we just have to take the 8 lower bits into account). */
	c16_ob = (((c16_ob >> (8 + 7)) & 0x0100010001) * 0xFF) & c16;
	const uint ob = ((uint16) c16_ob + (uint16) (c16_ob >> 16) + (uint16) (c16_ob >> 32)) / 2;

	const uint32 alpha32 = colour.data & 0xFF000000;
	__m128i ret;
	LoadUint64(c16, ret);
	if (ob != 0) {
		__m128i ob128 = _mm_cvtsi32_si128(ob);
		ob128 = _mm_shufflelo_epi16(ob128, 0xC0);
		__m128i white = OVERBRIGHT_VALUE_MASK;
		__m128i c128 = ret;
		ret = _mm_subs_epu16(white, c128); // PSUBUSW,   (255 - rgb)
		ret = _mm_mullo_epi16(ret, ob128); // PMULLW, ob*(255 - rgb)
		ret = _mm_srli_epi16(ret, 8);      // PSRLW,  ob*(255 - rgb)/256
		ret = _mm_add_epi16(ret, c128);    // PADDW,  ob*(255 - rgb)/256 + rgb
	}

	ret = _mm_packus_epi16(ret, ret);      // PACKUSWB, saturate and pack.
	return alpha32 | _mm_cvtsi128_si32(ret);
}
IGNORE_UNINITIALIZED_WARNING_STOP

/** ReallyAdjustBrightness() is not called that often.
 * Inlining this function implies a far jump, which has a huge latency.
 */
static inline Colour AdjustBrightneSSE(Colour colour, uint8 brightness)
{
	/* Shortcut for normal brightness. */
	if (brightness == Blitter_32bppBase::DEFAULT_BRIGHTNESS) return colour;

	return ReallyAdjustBrightness(colour, brightness);
}

static inline __m128i AdjustBrightnessOfTwoPixels(__m128i from, uint32 brightness)
{
#if (SSE_VERSION < 3)
	NOT_REACHED();
#else
	/* The following dataflow differs from the one of AdjustBrightness() only for alpha.
	 * In order to keep alpha in colAB, insert a 1 in a unused brightness byte (a*1->a).
	 * OK, not a 1 but DEFAULT_BRIGHTNESS to compensate the div.
	 */
	brightness &= 0xFF00FF00;
	brightness += Blitter_32bppBase::DEFAULT_BRIGHTNESS;

	__m128i colAB = _mm_unpacklo_epi8(from, _mm_setzero_si128());
	__m128i briAB = _mm_cvtsi32_si128(brightness);
	briAB = _mm_shuffle_epi8(briAB, BRIGHTNESS_LOW_CONTROL_MASK); // DEFAULT_BRIGHTNESS in 0, 0x00 in 2.
	colAB = _mm_mullo_epi16(colAB, briAB);
	__m128i colAB_ob = _mm_srli_epi16(colAB, 8+7);
	colAB = _mm_srli_epi16(colAB, 7);

	/* Sum overbright.
	 * Maximum for each rgb is 508 => 9 bits. The highest bit tells if there is overbright.
	 * -255 is changed in -256 so we just have to take the 8 lower bits into account.
	 */
	colAB = _mm_and_si128(colAB, BRIGHTNESS_DIV_CLEANER);
	colAB_ob = _mm_and_si128(colAB_ob, OVERBRIGHT_PRESENCE_MASK);
	colAB_ob = _mm_mullo_epi16(colAB_ob, OVERBRIGHT_VALUE_MASK);
	colAB_ob = _mm_and_si128(colAB_ob, colAB);
	__m128i obAB = _mm_hadd_epi16(_mm_hadd_epi16(colAB_ob, _mm_setzero_si128()), _mm_setzero_si128());

	obAB = _mm_srli_epi16(obAB, 1);        // Reduce overbright strength.
	obAB = _mm_shuffle_epi8(obAB, OVERBRIGHT_CONTROL_MASK);
	__m128i retAB = OVERBRIGHT_VALUE_MASK; // ob_mask is equal to white.
	retAB = _mm_subs_epu16(retAB, colAB);  //    (255 - rgb)
	retAB = _mm_mullo_epi16(retAB, obAB);  // ob*(255 - rgb)
	retAB = _mm_srli_epi16(retAB, 8);      // ob*(255 - rgb)/256
	retAB = _mm_add_epi16(retAB, colAB);   // ob*(255 - rgb)/256 + rgb

	return _mm_packus_epi16(retAB, retAB);
#endif
}

#endif /* WITH_SSE */
#endif /* BLITTER_32BPP_SSE_BASE_HPP */
