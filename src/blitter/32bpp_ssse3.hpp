/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_ssse3.hpp SSSE3 32 bpp blitter. */

#ifndef BLITTER_32BPP_SSSE3_HPP
#define BLITTER_32BPP_SSSE3_HPP

#ifdef WITH_SSE

#include "32bpp_sse2.hpp"
#include "tmmintrin.h"

/* Alpha blend 2 pixels. */
#undef ALPHA_BLEND_2
#define ALPHA_BLEND_2(m_pack_mask) { \
	__m128i srcAB = _mm_unpacklo_epi8(srcABCD, _mm_setzero_si128()); /* PUNPCKLBW, expand each uint8 into uint16 */ \
	__m128i dstAB = _mm_unpacklo_epi8(dstABCD, _mm_setzero_si128()); \
	\
	__m128i alphaAB = _mm_cmpgt_epi16(srcAB, _mm_setzero_si128());   /* PCMPGTW, if (alpha > 0) a++; */ \
	alphaAB = _mm_srli_epi16(alphaAB, 15); \
	alphaAB = _mm_add_epi16(alphaAB, srcAB); \
	alphaAB = _mm_shuffle_epi8(alphaAB, a_cm);        /* PSHUFB, put alpha in front of each rgb */ \
	\
	srcAB = _mm_sub_epi16(srcAB, dstAB);              /* PSUBW,    (r - Cr) */ \
	srcAB = _mm_mullo_epi16(srcAB, alphaAB);          /* PMULLW, a*(r - Cr) */ \
	srcAB = _mm_srli_epi16(srcAB, 8);                 /* PSRLW,  a*(r - Cr)/256 */ \
	srcAB = _mm_add_epi16(srcAB, dstAB);              /* PADDW,  a*(r - Cr)/256 + Cr */ \
	srcABCD = _mm_shuffle_epi8(srcAB, m_pack_mask);   /* PSHUFB, pack 2 Colour (without saturation) */ \
}

/* Adjust brightness of 2 pixels. */
#define ADJUST_BRIGHTNESS_2(colourX2, brightnessX2) \
	/* The following dataflow differs from the one of AdjustBrightness() only for alpha.
	 * In order to keep alpha in colAB, insert a 1 in a unused brightness byte (a*1->a).
	 * OK, not a 1 but DEFAULT_BRIGHTNESS to compensate the div.
	 */ \
	brightnessX2 &= 0xFF00FF00; \
	brightnessX2 += DEFAULT_BRIGHTNESS; \
	\
	__m128i zero = _mm_setzero_si128(); \
	__m128i colAB = _mm_unpacklo_epi8(colourX2, zero); \
	\
	__m128i briAB = _mm_cvtsi32_si128(brightnessX2); \
	briAB = _mm_shuffle_epi8(briAB, briAB_cm); /* DEFAULT_BRIGHTNESS in 0, 0x00 in 2. */ \
	colAB = _mm_mullo_epi16(colAB, briAB); \
	__m128i colAB_ob = _mm_srli_epi16(colAB, 8+7); \
	colAB = _mm_srli_epi16(colAB, 7); \
	\
	/* Sum overbright.
	 * Maximum for each rgb is 508 => 9 bits. The highest bit tells if there is overbright.
	 * -255 is changed in -256 so we just have to take the 8 lower bits into account.
	 */ \
	colAB = _mm_and_si128(colAB, div_cleaner); \
	colAB_ob = _mm_and_si128(colAB_ob, ob_check); \
	colAB_ob = _mm_mullo_epi16(colAB_ob, ob_mask); \
	colAB_ob = _mm_and_si128(colAB_ob, colAB); \
	__m128i obAB = _mm_hadd_epi16(_mm_hadd_epi16(colAB_ob, zero), zero); \
	\
	obAB = _mm_srli_epi16(obAB, 1);       /* Reduce overbright strength. */ \
	obAB = _mm_shuffle_epi8(obAB, ob_cm); \
	__m128i retAB = ob_mask;              /* ob_mask is equal to white. */ \
	retAB = _mm_subs_epu16(retAB, colAB); /*    (255 - rgb) */ \
	retAB = _mm_mullo_epi16(retAB, obAB); /* ob*(255 - rgb) */ \
	retAB = _mm_srli_epi16(retAB, 8);     /* ob*(255 - rgb)/256 */ \
	retAB = _mm_add_epi16(retAB, colAB);  /* ob*(255 - rgb)/256 + rgb */ \
	\
	colourX2 = _mm_packus_epi16(retAB, retAB);

/** The SSSE3 32 bpp blitter (without palette animation). */
class Blitter_32bppSSSE3 : public Blitter_32bppSSE2 {
public:
	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
	template <BlitterMode mode, Blitter_32bppSSE_Base::ReadMode read_mode, Blitter_32bppSSE_Base::BlockType bt_last>
	void Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom);
	/* virtual */ const char *GetName() { return "32bpp-ssse3"; }
};

/** Factory for the SSSE3 32 bpp blitter (without palette animation). */
class FBlitter_32bppSSSE3: public BlitterFactory {
public:
	FBlitter_32bppSSSE3() : BlitterFactory("32bpp-ssse3", "32bpp SSSE3 Blitter (no palette animation)", HasCPUIDFlag(1, 2, 9)) {}
	/* virtual */ Blitter *CreateInstance() { return new Blitter_32bppSSSE3(); }
};

#endif /* WITH_SSE */
#endif /* BLITTER_32BPP_SSSE3_HPP */
