/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_anim_sse4.cpp Implementation of the SSE4 32 bpp blitter with animation support. */

#include "palette_func.h"
#ifdef WITH_SSE

#include "../stdafx.h"
#include "../video/video_driver.hpp"
#include "../table/sprites.h"
#include "32bpp_anim_sse4.hpp"
#include "32bpp_sse_func.hpp"

#include "../safeguards.h"

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
template <BlitterMode mode, Blitter_32bppSSE2::ReadMode read_mode, Blitter_32bppSSE2::BlockType bt_last, bool translucent, bool animated>
GNU_TARGET("sse4.1")
inline void Blitter_32bppSSE4_Anim::Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom)
{
	const byte * const remap = bp->remap;
	Colour *dst_line = (Colour *) bp->dst + bp->top * bp->pitch + bp->left;
	uint16_t *anim_line = this->anim_buf + this->ScreenToAnimOffset((uint32_t *)bp->dst) + bp->top * this->anim_buf_pitch + bp->left;
	int effective_width = bp->width;

	/* Find where to start reading in the source sprite. */
	const Blitter_32bppSSE_Base::SpriteData * const sd = (const Blitter_32bppSSE_Base::SpriteData *) bp->sprite;
	const SpriteInfo * const si = &sd->infos[zoom];
	const MapValue *src_mv_line = (const MapValue *) &sd->data[si->mv_offset] + bp->skip_top * si->sprite_width;
	const Colour *src_rgba_line = (const Colour *) ((const byte *) &sd->data[si->sprite_offset] + bp->skip_top * si->sprite_line_size);

	if (read_mode != RM_WITH_MARGIN) {
		src_rgba_line += bp->skip_left;
		src_mv_line += bp->skip_left;
	}
	const MapValue *src_mv = src_mv_line;

	/* Load these variables into register before loop. */
	const __m128i a_cm        = ALPHA_CONTROL_MASK;
	const __m128i pack_low_cm = PACK_LOW_CONTROL_MASK;
	const __m128i tr_nom_base = TRANSPARENT_NOM_BASE;
	const __m128i a_am        = ALPHA_AND_MASK;

	for (int y = bp->height; y != 0; y--) {
		Colour *dst = dst_line;
		const Colour *src = src_rgba_line + META_LENGTH;
		if (mode != BM_TRANSPARENT) src_mv = src_mv_line;
		uint16_t *anim = anim_line;

		if (read_mode == RM_WITH_MARGIN) {
			assert(bt_last == BT_NONE); // or you must ensure block type is preserved
			anim += src_rgba_line[0].data;
			src += src_rgba_line[0].data;
			dst += src_rgba_line[0].data;
			if (mode != BM_TRANSPARENT) src_mv += src_rgba_line[0].data;
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
						if (src->a) {
							if (animated) {
								*anim = *(const uint16_t*) src_mv;
								*dst = (src_mv->m >= PALETTE_ANIM_START) ? AdjustBrightneSSE(this->LookupColourInPalette(src_mv->m), src_mv->v) : src->data;
							} else {
								*anim = 0;
								*dst = *src;
							}
						}
						if (animated) src_mv++;
						anim++;
						src++;
						dst++;
					}
					break;
				}

				for (uint x = (uint) effective_width/2; x != 0; x--) {
					uint32_t mvX2 = *((uint32_t *) const_cast<MapValue *>(src_mv));
					__m128i srcABCD = _mm_loadl_epi64((const __m128i*) src);
					__m128i dstABCD = _mm_loadl_epi64((__m128i*) dst);

					if (animated) {
						/* Remap colours. */
						const byte m0 = mvX2;
						if (m0 >= PALETTE_ANIM_START) {
							const Colour c0 = (this->LookupColourInPalette(m0).data & 0x00FFFFFF) | (src[0].data & 0xFF000000);
							InsertFirstUint32(AdjustBrightneSSE(c0, (byte) (mvX2 >> 8)).data, srcABCD);
						}
						const byte m1 = mvX2 >> 16;
						if (m1 >= PALETTE_ANIM_START) {
							const Colour c1 = (this->LookupColourInPalette(m1).data & 0x00FFFFFF) | (src[1].data & 0xFF000000);
							InsertSecondUint32(AdjustBrightneSSE(c1, (byte) (mvX2 >> 24)).data, srcABCD);
						}

						/* Update anim buffer. */
						const byte a0 = src[0].a;
						const byte a1 = src[1].a;
						uint32_t anim01 = 0;
						if (a0 == 255) {
							if (a1 == 255) {
								*(uint32_t*) anim = mvX2;
								goto bmno_full_opacity;
							}
							anim01 = (uint16_t) mvX2;
						} else if (a0 == 0) {
							if (a1 == 0) {
								goto bmno_full_transparency;
							} else {
								if (a1 == 255) anim[1] = (uint16_t) (mvX2 >> 16);
								goto bmno_alpha_blend;
							}
						}
						if (a1 > 0) {
							if (a1 == 255) anim01 |= mvX2 & 0xFFFF0000;
							*(uint32_t*) anim = anim01;
						} else {
							anim[0] = (uint16_t) anim01;
						}
					} else {
						if (src[0].a) anim[0] = 0;
						if (src[1].a) anim[1] = 0;
					}

					/* Blend colours. */
bmno_alpha_blend:
					srcABCD = AlphaBlendTwoPixels(srcABCD, dstABCD, a_cm, pack_low_cm, a_am);
bmno_full_opacity:
					_mm_storel_epi64((__m128i *) dst, srcABCD);
bmno_full_transparency:
					src_mv += 2;
					src += 2;
					anim += 2;
					dst += 2;
				}

				if ((bt_last == BT_NONE && effective_width & 1) || bt_last == BT_ODD) {
					if (src->a == 0) {
						/* Complete transparency. */
					} else if (src->a == 255) {
						*anim = *(const uint16_t*) src_mv;
						*dst = (src_mv->m >= PALETTE_ANIM_START) ? AdjustBrightneSSE(LookupColourInPalette(src_mv->m), src_mv->v) : *src;
					} else {
						*anim = 0;
						__m128i srcABCD;
						__m128i dstABCD = _mm_cvtsi32_si128(dst->data);
						if (src_mv->m >= PALETTE_ANIM_START) {
							Colour colour = AdjustBrightneSSE(LookupColourInPalette(src_mv->m), src_mv->v);
							colour.a = src->a;
							srcABCD = _mm_cvtsi32_si128(colour.data);
						} else {
							srcABCD = _mm_cvtsi32_si128(src->data);
						}
						dst->data = _mm_cvtsi128_si32(AlphaBlendTwoPixels(srcABCD, dstABCD, a_cm, pack_low_cm, a_am));
					}
				}
				break;

			case BM_COLOUR_REMAP:
				for (uint x = (uint) effective_width / 2; x != 0; x--) {
					uint32_t mvX2 = *((uint32_t *) const_cast<MapValue *>(src_mv));
					__m128i srcABCD = _mm_loadl_epi64((const __m128i*) src);
					__m128i dstABCD = _mm_loadl_epi64((__m128i*) dst);

					/* Remap colours. */
					const uint m0 = (byte) mvX2;
					const uint r0 = remap[m0];
					const uint m1 = (byte) (mvX2 >> 16);
					const uint r1 = remap[m1];
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
						uint64_t dsts;
						if (animated) dsts = _mm_cvtsi128_si64(dstABCD);
						uint64_t remapped_src = 0;
						CMOV_REMAP(c0, animated ? dsts : 0, srcs, mvX2);
						remapped_src = c0.data;
						CMOV_REMAP(c1, animated ? dsts >> 32 : 0, srcs >> 32, mvX2 >> 16);
						remapped_src |= (uint64_t) c1.data << 32;
						srcABCD = _mm_cvtsi64_si128(remapped_src);
#else
						Colour remapped_src[2];
						CMOV_REMAP(c0, animated ? _mm_cvtsi128_si32(dstABCD) : 0, _mm_cvtsi128_si32(srcABCD), mvX2);
						remapped_src[0] = c0.data;
						CMOV_REMAP(c1, animated ? dst[1] : 0, src[1], mvX2 >> 16);
						remapped_src[1] = c1.data;
						srcABCD = _mm_loadl_epi64((__m128i*) &remapped_src);
#endif

						if ((mvX2 & 0xFF00FF00) != 0x80008000) srcABCD = AdjustBrightnessOfTwoPixels(srcABCD, mvX2);
					}

					/* Update anim buffer. */
					if (animated) {
						const byte a0 = src[0].a;
						const byte a1 = src[1].a;
						uint32_t anim01 = mvX2 & 0xFF00FF00;
						if (a0 == 255) {
							anim01 |= r0;
							if (a1 == 255) {
								*(uint32_t*) anim = anim01 | (r1 << 16);
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
							*(uint32_t*) anim = anim01;
						} else {
							anim[0] = (uint16_t) anim01;
						}
					} else {
						if (src[0].a) anim[0] = 0;
						if (src[1].a) anim[1] = 0;
					}

					/* Blend colours. */
bmcr_alpha_blend:
					srcABCD = AlphaBlendTwoPixels(srcABCD, dstABCD, a_cm, pack_low_cm, a_am);
bmcr_full_opacity:
					_mm_storel_epi64((__m128i *) dst, srcABCD);
bmcr_full_transparency:
					src_mv += 2;
					dst += 2;
					src += 2;
					anim += 2;
				}

				if ((bt_last == BT_NONE && effective_width & 1) || bt_last == BT_ODD) {
					/* In case the m-channel is zero, do not remap this pixel in any way. */
					__m128i srcABCD;
					if (src->a == 0) break;
					if (src_mv->m) {
						const uint r = remap[src_mv->m];
						*anim = (animated && src->a == 255) ? r | ((uint16_t) src_mv->v << 8 ) : 0;
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
						*anim = 0;
						srcABCD = _mm_cvtsi32_si128(src->data);
						if (src->a < 255) {
bmcr_alpha_blend_single:
							__m128i dstABCD = _mm_cvtsi32_si128(dst->data);
							srcABCD = AlphaBlendTwoPixels(srcABCD, dstABCD, a_cm, pack_low_cm, a_am);
						}
						dst->data = _mm_cvtsi128_si32(srcABCD);
					}
				}
				break;

			case BM_TRANSPARENT:
				/* Make the current colour a bit more black, so it looks like this image is transparent. */
				for (uint x = (uint) bp->width / 2; x > 0; x--) {
					__m128i srcABCD = _mm_loadl_epi64((const __m128i*) src);
					__m128i dstABCD = _mm_loadl_epi64((__m128i*) dst);
					_mm_storel_epi64((__m128i *) dst, DarkenTwoPixels(srcABCD, dstABCD, a_cm, tr_nom_base));
					src += 2;
					dst += 2;
					anim += 2;
					if (src[-2].a) anim[-2] = 0;
					if (src[-1].a) anim[-1] = 0;
				}

				if ((bt_last == BT_NONE && bp->width & 1) || bt_last == BT_ODD) {
					__m128i srcABCD = _mm_cvtsi32_si128(src->data);
					__m128i dstABCD = _mm_cvtsi32_si128(dst->data);
					dst->data = _mm_cvtsi128_si32(DarkenTwoPixels(srcABCD, dstABCD, a_cm, tr_nom_base));
					if (src[0].a) anim[0] = 0;
				}
				break;

			case BM_TRANSPARENT_REMAP:
				/* Apply custom transparency remap. */
				for (uint x = (uint) bp->width; x > 0; x--) {
					if (src->a != 0) {
						*dst = this->LookupColourInPalette(remap[GetNearestColourIndex(*dst)]);
						*anim = 0;
					}
					src_mv++;
					dst++;
					src++;
					anim++;
				}
				break;


			case BM_CRASH_REMAP:
				for (uint x = (uint) bp->width; x > 0; x--) {
					if (src_mv->m == 0) {
						if (src->a != 0) {
							uint8_t g = MakeDark(src->r, src->g, src->b);
							*dst = ComposeColourRGBA(g, g, g, src->a, *dst);
							*anim = 0;
						}
					} else {
						uint r = remap[src_mv->m];
						if (r != 0) *dst = ComposeColourPANoCheck(this->AdjustBrightness(this->LookupColourInPalette(r), src_mv->v), src->a, *dst);
					}
					src_mv++;
					dst++;
					src++;
					anim++;
				}
				break;

			case BM_BLACK_REMAP:
				for (uint x = (uint) bp->width; x > 0; x--) {
					if (src->a != 0) {
						*dst = Colour(0, 0, 0);
						*anim = 0;
					}
					src_mv++;
					dst++;
					src++;
					anim++;
				}
				break;
		}

