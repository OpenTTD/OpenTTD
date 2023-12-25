/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_sse_func.hpp Functions related to SSE 32 bpp blitter. */

#ifndef BLITTER_32BPP_SSE_FUNC_HPP
#define BLITTER_32BPP_SSE_FUNC_HPP

#ifdef WITH_SSE

GNU_TARGET(SSE_TARGET)
static inline void InsertFirstUint32(const uint32_t value, __m128i &into)
{
#if (SSE_VERSION >= 4)
	into = _mm_insert_epi32(into, value, 0);
#else
	into = _mm_insert_epi16(into, value, 0);
	into = _mm_insert_epi16(into, value >> 16, 1);
#endif
}

GNU_TARGET(SSE_TARGET)
static inline void InsertSecondUint32(const uint32_t value, __m128i &into)
{
#if (SSE_VERSION >= 4)
	into = _mm_insert_epi32(into, value, 1);
#else
	into = _mm_insert_epi16(into, value, 2);
	into = _mm_insert_epi16(into, value >> 16, 3);
#endif
}

GNU_TARGET(SSE_TARGET)
static inline void LoadUint64(const uint64_t value, __m128i &into)
{
#ifdef POINTER_IS_64BIT
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

GNU_TARGET(SSE_TARGET)
static inline __m128i PackUnsaturated(__m128i from, const __m128i &mask)
{
#if (SSE_VERSION == 2)
	from = _mm_and_si128(from, mask);    // PAND, wipe high bytes to keep low bytes when packing
	return _mm_packus_epi16(from, from); // PACKUSWB, pack 2 colours (with saturation)
#else
	return _mm_shuffle_epi8(from, mask);
#endif
}

GNU_TARGET(SSE_TARGET)
static inline __m128i DistributeAlpha(const __m128i from, const __m128i &mask)
{
#if (SSE_VERSION == 2)
	__m128i alphaAB = _mm_shufflelo_epi16(from, 0x3F); // PSHUFLW, put alpha1 in front of each rgb1
	alphaAB = _mm_shufflehi_epi16(alphaAB, 0x3F);      // PSHUFHW, put alpha2 in front of each rgb2
	return _mm_andnot_si128(mask, alphaAB);            // PANDN, set alpha fields to 0
#else
	return _mm_shuffle_epi8(from, mask);
#endif
}

GNU_TARGET(SSE_TARGET)
static inline __m128i AlphaBlendTwoPixels(__m128i src, __m128i dst, const __m128i &distribution_mask, const __m128i &pack_mask, const __m128i &alpha_mask)
{
	__m128i srcAB = _mm_unpacklo_epi8(src, _mm_setzero_si128());   // PUNPCKLBW, expand each uint8_t into uint16
	__m128i dstAB = _mm_unpacklo_epi8(dst, _mm_setzero_si128());

	__m128i alphaMaskAB = _mm_cmpgt_epi16(srcAB, _mm_setzero_si128()); // PCMPGTW (alpha > 0) ? 0xFFFF : 0
	__m128i alphaAB = _mm_sub_epi16(srcAB, alphaMaskAB);               // if (alpha > 0) a++;
	alphaAB = DistributeAlpha(alphaAB, distribution_mask);

	srcAB = _mm_sub_epi16(srcAB, dstAB);     // PSUBW,    (r - Cr)
	srcAB = _mm_mullo_epi16(srcAB, alphaAB); // PMULLW, a*(r - Cr)
	srcAB = _mm_srli_epi16(srcAB, 8);        // PSRLW,  a*(r - Cr)/256
	srcAB = _mm_add_epi16(srcAB, dstAB);     // PADDW,  a*(r - Cr)/256 + Cr

	alphaMaskAB = _mm_and_si128(alphaMaskAB, alpha_mask); // PAND, set non alpha fields to 0
	srcAB = _mm_or_si128(srcAB, alphaMaskAB);             // POR, set alpha fields to 0xFFFF is src alpha was > 0

	return PackUnsaturated(srcAB, pack_mask);
}

/* Darken 2 pixels.
 * rgb = rgb * ((256/4) * 4 - (alpha/4)) / ((256/4) * 4)
 */
GNU_TARGET(SSE_TARGET)
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
GNU_TARGET(SSE_TARGET)
static Colour ReallyAdjustBrightness(Colour colour, uint8_t brightness)
{
	uint64_t c16 = colour.b | (uint64_t) colour.g << 16 | (uint64_t) colour.r << 32;
	c16 *= brightness;
	uint64_t c16_ob = c16; // Helps out of order execution.
	c16 /= Blitter_32bppBase::DEFAULT_BRIGHTNESS;
	c16 &= 0x01FF01FF01FFULL;

	/* Sum overbright (maximum for each rgb is 508, 9 bits, -255 is changed in -256 so we just have to take the 8 lower bits into account). */
	c16_ob = (((c16_ob >> (8 + 7)) & 0x0100010001ULL) * 0xFF) & c16;
	const uint ob = ((uint16_t) c16_ob + (uint16_t) (c16_ob >> 16) + (uint16_t) (c16_ob >> 32)) / 2;

	const uint32_t alpha32 = colour.data & 0xFF000000;
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
static inline Colour AdjustBrightneSSE(Colour colour, uint8_t brightness)
{
	/* Shortcut for normal brightness. */
	if (brightness == Blitter_32bppBase::DEFAULT_BRIGHTNESS) return colour;

	return ReallyAdjustBrightness(colour, brightness);
}

GNU_TARGET(SSE_TARGET)
static inline __m128i AdjustBrightnessOfTwoPixels([[maybe_unused]] __m128i from, [[maybe_unused]] uint32_t brightness)
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
	__m128i colAB_ob = _mm_srli_epi16(colAB, 8 + 7);
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

#if FULL_ANIMATION == 0
/**
 * Draws a sprite to a (screen) buffer. It is templated to allow faster operation.
 *
 * @tparam mode blitter mode
 * @param bp further blitting parameters
 * @param zoom zoom level at which we are drawing
 */
IGNORE_UNINITIALIZED_WARNING_START
template <BlitterMode mode, Blitter_32bppSSE2::ReadMode read_mode, Blitter_32bppSSE2::BlockType bt_last, bool translucent>
GNU_TARGET(SSE_TARGET)
#if (SSE_VERSION == 2)
inline void Blitter_32bppSSE2::Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom)
#elif (SSE_VERSION == 3)
inline void Blitter_32bppSSSE3::Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom)
#elif (SSE_VERSION == 4)
inline void Blitter_32bppSSE4::Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom)
#endif
{
	const byte * const remap = bp->remap;
	Colour *dst_line = (Colour *) bp->dst + bp->top * bp->pitch + bp->left;
	int effective_width = bp->width;

	/* Find where to start reading in the source sprite. */
	const SpriteData * const sd = (const SpriteData *) bp->sprite;
	const SpriteInfo * const si = &sd->infos[zoom];
	const MapValue *src_mv_line = (const MapValue *) &sd->data[si->mv_offset] + bp->skip_top * si->sprite_width;
	const Colour *src_rgba_line = (const Colour *) ((const byte *) &sd->data[si->sprite_offset] + bp->skip_top * si->sprite_line_size);

	if (read_mode != RM_WITH_MARGIN) {
		src_rgba_line += bp->skip_left;
		src_mv_line += bp->skip_left;
	}
	const MapValue *src_mv = src_mv_line;

	/* Load these variables into register before loop. */
	const __m128i alpha_and   = ALPHA_AND_MASK;
	#define ALPHA_BLEND_PARAM_3 alpha_and
#if (SSE_VERSION == 2)
	const __m128i clear_hi    = CLEAR_HIGH_BYTE_MASK;
	#define ALPHA_BLEND_PARAM_1 alpha_and
	#define ALPHA_BLEND_PARAM_2 clear_hi
	#define DARKEN_PARAM_1      tr_nom_base
	#define DARKEN_PARAM_2      tr_nom_base
#else
	const __m128i a_cm        = ALPHA_CONTROL_MASK;
	const __m128i pack_low_cm = PACK_LOW_CONTROL_MASK;
	#define ALPHA_BLEND_PARAM_1 a_cm
	#define ALPHA_BLEND_PARAM_2 pack_low_cm
	#define DARKEN_PARAM_1      a_cm
	#define DARKEN_PARAM_2      tr_nom_base
#endif
	const __m128i tr_nom_base = TRANSPARENT_NOM_BASE;

	for (int y = bp->height; y != 0; y--) {
		Colour *dst = dst_line;
		const Colour *src = src_rgba_line + META_LENGTH;
		if (mode == BM_COLOUR_REMAP || mode == BM_CRASH_REMAP) src_mv = src_mv_line;

		if (read_mode == RM_WITH_MARGIN) {
			assert(bt_last == BT_NONE); // or you must ensure block type is preserved
			src += src_rgba_line[0].data;
			dst += src_rgba_line[0].data;
			if (mode == BM_COLOUR_REMAP || mode == BM_CRASH_REMAP) src_mv += src_rgba_line[0].data;
			const int width_diff = si->sprite_width - bp->width;
			effective_width = bp->width - (int) src_rgba_line[0].data;
			const int delta_diff = (int) src_rgba_line[1].data - width_diff;
			const int new_width = effective_width - delta_diff;
			effective_width = delta_diff > 0 ? new_width : effective_width;
			if (effective_width <= 0) goto next_line;
		}

		switch (mode) {
			default:
				if (!translucent) {
					for (uint x = (uint) effective_width; x > 0; x--) {
						if (src->a) *dst = *src;
						src++;
						dst++;
					}
					break;
				}

				for (uint x = (uint) effective_width / 2; x > 0; x--) {
					__m128i srcABCD = _mm_loadl_epi64((const __m128i*) src);
					__m128i dstABCD = _mm_loadl_epi64((__m128i*) dst);
					_mm_storel_epi64((__m128i*) dst, AlphaBlendTwoPixels(srcABCD, dstABCD, ALPHA_BLEND_PARAM_1, ALPHA_BLEND_PARAM_2, ALPHA_BLEND_PARAM_3));
					src += 2;
					dst += 2;
				}

				if ((bt_last == BT_NONE && effective_width & 1) || bt_last == BT_ODD) {
					__m128i srcABCD = _mm_cvtsi32_si128(src->data);
					__m128i dstABCD = _mm_cvtsi32_si128(dst->data);
					dst->data = _mm_cvtsi128_si32(AlphaBlendTwoPixels(srcABCD, dstABCD, ALPHA_BLEND_PARAM_1, ALPHA_BLEND_PARAM_2, ALPHA_BLEND_PARAM_3));
				}
				break;

			case BM_COLOUR_REMAP:
#if (SSE_VERSION >= 3)
				for (uint x = (uint) effective_width / 2; x > 0; x--) {
					__m128i srcABCD = _mm_loadl_epi64((const __m128i*) src);
					__m128i dstABCD = _mm_loadl_epi64((__m128i*) dst);
					uint32_t mvX2 = *((uint32_t *) const_cast<MapValue *>(src_mv));

					/* Remap colours. */
					if (mvX2 & 0x00FF00FF) {
						#define CMOV_REMAP(m_colour, m_colour_init, m_src, m_m) \
							/* Written so the compiler uses CMOV. */ \
							Colour m_colour = m_colour_init; \
							{ \
							const Colour srcm = (Colour) (m_src); \
							const uint m = (byte) (m_m); \
							const uint r = remap[m]; \
							const Colour cmap = (this->LookupColourInPalette(r).data & 0x00FFFFFF) | (srcm.data & 0xFF000000); \
							m_colour = r == 0 ? m_colour : cmap; \
							m_colour = m != 0 ? m_colour : srcm; \
							}
#ifdef POINTER_IS_64BIT
						uint64_t srcs = _mm_cvtsi128_si64(srcABCD);
						uint64_t remapped_src = 0;
						CMOV_REMAP(c0, 0, srcs, mvX2);
						remapped_src = c0.data;
						CMOV_REMAP(c1, 0, srcs >> 32, mvX2 >> 16);
						remapped_src |= (uint64_t) c1.data << 32;
						srcABCD = _mm_cvtsi64_si128(remapped_src);
#else
						Colour remapped_src[2];
						CMOV_REMAP(c0, 0, _mm_cvtsi128_si32(srcABCD), mvX2);
						remapped_src[0] = c0.data;
						CMOV_REMAP(c1, 0, src[1], mvX2 >> 16);
						remapped_src[1] = c1.data;
						srcABCD = _mm_loadl_epi64((__m128i*) &remapped_src);
#endif

						if ((mvX2 & 0xFF00FF00) != 0x80008000) srcABCD = AdjustBrightnessOfTwoPixels(srcABCD, mvX2);
					}

					/* Blend colours. */
					_mm_storel_epi64((__m128i *) dst, AlphaBlendTwoPixels(srcABCD, dstABCD, ALPHA_BLEND_PARAM_1, ALPHA_BLEND_PARAM_2, ALPHA_BLEND_PARAM_3));
					dst += 2;
					src += 2;
					src_mv += 2;
				}

				if ((bt_last == BT_NONE && effective_width & 1) || bt_last == BT_ODD) {
#else
				for (uint x = (uint) effective_width; x > 0; x--) {
#endif
					/* In case the m-channel is zero, do not remap this pixel in any way. */
					__m128i srcABCD;
					if (src_mv->m) {
						const uint r = remap[src_mv->m];
						if (r != 0) {
							Colour remapped_colour = AdjustBrightneSSE(this->LookupColourInPalette(r), src_mv->v);
							if (src->a == 255) {
								*dst = remapped_colour;
							} else {
								remapped_colour.a = src->a;
								srcABCD = _mm_cvtsi32_si128(remapped_colour.data);
								goto bmcr_alpha_blend_single;
							}
						}
					} else {
						srcABCD = _mm_cvtsi32_si128(src->data);
						if (src->a < 255) {
bmcr_alpha_blend_single:
							__m128i dstABCD = _mm_cvtsi32_si128(dst->data);
							srcABCD = AlphaBlendTwoPixels(srcABCD, dstABCD, ALPHA_BLEND_PARAM_1, ALPHA_BLEND_PARAM_2, ALPHA_BLEND_PARAM_3);
						}
						dst->data = _mm_cvtsi128_si32(srcABCD);
					}
#if (SSE_VERSION == 2)
					src_mv++;
					dst++;
					src++;
#endif
				}
				break;

			case BM_TRANSPARENT:
				/* Make the current colour a bit more black, so it looks like this image is transparent. */
				for (uint x = (uint) bp->width / 2; x > 0; x--) {
					__m128i srcABCD = _mm_loadl_epi64((const __m128i*) src);
					__m128i dstABCD = _mm_loadl_epi64((__m128i*) dst);
					_mm_storel_epi64((__m128i *) dst, DarkenTwoPixels(srcABCD, dstABCD, DARKEN_PARAM_1, DARKEN_PARAM_2));
					src += 2;
					dst += 2;
				}

				if ((bt_last == BT_NONE && bp->width & 1) || bt_last == BT_ODD) {
					__m128i srcABCD = _mm_cvtsi32_si128(src->data);
					__m128i dstABCD = _mm_cvtsi32_si128(dst->data);
					dst->data = _mm_cvtsi128_si32(DarkenTwoPixels(srcABCD, dstABCD, DARKEN_PARAM_1, DARKEN_PARAM_2));
				}
				break;

			case BM_TRANSPARENT_REMAP:
				/* Apply custom transparency remap. */
				for (uint x = (uint) bp->width; x > 0; x--) {
					if (src->a != 0) {
						*dst = this->LookupColourInPalette(remap[GetNearestColourIndex(*dst)]);
					}
					src_mv++;
					dst++;
					src++;
				}
				break;

			case BM_CRASH_REMAP:
				for (uint x = (uint) bp->width; x > 0; x--) {
					if (src_mv->m == 0) {
						if (src->a != 0) {
							uint8_t g = MakeDark(src->r, src->g, src->b);
							*dst = ComposeColourRGBA(g, g, g, src->a, *dst);
						}
					} else {
						uint r = remap[src_mv->m];
						if (r != 0) *dst = ComposeColourPANoCheck(this->AdjustBrightness(this->LookupColourInPalette(r), src_mv->v), src->a, *dst);
					}
					src_mv++;
					dst++;
					src++;
				}
				break;

			case BM_BLACK_REMAP:
				for (uint x = (uint) bp->width; x > 0; x--) {
					if (src->a != 0) {
						*dst = Colour(0, 0, 0);
					}
					src_mv++;
					dst++;
					src++;
				}
				break;
		}

next_line:
		if (mode == BM_COLOUR_REMAP || mode == BM_CRASH_REMAP) src_mv_line += si->sprite_width;
		src_rgba_line = (const Colour*) ((const byte*) src_rgba_line + si->sprite_line_size);
		dst_line += bp->pitch;
	}
}
IGNORE_UNINITIALIZED_WARNING_STOP

/**
 * Draws a sprite to a (screen) buffer. Calls adequate templated function.
 *
 * @param bp further blitting parameters
 * @param mode blitter mode
 * @param zoom zoom level at which we are drawing
 */
#if (SSE_VERSION == 2)
void Blitter_32bppSSE2::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
#elif (SSE_VERSION == 3)
void Blitter_32bppSSSE3::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
#elif (SSE_VERSION == 4)
void Blitter_32bppSSE4::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
#endif
{
	switch (mode) {
		default: {
			if (bp->skip_left != 0 || bp->width <= MARGIN_NORMAL_THRESHOLD) {
bm_normal:
				const BlockType bt_last = (BlockType) (bp->width & 1);
				switch (bt_last) {
					default:     Draw<BM_NORMAL, RM_WITH_SKIP, BT_EVEN, true>(bp, zoom); return;
					case BT_ODD: Draw<BM_NORMAL, RM_WITH_SKIP, BT_ODD, true>(bp, zoom); return;
				}
			} else {
				if (((const Blitter_32bppSSE_Base::SpriteData *) bp->sprite)->flags & SF_TRANSLUCENT) {
					Draw<BM_NORMAL, RM_WITH_MARGIN, BT_NONE, true>(bp, zoom);
				} else {
					Draw<BM_NORMAL, RM_WITH_MARGIN, BT_NONE, false>(bp, zoom);
				}
				return;
			}
			break;
		}
		case BM_COLOUR_REMAP:
			if (((const Blitter_32bppSSE_Base::SpriteData *) bp->sprite)->flags & SF_NO_REMAP) goto bm_normal;
			if (bp->skip_left != 0 || bp->width <= MARGIN_REMAP_THRESHOLD) {
				Draw<BM_COLOUR_REMAP, RM_WITH_SKIP, BT_NONE, true>(bp, zoom); return;
			} else {
				Draw<BM_COLOUR_REMAP, RM_WITH_MARGIN, BT_NONE, true>(bp, zoom); return;
			}
		case BM_TRANSPARENT:  Draw<BM_TRANSPARENT, RM_NONE, BT_NONE, true>(bp, zoom); return;
		case BM_TRANSPARENT_REMAP: Draw<BM_TRANSPARENT_REMAP, RM_NONE, BT_NONE, true>(bp, zoom); return;
		case BM_CRASH_REMAP:  Draw<BM_CRASH_REMAP, RM_NONE, BT_NONE, true>(bp, zoom); return;
		case BM_BLACK_REMAP:  Draw<BM_BLACK_REMAP, RM_NONE, BT_NONE, true>(bp, zoom); return;
	}
}
#endif /* FULL_ANIMATION */

#endif /* WITH_SSE */
#endif /* BLITTER_32BPP_SSE_FUNC_HPP */
