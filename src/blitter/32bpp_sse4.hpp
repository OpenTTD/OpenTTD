/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_sse4.hpp SSE4 32 bpp blitter. */

#ifndef BLITTER_32BPP_SSE4_HPP
#define BLITTER_32BPP_SSE4_HPP

#ifdef WITH_SSE

#include "32bpp_ssse3.hpp"
#include "smmintrin.h"

#undef EXTR32
#define EXTR32(m_from, m_rank) _mm_extract_epi32((*(um128i*) &m_from).m128i, m_rank)
#undef INSR32
#define INSR32(m_val, m_into, m_rank) (*(um128i*) &m_into).m128i = _mm_insert_epi32((*(um128i*) &m_into).m128i, m_val, m_rank)

IGNORE_UNINITIALIZED_WARNING_START
#ifdef _SQ64
	#undef INSR64
	#define INSR64(m_val, m_into, m_rank) (*(um128i*) &m_into).m128i = _mm_insert_epi64((*(um128i*) &m_into).m128i, m_val, m_rank)
#else
	typedef union { uint64 u64; struct _u32 { uint32 low, high; } u32; } u6432;
	#undef INSR64
	#define INSR64(m_val, m_into, m_rank) { \
		u6432 v; \
		v.u64 = m_val; \
		(*(um128i*) &m_into).m128i = _mm_insert_epi32((*(um128i*) &m_into).m128i, v.u32.low, (m_rank)*2); \
		(*(um128i*) &m_into).m128i = _mm_insert_epi32((*(um128i*) &m_into).m128i, v.u32.high, (m_rank)*2 + 1); \
	}

	#undef LOAD64
	#define LOAD64(m_val, m_into) \
		m_into = _mm_cvtsi32_si128(m_val); \
		INSR32((m_val) >> 32, m_into, 1);
#endif
IGNORE_UNINITIALIZED_WARNING_STOP

/** The SSE4 32 bpp blitter (without palette animation). */
class Blitter_32bppSSE4 : public Blitter_32bppSSSE3 {
public:
	Colour AdjustBrightness(Colour colour, uint8 brightness);
	static Colour ReallyAdjustBrightness(Colour colour, uint8 brightness);

	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
	template <BlitterMode mode, Blitter_32bppSSE_Base::ReadMode read_mode, Blitter_32bppSSE_Base::BlockType bt_last>
	void Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom);
	/* virtual */ const char *GetName() { return "32bpp-sse4"; }
};

/** Factory for the SSE4 32 bpp blitter (without palette animation). */
class FBlitter_32bppSSE4: public BlitterFactory {
public:
	FBlitter_32bppSSE4() : BlitterFactory("32bpp-sse4", "32bpp SSE4 Blitter (no palette animation)", HasCPUIDFlag(1, 2, 19)) {}
	/* virtual */ Blitter *CreateInstance() { return new Blitter_32bppSSE4(); }
};

#endif /* WITH_SSE */
#endif /* BLITTER_32BPP_SSE4_HPP */
