/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_sse4_anim.cpp Implementation of the SSE4 32 bpp blitter with animation support. */

#ifdef WITH_SSE

#include "../stdafx.h"
#include "../video/video_driver.hpp"
#include "../table/sprites.h"
#include "32bpp_anim_sse4.hpp"

/** Instantiation of the SSE4 32bpp blitter factory. */
static FBlitter_32bppSSE4_Anim iFBlitter_32bppSSE4_Anim;

/**
 * Draws a sprite to a (screen) buffer. It is templated to allow faster operation.
 *
 * @tparam mode blitter mode
 * @param bp further blitting parameters
 * @param zoom zoom level at which we are drawing
 */
IGNORE_UNINITIALIZED_WARNING_START
template <BlitterMode mode, Blitter_32bppSSE2::ReadMode read_mode, Blitter_32bppSSE2::BlockType bt_last>
inline void Blitter_32bppSSE4_Anim::Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom)
{
	Colour *dst_line = (Colour *) bp->dst + bp->top * bp->pitch + bp->left;
	uint16 *anim_line = this->anim_buf + ((uint32 *)bp->dst - (uint32 *)_screen.dst_ptr) + bp->top * this->anim_buf_width + bp->left;
	int effective_width = bp->width;

	/* Find where to start reading in the source sprite. */
	const byte * const remap = bp->remap;
	const Blitter_32bppSSE_Base::SpriteData * const sd = (const Blitter_32bppSSE_Base::SpriteData *) bp->sprite;
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
		Colour *dst = dst_line;
		const Colour *src = src_rgba_line + META_LENGTH;
		const MapValue *src_mv = src_mv_line;
		uint16 *anim = anim_line;

		switch (mode) {
			default: {
				switch (read_mode) {
					case RM_WITH_MARGIN: {
						src += src_rgba_line[0].data;
						dst += src_rgba_line[0].data;
						src_mv += src_rgba_line[0].data;
						anim += src_rgba_line[0].data;
						const int width_diff = si->sprite_width - bp->width;
						effective_width = bp->width - (int) src_rgba_line[0].data;
						const int delta_diff = (int) src_rgba_line[1].data - width_diff;
						const int new_width = effective_width - (delta_diff & ~1);
						effective_width = delta_diff > 0 ? new_width : effective_width;
						if (effective_width <= 0) break;
						/* FALLTHROUGH */
					}

					case RM_WITH_SKIP: {
						uint32 mvX2 = *((uint32 *) const_cast<MapValue *>(src_mv));
						__m128i srcABCD = _mm_loadu_si128((const __m128i*) src);
						__m128i dstABCD = _mm_loadu_si128((__m128i*) dst);
						for (uint x = (uint) effective_width/2; x != 0; x--) {
							/* Remap colours. */
							const byte m0 = mvX2;
							if (m0 >= PALETTE_ANIM_START) {
								const Colour c0 = (this->LookupColourInPalette(m0).data & 0x00FFFFFF) | (src[0].data & 0xFF000000);
								INSR32(AdjustBrightness(c0, (byte) (mvX2 >> 8)).data, srcABCD, 0);
							}
							const byte m1 = mvX2 >> 16;
							if (m1 >= PALETTE_ANIM_START) {
								const Colour c1 = (this->LookupColourInPalette(m1).data & 0x00FFFFFF) | (src[1].data & 0xFF000000);
								INSR32(AdjustBrightness(c1, (byte) (mvX2 >> 24)).data, srcABCD, 1);
							}

							/* Update anim buffer. */
							const byte a0 = src[0].a;
							const byte a1 = src[1].a;
							uint32 anim01 = 0;
							if (a0 == 255) {
								if (a1 == 255) {
									*(uint32*) anim = mvX2;
									goto bmno_full_opacity;
								}
								anim01 = (uint16) mvX2;
							} else if (a0 == 0) {
								if (a1 == 0) {
									goto bmno_full_transparency;
								} else {
									if (a1 == 255) anim[1] = (uint16) (mvX2 >> 16);
									goto bmno_alpha_blend;
								}
							}
							if (a1 > 0) {
								if (a1 == 255) anim01 |= mvX2 & 0xFFFF0000;
								*(uint32*) anim = anim01;
							} else {
								anim[0] = (uint16) anim01;
							}

							/* Blend colours. */
bmno_alpha_blend:
							ALPHA_BLEND_2(pack_low_cm);
bmno_full_opacity:
							srcABCD = _mm_blend_epi16(srcABCD, dstABCD, 0xF0);

							src_mv += 2;
							mvX2 = *((uint32 *) const_cast<MapValue *>(src_mv));
							src += 2;
							anim += 2;
							dstABCD = _mm_loadu_si128((__m128i*) (dst+2));
							_mm_storeu_si128((__m128i *) dst, srcABCD);
							srcABCD = _mm_loadu_si128((const __m128i*) src);
							dst += 2;
							continue;

bmno_full_transparency:
							src_mv += 2;
							mvX2 = *((uint32 *) const_cast<MapValue *>(src_mv));
							dst += 2;
							src += 2;
							anim += 2;
							dstABCD = _mm_loadu_si128((__m128i*) dst);
							srcABCD = _mm_loadu_si128((const __m128i*) src);
						}

						if (bt_last == BT_ODD) {
							if (src->a == 0) {
							} else if (src->a == 255) {
								*anim = (uint16) mvX2;
								*dst = ((byte) mvX2 >= PALETTE_ANIM_START) ? AdjustBrightness(LookupColourInPalette((byte) mvX2), (byte) (mvX2 >> 8)) : *src;
							} else {
								*anim = 0;
								if ((byte) mvX2 >= PALETTE_ANIM_START) {
									ALIGN(16) Colour colour = AdjustBrightness(LookupColourInPalette((byte) mvX2), (byte) (mvX2 >> 8));
									colour.a = src->a;
									srcABCD = _mm_load_si128((__m128i*) &colour);
								}
								ALPHA_BLEND_2(pack_low_cm);
								(*dst).data = EXTR32(srcABCD, 0);
							}
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
						anim += src_rgba_line[0].data;
						const int width_diff = si->sprite_width - bp->width;
						effective_width = bp->width - (int) src_rgba_line[0].data;
						const int delta_diff = (int) src_rgba_line[1].data - width_diff;
						const int nd = effective_width - delta_diff;
						effective_width = delta_diff > 0 ? nd : effective_width;
						if (effective_width <= 0) break;
						/* FALLTHROUGH */
					}

					case RM_WITH_SKIP: {
						uint32 mvX2 = *((uint32 *) const_cast<MapValue *>(src_mv));
						__m128i srcABCD = _mm_loadu_si128((const __m128i*) src);
						__m128i dstABCD = _mm_loadu_si128((__m128i*) dst);

						for (uint x = (uint) effective_width / 2; x != 0; x--) {
							/* Remap colours. */
							const uint m0 = (byte) mvX2;
							const uint r0 = remap[m0];
							const uint m1 = (byte) (mvX2 >> 16);
							const uint r1 = remap[m1];
							if (mvX2 & 0x00FF00FF) {
								/* Written so the compiler uses CMOV. */
								const Colour src0 = src[0];
								const Colour c0map = (this->LookupColourInPalette(r0).data & 0x00FFFFFF) | (src0.data & 0xFF000000);
								Colour c0 = dst[0];
								c0 = r0 == 0 ? c0 : c0map;
								c0 = m0 != 0 ? c0 : src0;
								INSR32(c0.data, srcABCD, 0);

								const Colour src1 = src[1];
								const Colour c1map = (this->LookupColourInPalette(r1).data & 0x00FFFFFF) | (src1.data & 0xFF000000);
								Colour c1 = dst[1];
								c1 = r1 == 0 ? c1 : c1map;
								c1 = m1 != 0 ? c1 : src1;
								INSR32(c1.data, srcABCD, 1);

								if ((mvX2 & 0xFF00FF00) != 0x80008000) {
									ADJUST_BRIGHTNESS_2(srcABCD, mvX2);
								}
							}

							/* Update anim buffer. */
							const byte a0 = src[0].a;
							const byte a1 = src[1].a;
							uint32 anim01 = mvX2 & 0xFF00FF00;
							if (a0 == 255) {
								anim01 |= r0;
								if (a1 == 255) {
									*(uint32*) anim = anim01 | (r1 << 16);
									goto bmcr_full_opacity;
								}
							} else if (a0 == 0) {
								if (a1 == 0) {
									goto bmcr_full_transparency;
								} else {
									if (a1 == 255) {
										anim[1] = r1 | (anim01 >> 16);
									}
									goto bmcr_alpha_blend;
								}
							}
							if (a1 > 0) {
								if (a1 == 255) anim01 |= r1 << 16;
								*(uint32*) anim = anim01;
							} else {
								anim[0] = (uint16) anim01;
							}

							/* Blend colours. */
bmcr_alpha_blend:
							ALPHA_BLEND_2(pack_low_cm);
bmcr_full_opacity:
							srcABCD = _mm_blend_epi16(srcABCD, dstABCD, 0xF0);

							src += 2;
							src_mv += 2;
							anim += 2;
							mvX2 = *((uint32 *) const_cast<MapValue *>(src_mv));
							dstABCD = _mm_loadu_si128((__m128i*) (dst+2));
							_mm_storeu_si128((__m128i *) dst, srcABCD);
							srcABCD = _mm_loadu_si128((const __m128i*) src);
							dst += 2;
							continue;

bmcr_full_transparency:
							src_mv += 2;
							mvX2 = *((uint32 *) const_cast<MapValue *>(src_mv));
							dst += 2;
							src += 2;
							anim += 2;
							dstABCD = _mm_loadu_si128((__m128i*) dst);
							srcABCD = _mm_loadu_si128((const __m128i*) src);
						}

						if (effective_width & 1) {
							/* In case the m-channel is zero, do not remap this pixel in any way. */
							if (src->a == 0) {
							} else if ((byte) mvX2 != 0) {
								const uint r = remap[(byte) mvX2];
								*anim = (src->a == 255) ? (r | ((uint16) mvX2 & 0xFF00)) : 0;
								if (r != 0) {
									Colour remapped_colour = AdjustBrightness(LookupColourInPalette(r), (byte) (mvX2 >> 8));
									if (src->a == 255) {
										*dst = remapped_colour;
									} else {
										remapped_colour.a = src->a;
										INSR32(remapped_colour.data, srcABCD, 0);
										goto bmcr_alpha_blend_single;
									}
								}
							} else {
								*anim = 0;
								if (src->a == 255) {
									*dst = *src;
								} else {
bmcr_alpha_blend_single:
									ALPHA_BLEND_2(pack_low_cm);
									(*dst).data = EXTR32(srcABCD, 0);
								}
							}
						}
						break;
					}

					default: NOT_REACHED();
				}
				break;
			}

			case BM_TRANSPARENT: {
				/* Make the current colour a bit more black, so it looks like this image is transparent. */
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
					anim += 2;
					dstABCD = _mm_loadu_si128((__m128i*) dst);
					_mm_storeu_si128((__m128i *) old_dst, dstAB);
					srcABCD = _mm_loadu_si128((const __m128i*) src);
					if (src[-2].a) anim[-2] = 0;
					if (src[-1].a) anim[-1] = 0;
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
					if (src[0].a) anim[0] = 0;
				}
				break;
			}
		}

		src_mv_line += si->sprite_width;
		src_rgba_line = (const Colour*) ((const byte*) src_rgba_line + si->sprite_line_size);
		dst_line += bp->pitch;
		anim_line += this->anim_buf_width;
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
void Blitter_32bppSSE4_Anim::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
{
	switch (mode) {
		case BM_NORMAL: {
			const BlockType bt_last = (BlockType) (bp->width & 1);
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
inline Colour Blitter_32bppSSE4_Anim::AdjustBrightness(Colour colour, uint8 brightness)
{
	/* Shortcut for normal brightness. */
	if (brightness == DEFAULT_BRIGHTNESS) return colour;

	return Blitter_32bppSSE4::ReallyAdjustBrightness(colour, brightness);
}

#endif /* WITH_SSE */
