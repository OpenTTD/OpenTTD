/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_sse4.cpp Implementation of the SSE4 32 bpp blitter. */

#ifdef WITH_SSE

#include "../stdafx.h"
#include "../zoom_func.h"
#include "../settings_type.h"
#include "32bpp_sse4.hpp"

/** Instantiation of the SSE4 32bpp blitter factory. */
static FBlitter_32bppSSE4 iFBlitter_32bppSSE4;

#if defined(__GNUC__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
/**
 * Draws a sprite to a (screen) buffer. It is templated to allow faster operation.
 *
 * @tparam mode blitter mode
 * @param bp further blitting parameters
 * @param zoom zoom level at which we are drawing
 */
template <BlitterMode mode, Blitter_32bppSSE2::ReadMode read_mode, Blitter_32bppSSE2::BlockType bt_last>
inline void Blitter_32bppSSE4::Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom)
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

	/* Load these variables into register before loop. */
	const __m128i a_cm        = ALPHA_CONTROL_MASK;
	const __m128i pack_low_cm = PACK_LOW_CONTROL_MASK;
	const __m128i briAB_cm    = BRIGHTNESS_LOW_CONTROL_MASK;
	const __m128i div_cleaner = BRIGHTNESS_DIV_CLEANER;
	const __m128i ob_check    = OVERBRIGHT_PRESENCE_MASK;
	const __m128i ob_mask     = OVERBRIGHT_VALUE_MASK;
	const __m128i ob_cm       = OVERBRIGHT_CONTROL_MASK;
	const __m128i tr_nom_base = TRANSPARENT_NOM_BASE;

	for (int y = bp->height; y != 0; y--) {
		const Colour *src = src_rgba_line + META_LENGTH;
		Colour *dst = dst_line;
		const MapValue *src_mv = src_mv_line;

		switch (mode) {
			default: {
				switch (read_mode) {
					case RM_WITH_MARGIN: {
						src += src_rgba_line[0].data;
						dst += src_rgba_line[0].data;
						const int width_diff = si->sprite_width - bp->width;
						effective_width = bp->width - (int) src_rgba_line[0].data;
						const int delta_diff = (int) src_rgba_line[1].data - width_diff;
						const int new_width = effective_width - (delta_diff & ~1);
						effective_width = delta_diff > 0 ? new_width : effective_width;
						if (effective_width <= 0) break;
						/* FALLTHROUGH */
					}

					case RM_WITH_SKIP: {
						__m128i srcABCD = _mm_loadu_si128((const __m128i*) src);
						__m128i dstABCD = _mm_loadu_si128((__m128i*) dst);
						for (uint x = (uint) effective_width / 2; x > 0; x--) {
							ALPHA_BLEND_2(pack_low_cm);
							srcABCD = _mm_blend_epi16(srcABCD, dstABCD, 0xF0);
							Colour *old_dst = dst;
							src += 2;
							dst += 2;
							/* It is VERY important to read next data before it gets invalidated in cpu cache.
							 * And PEXTR latency is a real problem here.
							 */
							dstABCD = _mm_loadu_si128((__m128i*) dst);
							_mm_storeu_si128((__m128i *) old_dst, srcABCD);
							srcABCD = _mm_loadu_si128((const __m128i*) src);
						}
						if (bt_last == BT_ODD) {
							ALPHA_BLEND_2(pack_low_cm);
							*dst = (Colour) EXTR32(srcABCD, 0);
						}
						break;
					}

					default: NOT_REACHED();
				}
				break;
			}

			case BM_COLOUR_REMAP: {
				switch (read_mode) {
					case RM_WITH_MARGIN: {
						src += src_rgba_line[0].data;
						src_mv += src_rgba_line[0].data;
						dst += src_rgba_line[0].data;
						const int width_diff = si->sprite_width - bp->width;
						effective_width = bp->width - (int) src_rgba_line[0].data;
						const int delta_diff = (int) src_rgba_line[1].data - width_diff;
						const int nd = effective_width - delta_diff;
						effective_width = delta_diff > 0 ? nd : effective_width;
						if (effective_width <= 0) break;
						/* FALLTHROUGH */
					}

					case RM_WITH_SKIP: {
						__m128i srcABCD = _mm_loadu_si128((const __m128i*) src);
						__m128i dstABCD = _mm_loadu_si128((__m128i*) dst);
						uint32 mvX2 = *((uint32 *) const_cast<MapValue *>(src_mv));

						for (uint x = (uint) effective_width / 2; x > 0; x--) {
							/* Remap colours. */
							if (mvX2 & 0x00FF00FF) {
								/* Written so the compiler uses CMOV. */
								const Colour src0 = src[0];
								const uint m0 = (byte) mvX2;
								const uint r0 = remap[m0];
								const Colour c0map = (this->LookupColourInPalette(r0).data & 0x00FFFFFF) | (src0.data & 0xFF000000);
								Colour c0 = 0; // Use alpha of 0 to keep dst as is.
								c0 = r0 == 0 ? c0 : c0map;
								c0 = m0 != 0 ? c0 : src0;
								INSR32(c0.data, srcABCD, 0);

								const Colour src1 = src[1];
								const uint m1 = (byte) (mvX2 >> 16);
								const uint r1 = remap[m1];
								const Colour c1map = (this->LookupColourInPalette(r1).data & 0x00FFFFFF) | (src1.data & 0xFF000000);
								Colour c1 = 0;
								c1 = r1 == 0 ? c1 : c1map;
								c1 = m1 != 0 ? c1 : src1;
								INSR32(c1.data, srcABCD, 1);

								if ((mvX2 & 0xFF00FF00) != 0x80008000) {
									ADJUST_BRIGHTNESS_2(srcABCD, mvX2);
								}
							}

							/* Blend colours. */
							ALPHA_BLEND_2(pack_low_cm);
							srcABCD = _mm_blend_epi16(srcABCD, dstABCD, 0xF0);
							Colour *old_dst = dst;
							dst += 2;
							src += 2;
							src_mv += 2;
							dstABCD = _mm_loadu_si128((__m128i*) dst);
							_mm_storeu_si128((__m128i *) old_dst, srcABCD);
							mvX2 = *((uint32 *) const_cast<MapValue *>(src_mv));
							srcABCD = _mm_loadu_si128((const __m128i*) src);
						}

						if (effective_width & 1) {
							/* In case the m-channel is zero, do not remap this pixel in any way. */
							if ((byte) mvX2 == 0) {
								if (src->a < 255) {
									ALPHA_BLEND_2(pack_low_cm);
									(*dst).data = EXTR32(srcABCD, 0);
								} else
									*dst = *src;
							} else {
								const uint r = remap[(byte) mvX2];
								if (r != 0) {
									Colour remapped_colour = AdjustBrightness(this->LookupColourInPalette(r), (byte) (mvX2 >> 8));
									if (src->a == 255) {
										*dst = remapped_colour;
									} else {
										remapped_colour.a = src->a;
										INSR32(remapped_colour.data, srcABCD, 0);
										ALPHA_BLEND_2(pack_low_cm);
										(*dst).data = EXTR32(srcABCD, 0);
									}
								}
							}
						}
						break;
					}

					default: NOT_REACHED();
				}
				src_mv_line += si->sprite_width;
				break;
			}

			case BM_TRANSPARENT: {
				/* Make the current colour a bit more black, so it looks like this image is transparent.
				 * rgb = rgb * ((256/4) * 4 - (alpha/4)) / ((256/4) * 4)
				 */
				__m128i srcABCD = _mm_loadu_si128((const __m128i*) src);
				__m128i dstABCD = _mm_loadu_si128((__m128i*) dst);
				for (uint x = (uint) bp->width / 2; x > 0; x--) {
					__m128i srcAB = _mm_unpacklo_epi8(srcABCD, _mm_setzero_si128());
					__m128i dstAB = _mm_unpacklo_epi8(dstABCD, _mm_setzero_si128());
					__m128i dstCD = _mm_unpackhi_epi8(dstABCD, _mm_setzero_si128());
					__m128i alphaAB = _mm_shuffle_epi8(srcAB, a_cm);
					alphaAB = _mm_srli_epi16(alphaAB, 2); // Reduce to 64 levels of shades so the max value fits in 16 bits.
					__m128i nom = _mm_sub_epi16(tr_nom_base, alphaAB);
					dstAB = _mm_mullo_epi16(dstAB, nom);
					dstAB = _mm_srli_epi16(dstAB, 8);
					dstAB = _mm_packus_epi16(dstAB, dstCD);
					Colour *old_dst = dst;
					src += 2;
					dst += 2;
					dstABCD = _mm_loadu_si128((__m128i*) dst);
					_mm_storeu_si128((__m128i *) old_dst, dstAB);
					srcABCD = _mm_loadu_si128((const __m128i*) src);
				}
				if (bp->width & 1) {
					__m128i srcAB = _mm_unpacklo_epi8(srcABCD, _mm_setzero_si128());
					__m128i dstAB = _mm_unpacklo_epi8(dstABCD, _mm_setzero_si128());
					__m128i alphaAB = _mm_shuffle_epi8(srcAB, a_cm);
					alphaAB = _mm_srli_epi16(alphaAB, 2);
					__m128i nom = _mm_sub_epi16(tr_nom_base, alphaAB);
					dstAB = _mm_mullo_epi16(dstAB, nom);
					dstAB = _mm_srli_epi16(dstAB, 8);
					dstAB = _mm_packus_epi16(dstAB, dstAB);
					(*dst).data = EXTR32(dstAB, 0);
				}

				break;
			}
		}

		src_rgba_line = (const Colour*) ((const byte*) src_rgba_line + si->sprite_line_size);
		dst_line += bp->pitch;
	}
}
#if defined(__GNUC__)
	#pragma GCC diagnostic pop
