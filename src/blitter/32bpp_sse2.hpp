/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_sse2.hpp SSE2 32 bpp blitter. */

#ifndef BLITTER_32BPP_SSE2_HPP
#define BLITTER_32BPP_SSE2_HPP

#ifdef WITH_SSE

#include "32bpp_simple.hpp"
#include "emmintrin.h"

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

#define EXTR32(m_from, m_rank) (*(um128i*) &m_from).m128i_u32[m_rank]
#define EXTR64(m_from, m_rank) (*(um128i*) &m_from).m128i_u64[m_rank]
#define INSR32(m_val, m_into, m_rank) { \
	(*(um128i*) &m_into).m128i = _mm_insert_epi16((*(um128i*) &m_into).m128i, m_val, (m_rank)*2); \
	(*(um128i*) &m_into).m128i = _mm_insert_epi16((*(um128i*) &m_into).m128i, (m_val) >> 16, (m_rank)*2 + 1); \
}
#define INSR64(m_val, m_into, m_rank) (*(um128i*) &m_into).m128i_u64[m_rank] = (m_val)

/* PUT_ALPHA_IN_FRONT_OF_RGB is redefined in 32bpp_ssse3.hpp. */
#define PUT_ALPHA_IN_FRONT_OF_RGB(m_from, m_into) \
	m_into = _mm_shufflelo_epi16(m_from, 0x3F); /* PSHUFLW, put alpha1 in front of each rgb1 */ \
	m_into = _mm_shufflehi_epi16(m_into, 0x3F); /* PSHUFHW, put alpha2 in front of each rgb2 */

/* PACK_AB_WITHOUT_SATURATION is redefined in 32bpp_ssse3.hpp. */
#define PACK_AB_WITHOUT_SATURATION(m_from, m_into) \
	m_from = _mm_and_si128(m_from, clear_hi);  /* PAND, wipe high bytes to keep low bytes when packing */ \
	m_into = _mm_packus_epi16(m_from, m_from); /* PACKUSWB, pack 2 colours (with saturation) */

/* Alpha blend 2 pixels. */
#define ALPHA_BLEND_2() { \
	__m128i srcAB = _mm_unpacklo_epi8(srcABCD, _mm_setzero_si128()); /* PUNPCKLBW, expand each uint8 into uint16 */ \
	__m128i dstAB = _mm_unpacklo_epi8(dstABCD, _mm_setzero_si128()); \
	\
	__m128i alphaAB = _mm_cmpgt_epi16(srcAB, _mm_setzero_si128());   /* PCMPGTW, if (alpha > 0) a++; */ \
	alphaAB = _mm_srli_epi16(alphaAB, 15); \
	alphaAB = _mm_add_epi16(alphaAB, srcAB); \
	PUT_ALPHA_IN_FRONT_OF_RGB(alphaAB, alphaAB); \
	\
	srcAB = _mm_sub_epi16(srcAB, dstAB);          /* PSUBW,    (r - Cr) */ \
	srcAB = _mm_mullo_epi16(srcAB, alphaAB);      /* PMULLW, a*(r - Cr) */ \
	srcAB = _mm_srli_epi16(srcAB, 8);             /* PSRLW,  a*(r - Cr)/256 */ \
	srcAB = _mm_add_epi16(srcAB, dstAB);          /* PADDW,  a*(r - Cr)/256 + Cr */ \
	PACK_AB_WITHOUT_SATURATION(srcAB, srcABCD); \
}

/** Base methods for 32bpp SSE blitters. */
class Blitter_32bppSSE_Base {
public:
	virtual ~Blitter_32bppSSE_Base() {}

	struct MapValue {
		uint8 m;
		uint8 v;
	};
	assert_compile(sizeof(MapValue) == 2);

	/** Helper for creating specialised functions for specific optimisations. */
	enum ReadMode {
		RM_WITH_SKIP,   ///< Use normal code for skipping empty pixels.
		RM_WITH_MARGIN, ///< Use cached number of empty pixels at begin and end of line to reduce work.
		RM_NONE,        ///< No specialisation.
	};

	/** Helper for creating specialised functions for the case where the sprite width is odd or even. */
	enum BlockType {
		BT_EVEN, ///< An even number of pixels in the width; no need for a special case for the last pixel.
		BT_ODD,  ///< An odd number of pixels in the width; special case for the last pixel.
		BT_NONE, ///< No specialisation for either case.
	};

	/** Data stored about a (single) sprite. */
	struct SpriteInfo {
		uint32 sprite_offset;    ///< The offset to the sprite data.
		uint32 mv_offset;        ///< The offset to the map value data.
		uint16 sprite_line_size; ///< The size of a single line (pitch).
		uint16 sprite_width;     ///< The width of the sprite.
	};
	struct SpriteData {
		SpriteInfo infos[ZOOM_LVL_COUNT];
		byte data[]; ///< Data, all zoomlevels.
	};

	Sprite *Encode(const SpriteLoader::Sprite *sprite, AllocatorProc *allocator);
	virtual Colour AdjustBrightness(Colour colour, uint8 brightness) = 0;
};

/** The SSE2 32 bpp blitter (without palette animation). */
class Blitter_32bppSSE2 : public Blitter_32bppSimple, public Blitter_32bppSSE_Base {
public:
	virtual Colour AdjustBrightness(Colour colour, uint8 brightness);
	static Colour ReallyAdjustBrightness(Colour colour, uint8 brightness);
	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
	template <BlitterMode mode, Blitter_32bppSSE_Base::ReadMode read_mode, Blitter_32bppSSE_Base::BlockType bt_last>
	void Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom);

	/* virtual */ Sprite *Encode(const SpriteLoader::Sprite *sprite, AllocatorProc *allocator) {
		return Blitter_32bppSSE_Base::Encode(sprite, allocator);
	}

	/* virtual */ const char *GetName() { return "32bpp-sse2"; }
};

/** Factory for the SSE2 32 bpp blitter (without palette animation). */
class FBlitter_32bppSSE2 : public BlitterFactory {
public:
	FBlitter_32bppSSE2() : BlitterFactory("32bpp-sse2", "32bpp SSE2 Blitter (no palette animation)", HasCPUIDFlag(1, 3, 26)) {}
	/* virtual */ Blitter *CreateInstance() { return new Blitter_32bppSSE2(); }
};

#endif /* WITH_SSE */
#endif /* BLITTER_32BPP_SSE2_HPP */