next_line:
		if (mode != BM_TRANSPARENT && mode != BM_TRANSPARENT_REMAP) src_mv_line += si->sprite_width;
		src_rgba_line = (const Colour*) ((const byte*) src_rgba_line + si->sprite_line_size);
		dst_line += bp->pitch;
		anim_line += this->anim_buf_pitch;
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
	if (_screen_disable_anim) {
		/* This means our output is not to the screen, so we can't be doing any animation stuff, so use our parent Draw() */
		Blitter_32bppSSE4::Draw(bp, mode, zoom);
		return;
	}

	const Blitter_32bppSSE_Base::SpriteFlags sprite_flags = ((const Blitter_32bppSSE_Base::SpriteData *) bp->sprite)->flags;
	switch (mode) {
		default: {
bm_normal:
			if (bp->skip_left != 0 || bp->width <= MARGIN_NORMAL_THRESHOLD) {
				const BlockType bt_last = (BlockType) (bp->width & 1);
				if (bt_last == BT_EVEN) {
					if (sprite_flags & SF_NO_ANIM) Draw<BM_NORMAL, RM_WITH_SKIP, BT_EVEN, true, false>(bp, zoom);
					else                           Draw<BM_NORMAL, RM_WITH_SKIP, BT_EVEN, true, true>(bp, zoom);
				} else {
					if (sprite_flags & SF_NO_ANIM) Draw<BM_NORMAL, RM_WITH_SKIP, BT_ODD, true, false>(bp, zoom);
					else                           Draw<BM_NORMAL, RM_WITH_SKIP, BT_ODD, true, true>(bp, zoom);
				}
			} else {
#ifdef POINTER_IS_64BIT
				if (sprite_flags & SF_TRANSLUCENT) {
					if (sprite_flags & SF_NO_ANIM) Draw<BM_NORMAL, RM_WITH_MARGIN, BT_NONE, true, false>(bp, zoom);
					else                           Draw<BM_NORMAL, RM_WITH_MARGIN, BT_NONE, true, true>(bp, zoom);
				} else {
					if (sprite_flags & SF_NO_ANIM) Draw<BM_NORMAL, RM_WITH_MARGIN, BT_NONE, false, false>(bp, zoom);
					else                           Draw<BM_NORMAL, RM_WITH_MARGIN, BT_NONE, false, true>(bp, zoom);
				}
#else
				if (sprite_flags & SF_NO_ANIM) Draw<BM_NORMAL, RM_WITH_MARGIN, BT_NONE, true, false>(bp, zoom);
				else                           Draw<BM_NORMAL, RM_WITH_MARGIN, BT_NONE, true, true>(bp, zoom);
#endif
			}
			break;
		}
		case BM_COLOUR_REMAP:
			if (sprite_flags & SF_NO_REMAP) goto bm_normal;
			if (bp->skip_left != 0 || bp->width <= MARGIN_REMAP_THRESHOLD) {
				if (sprite_flags & SF_NO_ANIM) Draw<BM_COLOUR_REMAP, RM_WITH_SKIP, BT_NONE, true, false>(bp, zoom);
				else                           Draw<BM_COLOUR_REMAP, RM_WITH_SKIP, BT_NONE, true, true>(bp, zoom);
			} else {
				if (sprite_flags & SF_NO_ANIM) Draw<BM_COLOUR_REMAP, RM_WITH_MARGIN, BT_NONE, true, false>(bp, zoom);
				else                           Draw<BM_COLOUR_REMAP, RM_WITH_MARGIN, BT_NONE, true, true>(bp, zoom);
			}
			break;
		case BM_TRANSPARENT:  Draw<BM_TRANSPARENT, RM_NONE, BT_NONE, true, true>(bp, zoom); return;
		case BM_TRANSPARENT_REMAP: Draw<BM_TRANSPARENT_REMAP, RM_NONE, BT_NONE, true, true>(bp, zoom); return;
		case BM_CRASH_REMAP:  Draw<BM_CRASH_REMAP, RM_NONE, BT_NONE, true, true>(bp, zoom); return;
		case BM_BLACK_REMAP:  Draw<BM_BLACK_REMAP, RM_NONE, BT_NONE, true, true>(bp, zoom); return;
	}
}

#endif /* WITH_SSE */