#endif

/**
 * Draws a sprite to a (screen) buffer. Calls adequate templated function.
 *
 * @param bp further blitting parameters
 * @param mode blitter mode
 * @param zoom zoom level at which we are drawing
 */
void Blitter_32bppSSE4::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
{
	const BlockType bt_last = (BlockType) (bp->width & 1);
	switch (mode) {
		case BM_NORMAL: {
			if (bp->skip_left != 0 || bp->width <= MARGIN_NORMAL_THRESHOLD) {
				switch (bt_last) {
					case BT_EVEN: Draw<BM_NORMAL, RM_WITH_SKIP, BT_EVEN>(bp, zoom); return;
					case BT_ODD:  Draw<BM_NORMAL, RM_WITH_SKIP, BT_ODD>(bp, zoom); return;
					default: NOT_REACHED();
				}
			} else {
				switch (bt_last) {
					case BT_EVEN: Draw<BM_NORMAL, RM_WITH_MARGIN, BT_EVEN>(bp, zoom); return;
					case BT_ODD:  Draw<BM_NORMAL, RM_WITH_MARGIN, BT_ODD>(bp, zoom); return;
					default: NOT_REACHED();
				}
			}
			break;
		}
		case BM_COLOUR_REMAP:
			if (bp->skip_left != 0 || bp->width <= MARGIN_REMAP_THRESHOLD) {
				Draw<BM_COLOUR_REMAP, RM_WITH_SKIP, BT_NONE>(bp, zoom); return;
			} else {
				Draw<BM_COLOUR_REMAP, RM_WITH_MARGIN, BT_NONE>(bp, zoom); return;
			}
		case BM_TRANSPARENT:  Draw<BM_TRANSPARENT, RM_NONE, BT_NONE>(bp, zoom); return;
		default: NOT_REACHED();
	}
}

