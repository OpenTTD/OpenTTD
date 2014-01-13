/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_sse2.cpp Implementation of the SSE2 32 bpp blitter. */

#ifdef WITH_SSE

#include "../stdafx.h"
#include "../zoom_func.h"
#include "../settings_type.h"
#include "32bpp_sse2.hpp"

/** Instantiation of the SSE2 32bpp blitter factory. */
static FBlitter_32bppSSE2 iFBlitter_32bppSSE2;

/**
 * Draws a sprite to a (screen) buffer. It is templated to allow faster operation.
 *
 * @tparam mode blitter mode
 * @param bp further blitting parameters
 * @param zoom zoom level at which we are drawing
 */
IGNORE_UNINITIALIZED_WARNING_START
template <BlitterMode mode, Blitter_32bppSSE2::ReadMode read_mode, Blitter_32bppSSE2::BlockType bt_last>
inline void Blitter_32bppSSE2::Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom)
{
	Colour *dst_line = (Colour *) bp->dst + bp->top * bp->pitch + bp->left;
	int effective_width = bp->width;

	/* Find where to start reading in the source sprite */
	const SpriteData * const sd = (const SpriteData *) bp->sprite;
	const SpriteInfo * const si = &sd->infos[zoom];
	const MapValue *src_mv_line = (const MapValue *) &sd->data[si->mv_offset] + bp->skip_top * si->sprite_width;
	const Colour *src_rgba_line = (const Colour *) ((const byte *) &sd->data[si->sprite_offset] + bp->skip_top * si->sprite_line_size);

	if (read_mode != RM_WITH_MARGIN) {
		src_rgba_line += bp->skip_left;
		src_mv_line += bp->skip_left;
	}

	/* Load these variables into register before loop. */
	const __m128i clear_hi    = CLEAR_HIGH_BYTE_MASK;
	const __m128i tr_nom_base = TRANSPARENT_NOM_BASE;

	for (int y = bp->height; y != 0; y--) {
		Colour *dst = dst_line;
		const Colour *src = src_rgba_line + META_LENGTH;
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
						for (uint x = (uint) effective_width / 2; x > 0; x--) {
							__m128i srcABCD = _mm_loadl_epi64((const __m128i*) src);
							__m128i dstABCD = _mm_loadl_epi64((__m128i*) dst);
							ALPHA_BLEND_2();
							_mm_storel_epi64((__m128i*) dst, srcABCD);
							src += 2;
							dst += 2;
						}
						if (bt_last == BT_ODD) {
							__m128i srcABCD = _mm_cvtsi32_si128(src->data);
							__m128i dstABCD = _mm_cvtsi32_si128(dst->data);
							ALPHA_BLEND_2();
							dst->data = _mm_cvtsi128_si32(srcABCD);
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
						const int new_width = effective_width - delta_diff;
						effective_width = delta_diff > 0 ? new_width : effective_width;
						if (effective_width <= 0) break;
						/* FALLTHROUGH */
					}

					case RM_WITH_SKIP: {
						const byte *remap = bp->remap;
						for (uint x = (uint) effective_width; x != 0; x--) {
							/* In case the m-channel is zero, do not remap this pixel in any way. */
							__m128i srcABCD;
							if (src_mv->m) {
								const uint r = remap[src_mv->m];
								if (r != 0) {
									Colour remapped_colour = AdjustBrightness(this->LookupColourInPalette(r), src_mv->v);
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
									ALPHA_BLEND_2();
								}
								dst->data = _mm_cvtsi128_si32(srcABCD);
							}
							src_mv++;
							dst++;
							src++;
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
				for (uint x = (uint) bp->width / 2; x > 0; x--) {
					__m128i srcABCD = _mm_loadl_epi64((const __m128i*) src);
					__m128i dstABCD = _mm_loadl_epi64((__m128i*) dst);
					__m128i srcAB = _mm_unpacklo_epi8(srcABCD, _mm_setzero_si128());
					__m128i dstAB = _mm_unpacklo_epi8(dstABCD, _mm_setzero_si128());
					__m128i alphaAB = _mm_shufflelo_epi16(srcAB, 0x3F);
					alphaAB = _mm_shufflehi_epi16(alphaAB, 0x3F);
					alphaAB = _mm_srli_epi16(alphaAB, 2); // Reduce to 64 levels of shades so the max value fits in 16 bits.
					__m128i nom = _mm_sub_epi16(tr_nom_base, alphaAB);
					dstAB = _mm_mullo_epi16(dstAB, nom);
					dstAB = _mm_srli_epi16(dstAB, 8);
					dstAB = _mm_packus_epi16(dstAB, dstAB);
					_mm_storel_epi64((__m128i *) dst, dstAB);
					src += 2;
					dst += 2;
				}
				if (bp->width & 1) {
					__m128i srcABCD = _mm_cvtsi32_si128(src->data);
					__m128i dstABCD = _mm_cvtsi32_si128(dst->data);
					__m128i srcAB = _mm_unpacklo_epi8(srcABCD, _mm_setzero_si128());
					__m128i dstAB = _mm_unpacklo_epi8(dstABCD, _mm_setzero_si128());
					__m128i alphaAB = _mm_shufflelo_epi16(srcAB, 0x3F);
					alphaAB = _mm_shufflehi_epi16(alphaAB, 0x3F);
					alphaAB = _mm_srli_epi16(alphaAB, 2);
					__m128i nom = _mm_sub_epi16(tr_nom_base, alphaAB);
					dstAB = _mm_mullo_epi16(dstAB, nom);
					dstAB = _mm_srli_epi16(dstAB, 8);
					dstAB = _mm_packus_epi16(dstAB, dstAB);
					dst->data = _mm_cvtsi128_si32(dstAB);
				}
				break;
			}
		}

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
void Blitter_32bppSSE2::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
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

Sprite *Blitter_32bppSSE_Base::Encode(const SpriteLoader::Sprite *sprite, AllocatorProc *allocator)
{
	/* First uint32 of a line = ~1 & the number of transparent pixels from the left.
	 * Second uint32 of a line = the number of transparent pixels from the right.
	 * Then all RGBA then all MV.
	 */
	ZoomLevel zoom_min = ZOOM_LVL_NORMAL;
	ZoomLevel zoom_max = ZOOM_LVL_NORMAL;
	if (sprite->type != ST_FONT) {
		zoom_min = _settings_client.gui.zoom_min;
		zoom_max = _settings_client.gui.zoom_max;
		if (zoom_max == zoom_min) zoom_max = ZOOM_LVL_MAX;
	}

	/* Calculate sizes and allocate. */
	SpriteData sd;
	uint all_sprites_size = 0;
	for (ZoomLevel z = zoom_min; z <= zoom_max; z++) {
		const SpriteLoader::Sprite *src_sprite = &sprite[z];
		sd.infos[z].sprite_width = src_sprite->width;
		sd.infos[z].sprite_offset = all_sprites_size;
		sd.infos[z].sprite_line_size = sizeof(Colour) * src_sprite->width + sizeof(uint32) * META_LENGTH;

		const uint rgba_size = sd.infos[z].sprite_line_size * src_sprite->height;
		sd.infos[z].mv_offset = all_sprites_size + rgba_size;

		const uint mv_size = sizeof(MapValue) * src_sprite->width * src_sprite->height;
		all_sprites_size += rgba_size + mv_size;
	}

	Sprite *dst_sprite = (Sprite *) allocator(sizeof(Sprite) + sizeof(SpriteData) + all_sprites_size);
	dst_sprite->height = sprite->height;
	dst_sprite->width  = sprite->width;
	dst_sprite->x_offs = sprite->x_offs;
	dst_sprite->y_offs = sprite->y_offs;
	memcpy(dst_sprite->data, &sd, sizeof(SpriteData));

	/* Copy colours. */
	for (ZoomLevel z = zoom_min; z <= zoom_max; z++) {
		const SpriteLoader::Sprite *src_sprite = &sprite[z];
		const SpriteLoader::CommonPixel *src = (const SpriteLoader::CommonPixel *) src_sprite->data;
		Colour *dst_rgba_line = (Colour *) &dst_sprite->data[sizeof(SpriteData) + sd.infos[z].sprite_offset];
		MapValue *dst_mv = (MapValue *) &dst_sprite->data[sizeof(SpriteData) + sd.infos[z].mv_offset];
		for (uint y = src_sprite->height; y != 0; y--) {
			Colour *dst_rgba = dst_rgba_line + META_LENGTH;
			for (uint x = src_sprite->width; x != 0; x--) {
				if (src->a != 0) {
					dst_rgba->a = src->a;
					dst_mv->m = src->m;
					if (src->m != 0) {
						/* Get brightest value (or default brightness if it's a black pixel). */
						const uint8 rgb_max = max(src->r, max(src->g, src->b));
						dst_mv->v = (rgb_max == 0) ? Blitter_32bppBase::DEFAULT_BRIGHTNESS : rgb_max;

						/* Pre-convert the mapping channel to a RGB value. */
						const Colour colour = AdjustBrightness(Blitter_32bppBase::LookupColourInPalette(src->m), dst_mv->v);
						dst_rgba->r = colour.r;
						dst_rgba->g = colour.g;
						dst_rgba->b = colour.b;
					} else {
						dst_rgba->r = src->r;
						dst_rgba->g = src->g;
						dst_rgba->b = src->b;
						dst_mv->v = Blitter_32bppBase::DEFAULT_BRIGHTNESS;
					}
				} else {
					dst_rgba->data = 0;
					*(uint16*) dst_mv = 0;
				}
				dst_rgba++;
				dst_mv++;
				src++;
			}

			/* Count the number of transparent pixels from the left. */
			dst_rgba = dst_rgba_line + META_LENGTH;
			uint32 nb_pix_transp = 0;
			for (uint x = src_sprite->width; x != 0; x--) {
				if (dst_rgba->a == 0) nb_pix_transp++;
				else break;
				dst_rgba++;
			}
			(*dst_rgba_line).data = nb_pix_transp & ~1; // "& ~1" to preserve the last block type

			Colour *nb_right = dst_rgba_line + 1;
			dst_rgba_line = (Colour*) ((byte*) dst_rgba_line + sd.infos[z].sprite_line_size);

			/* Count the number of transparent pixels from the right. */
			dst_rgba = dst_rgba_line - 1;
			nb_pix_transp = 0;
			for (uint x = src_sprite->width; x != 0; x--) {
				if (dst_rgba->a == 0) nb_pix_transp++;
				else break;
				dst_rgba--;
			}
			(*nb_right).data = nb_pix_transp; // no "& ~1" here, must be done when we know bp->width
		}
	}

	return dst_sprite;
}

/** ReallyAdjustBrightness() is not called that often.
 * Inlining this function implies a far jump, which has a huge latency.
 */
inline Colour Blitter_32bppSSE2::AdjustBrightness(Colour colour, uint8 brightness)
{
	/* Shortcut for normal brightness. */
	if (brightness == DEFAULT_BRIGHTNESS) return colour;

	return Blitter_32bppSSE2::ReallyAdjustBrightness(colour, brightness);
}

IGNORE_UNINITIALIZED_WARNING_START
/* static */ Colour Blitter_32bppSSE2::ReallyAdjustBrightness(Colour colour, uint8 brightness)
{
	uint64 c16 = colour.b | (uint64) colour.g << 16 | (uint64) colour.r << 32;
	c16 *= brightness;
	uint64 c16_ob = c16; // Helps out of order execution.
	c16 /= DEFAULT_BRIGHTNESS;
	c16 &= 0x01FF01FF01FF;

	/* Sum overbright (maximum for each rgb is 508, 9 bits, -255 is changed in -256 so we just have to take the 8 lower bits into account). */
	c16_ob = (((c16_ob >> (8 + 7)) & 0x0100010001) * 0xFF) & c16;
	uint64 ob = (uint16) c16_ob + (uint16) (c16_ob >> 16) + (uint16) (c16_ob >> 32);

	const uint32 alpha32 = colour.data & 0xFF000000;
	__m128i ret;
#ifdef _SQ64
	ret = _mm_cvtsi64_si128(c16);
#else
	INSR64(c16, ret, 0);
#endif
	if (ob != 0) {
		/* Reduce overbright strength. */
		ob /= 2;
		__m128i ob128;
#ifdef _SQ64
		ob128 = _mm_cvtsi64_si128(ob | ob << 16 | ob << 32);
#else
		INSR64(ob | ob << 16 | ob << 32, ob128, 0);
#endif
		__m128i white = OVERBRIGHT_VALUE_MASK;
		__m128i c128 = ret;
		ret = _mm_subs_epu16(white, c128); /* PSUBUSW,   (255 - rgb) */
		ret = _mm_mullo_epi16(ret, ob128); /* PMULLW, ob*(255 - rgb) */
		ret = _mm_srli_epi16(ret, 8);      /* PSRLW,  ob*(255 - rgb)/256 */
		ret = _mm_add_epi16(ret, c128);    /* PADDW,  ob*(255 - rgb)/256 + rgb */
	}

	ret = _mm_packus_epi16(ret, ret);      /* PACKUSWB, saturate and pack. */
	return alpha32 | _mm_cvtsi128_si32(ret);
}
IGNORE_UNINITIALIZED_WARNING_STOP

#endif /* WITH_SSE */