/** Same code as seen in 32bpp_sse2.cpp but some macros are not the same. */
inline Colour Blitter_32bppSSE4::AdjustBrightness(Colour colour, uint8 brightness)
{
	/* Shortcut for normal brightness. */
	if (brightness == DEFAULT_BRIGHTNESS) return colour;

	return this->ReallyAdjustBrightness(colour, brightness);
}

Colour Blitter_32bppSSE4::ReallyAdjustBrightness(Colour colour, uint8 brightness)
{
	ALIGN(16) uint64 c16 = colour.b | (uint64) colour.g << 16 | (uint64) colour.r << 32;
	c16 *= brightness;
	uint64 c16_ob = c16; // Helps out of order execution.
	c16 /= DEFAULT_BRIGHTNESS;
	c16 &= 0x01FF01FF01FF;

	/* Sum overbright (maximum for each rgb is 508, 9 bits, -255 is changed in -256 so we just have to take the 8 lower bits into account). */
	c16_ob = (((c16_ob >> (8 + 7)) & 0x0100010001) * 0xFF) & c16;
	uint64 ob = (uint16) c16_ob + (uint16) (c16_ob >> 16) + (uint16) (c16_ob >> 32);

	const uint32 alpha32 = colour.data & 0xFF000000;
	__m128i ret;
	INSR64(c16, ret, 0);
	if (ob != 0) {
		/* Reduce overbright strength. */
		ob /= 2;
		__m128i ob128;
		INSR64(ob | ob << 16 | ob << 32, ob128, 0);
		__m128i white = OVERBRIGHT_VALUE_MASK;
		__m128i c128 = ret;
		ret = _mm_subs_epu16(white, c128); /* PSUBUSW,   (255 - rgb) */
		ret = _mm_mullo_epi16(ret, ob128); /* PMULLW, ob*(255 - rgb) */
		ret = _mm_srli_epi16(ret, 8);      /* PSRLW,  ob*(255 - rgb)/256 */
		ret = _mm_add_epi16(ret, c128);    /* PADDW,  ob*(255 - rgb)/256 + rgb */
	}

	ret = _mm_packus_epi16(ret, ret);      /* PACKUSWB, saturate and pack. */
	return alpha32 | EXTR32(ret, 0);
}

#endif /* WITH_SSE */
