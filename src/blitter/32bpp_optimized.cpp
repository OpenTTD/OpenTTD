/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_optimized.cpp Implementation of the optimized 32 bpp blitter. */

#include "../stdafx.h"
#include "../zoom_func.h"
#include "../settings_type.h"
#include "../palette_func.h"
#include "32bpp_optimized.hpp"

#include "../safeguards.h"

/** Instantiation of the optimized 32bpp blitter factory. */
static FBlitter_32bppOptimized iFBlitter_32bppOptimized;

/**
 * Draws a sprite to a (screen) buffer. It is templated to allow faster operation.
 *
 * @tparam mode blitter mode
 * @param bp further blitting parameters
 * @param zoom zoom level at which we are drawing
 */
template <BlitterMode mode, bool Tpal_to_rgb>
inline void Blitter_32bppOptimized::Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom)
{
	const SpriteData *src = (const SpriteData *)bp->sprite;

	/* src_px : each line begins with uint32_t n = 'number of bytes in this line',
	 *          then n times is the Colour struct for this line */
	const Colour *src_px = (const Colour *)(src->data + src->offset[zoom][0]);
	/* src_n  : each line begins with uint32_t n = 'number of bytes in this line',
	 *          then interleaved stream of 'm' and 'n' channels. 'm' is remap,
	 *          'n' is number of bytes with the same alpha channel class */
	const uint16_t *src_n  = (const uint16_t *)(src->data + src->offset[zoom][1]);

	/* skip upper lines in src_px and src_n */
	for (uint i = bp->skip_top; i != 0; i--) {
		src_px = (const Colour *)((const byte *)src_px + *(const uint32_t *)src_px);
		src_n = (const uint16_t *)((const byte *)src_n + *(const uint32_t *)src_n);
	}

	/* skip lines in dst */
	Colour *dst = (Colour *)bp->dst + bp->top * bp->pitch + bp->left;

	/* store so we don't have to access it via bp every time (compiler assumes pointer aliasing) */
	const byte *remap = bp->remap;

	for (int y = 0; y < bp->height; y++) {
		/* next dst line begins here */
		Colour *dst_ln = dst + bp->pitch;

		/* next src line begins here */
		const Colour *src_px_ln = (const Colour *)((const byte *)src_px + *(const uint32_t *)src_px);
		src_px++;

		/* next src_n line begins here */
		const uint16_t *src_n_ln = (const uint16_t *)((const byte *)src_n + *(const uint32_t *)src_n);
		src_n += 2;

		/* we will end this line when we reach this point */
		Colour *dst_end = dst + bp->skip_left;

		/* number of pixels with the same alpha channel class */
		uint n;

		while (dst < dst_end) {
			n = *src_n++;

			if (src_px->a == 0) {
				dst += n;
				src_px ++;
				src_n++;
			} else {
				if (dst + n > dst_end) {
					uint d = dst_end - dst;
					src_px += d;
					src_n += d;

					dst = dst_end - bp->skip_left;
					dst_end = dst + bp->width;

					n = std::min(n - d, (uint)bp->width);
					goto draw;
				}
				dst += n;
				src_px += n;
				src_n += n;
			}
		}

		dst -= bp->skip_left;
		dst_end -= bp->skip_left;

		dst_end += bp->width;

		while (dst < dst_end) {
			n = std::min<uint>(*src_n++, dst_end - dst);

			if (src_px->a == 0) {
				dst += n;
				src_px++;
				src_n++;
				continue;
			}

			draw:;

			switch (mode) {
				case BM_COLOUR_REMAP:
					if (src_px->a == 255) {
						do {
							uint m = *src_n;
							/* In case the m-channel is zero, do not remap this pixel in any way */
							if (m == 0) {
								*dst = src_px->data;
							} else {
								uint r = remap[GB(m, 0, 8)];
								if (r != 0) *dst = this->AdjustBrightness(this->LookupColourInPalette(r), GB(m, 8, 8));
							}
							dst++;
							src_px++;
							src_n++;
						} while (--n != 0);
					} else {
						do {
							uint m = *src_n;
							if (m == 0) {
								*dst = ComposeColourRGBANoCheck(src_px->r, src_px->g, src_px->b, src_px->a, *dst);
							} else {
								uint r = remap[GB(m, 0, 8)];
								if (r != 0) *dst = ComposeColourPANoCheck(this->AdjustBrightness(this->LookupColourInPalette(r), GB(m, 8, 8)), src_px->a, *dst);
							}
							dst++;
							src_px++;
							src_n++;
						} while (--n != 0);
					}
					break;

				case BM_CRASH_REMAP:
					if (src_px->a == 255) {
						do {
							uint m = *src_n;
							if (m == 0) {
								uint8_t g = MakeDark(src_px->r, src_px->g, src_px->b);
								*dst = ComposeColourRGBA(g, g, g, src_px->a, *dst);
							} else {
								uint r = remap[GB(m, 0, 8)];
								if (r != 0) *dst = this->AdjustBrightness(this->LookupColourInPalette(r), GB(m, 8, 8));
							}
							dst++;
							src_px++;
							src_n++;
						} while (--n != 0);
					} else {
						do {
							uint m = *src_n;
							if (m == 0) {
								if (src_px->a != 0) {
									uint8_t g = MakeDark(src_px->r, src_px->g, src_px->b);
									*dst = ComposeColourRGBA(g, g, g, src_px->a, *dst);
								}
							} else {
								uint r = remap[GB(m, 0, 8)];
								if (r != 0) *dst = ComposeColourPANoCheck(this->AdjustBrightness(this->LookupColourInPalette(r), GB(m, 8, 8)), src_px->a, *dst);
							}
							dst++;
							src_px++;
							src_n++;
						} while (--n != 0);
					}
					break;

				case BM_BLACK_REMAP:
					do {
						*dst = Colour(0, 0, 0);
						dst++;
						src_px++;
						src_n++;
					} while (--n != 0);
					break;

				case BM_TRANSPARENT:
					/* Make the current colour a bit more black, so it looks like this image is transparent */
					src_n += n;
					if (src_px->a == 255) {
						src_px += n;
						do {
							*dst = MakeTransparent(*dst, 3, 4);
							dst++;
						} while (--n != 0);
					} else {
						do {
							*dst = MakeTransparent(*dst, (256 * 4 - src_px->a), 256 * 4);
							dst++;
							src_px++;
						} while (--n != 0);
					}
					break;

				case BM_TRANSPARENT_REMAP:
					/* Apply custom transparency remap. */
					src_n += n;
					if (src_px->a != 0) {
						src_px += n;
						do {
							*dst = this->LookupColourInPalette(remap[GetNearestColourIndex(*dst)]);
							dst++;
						} while (--n != 0);
					} else {
						dst += n;
						src_px += n;
					}
					break;

				default:
					if (src_px->a == 255) {
						/* faster than memcpy(), n is usually low */
						do {
							if (Tpal_to_rgb && *src_n != 0) {
								/* Convert the mapping channel to a RGB value */
								*dst = this->AdjustBrightness(this->LookupColourInPalette(GB(*src_n, 0, 8)), GB(*src_n, 8, 8)).data;
							} else {
								*dst = src_px->data;
							}
							dst++;
							src_px++;
							src_n++;
						} while (--n != 0);
					} else {
						do {
							if (Tpal_to_rgb && *src_n != 0) {
								/* Convert the mapping channel to a RGB value */
								Colour colour = this->AdjustBrightness(this->LookupColourInPalette(GB(*src_n, 0, 8)), GB(*src_n, 8, 8));
								*dst = ComposeColourRGBANoCheck(colour.r, colour.g, colour.b, src_px->a, *dst);
							} else {
								*dst = ComposeColourRGBANoCheck(src_px->r, src_px->g, src_px->b, src_px->a, *dst);
							}
							dst++;
							src_px++;
							src_n++;
						} while (--n != 0);
					}
					break;
			}
		}

		dst = dst_ln;
		src_px = src_px_ln;
		src_n  = src_n_ln;
	}
}

template <bool Tpal_to_rgb>
void Blitter_32bppOptimized::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
{
	switch (mode) {
		default: NOT_REACHED();
		case BM_NORMAL:       Draw<BM_NORMAL, Tpal_to_rgb>(bp, zoom); return;
		case BM_COLOUR_REMAP: Draw<BM_COLOUR_REMAP, Tpal_to_rgb>(bp, zoom); return;
		case BM_TRANSPARENT:  Draw<BM_TRANSPARENT, Tpal_to_rgb>(bp, zoom); return;
		case BM_TRANSPARENT_REMAP: Draw<BM_TRANSPARENT_REMAP, Tpal_to_rgb>(bp, zoom); return;
		case BM_CRASH_REMAP:  Draw<BM_CRASH_REMAP, Tpal_to_rgb>(bp, zoom); return;
		case BM_BLACK_REMAP:  Draw<BM_BLACK_REMAP, Tpal_to_rgb>(bp, zoom); return;
	}
}

template void Blitter_32bppOptimized::Draw<true>(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
template void Blitter_32bppOptimized::Draw<false>(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);

/**
 * Draws a sprite to a (screen) buffer. Calls adequate templated function.
 *
 * @param bp further blitting parameters
 * @param mode blitter mode
 * @param zoom zoom level at which we are drawing
 */
void Blitter_32bppOptimized::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
{
	this->Draw<false>(bp, mode, zoom);
}

template <bool Tpal_to_rgb> Sprite *Blitter_32bppOptimized::EncodeInternal(const SpriteLoader::SpriteCollection &sprite, AllocatorProc *allocator)
{
	/* streams of pixels (a, r, g, b channels)
	 *
	 * stored in separated stream so data are always aligned on 4B boundary */
	Colour *dst_px_orig[ZOOM_LVL_END];

	/* interleaved stream of 'm' channel and 'n' channel
	 * 'n' is number of following pixels with the same alpha channel class
	 * there are 3 classes: 0, 255, others
	 *
	 * it has to be stored in one stream so fewer registers are used -
	 * x86 has problems with register allocation even with this solution */
	uint16_t *dst_n_orig[ZOOM_LVL_END];

	/* lengths of streams */
	uint32_t lengths[ZOOM_LVL_END][2];

	ZoomLevel zoom_min;
	ZoomLevel zoom_max;

	if (sprite[ZOOM_LVL_NORMAL].type == SpriteType::Font) {
		zoom_min = ZOOM_LVL_NORMAL;
		zoom_max = ZOOM_LVL_NORMAL;
	} else {
		zoom_min = _settings_client.gui.zoom_min;
		zoom_max = _settings_client.gui.zoom_max;
		if (zoom_max == zoom_min) zoom_max = ZOOM_LVL_MAX;
	}

	for (ZoomLevel z = zoom_min; z <= zoom_max; z++) {
		const SpriteLoader::Sprite *src_orig = &sprite[z];

		uint size = src_orig->height * src_orig->width;

		dst_px_orig[z] = CallocT<Colour>(size + src_orig->height * 2);
		dst_n_orig[z]  = CallocT<uint16_t>(size * 2 + src_orig->height * 4 * 2);

		uint32_t *dst_px_ln = (uint32_t *)dst_px_orig[z];
		uint32_t *dst_n_ln  = (uint32_t *)dst_n_orig[z];

		const SpriteLoader::CommonPixel *src = (const SpriteLoader::CommonPixel *)src_orig->data;

		for (uint y = src_orig->height; y > 0; y--) {
			/* Index 0 of dst_px and dst_n is left as space to save the length of the row to be filled later. */
			Colour *dst_px = (Colour *)&dst_px_ln[1];
			uint16_t *dst_n = (uint16_t *)&dst_n_ln[1];

			uint16_t *dst_len = dst_n++;

			uint last = 3;
			int len = 0;

			for (uint x = src_orig->width; x > 0; x--) {
				uint8_t a = src->a;
				uint t = a > 0 && a < 255 ? 1 : a;

				if (last != t || len == 65535) {
					if (last != 3) {
						*dst_len = len;
						dst_len = dst_n++;
					}
					len = 0;
				}

				last = t;
				len++;

				if (a != 0) {
					dst_px->a = a;
					*dst_n = src->m;
					if (src->m != 0) {
						/* Get brightest value */
						uint8_t rgb_max = std::max({ src->r, src->g, src->b });

						/* Black pixel (8bpp or old 32bpp image), so use default value */
						if (rgb_max == 0) rgb_max = DEFAULT_BRIGHTNESS;
						*dst_n |= rgb_max << 8;

						if (Tpal_to_rgb) {
							/* Pre-convert the mapping channel to a RGB value */
							Colour colour = this->AdjustBrightness(this->LookupColourInPalette(src->m), rgb_max);
							dst_px->r = colour.r;
							dst_px->g = colour.g;
							dst_px->b = colour.b;
						} else {
							dst_px->r = src->r;
							dst_px->g = src->g;
							dst_px->b = src->b;
						}
					} else {
						dst_px->r = src->r;
						dst_px->g = src->g;
						dst_px->b = src->b;
					}
					dst_px++;
					dst_n++;
				} else if (len == 1) {
					dst_px++;
					*dst_n = src->m;
					dst_n++;
				}

				src++;
			}

			if (last != 3) {
				*dst_len = len;
			}

			dst_px = (Colour *)AlignPtr(dst_px, 4);
			dst_n  = (uint16_t *)AlignPtr(dst_n, 4);

			*dst_px_ln = (uint8_t *)dst_px - (uint8_t *)dst_px_ln;
			*dst_n_ln  = (uint8_t *)dst_n  - (uint8_t *)dst_n_ln;

			dst_px_ln = (uint32_t *)dst_px;
			dst_n_ln =  (uint32_t *)dst_n;
		}

		lengths[z][0] = (byte *)dst_px_ln - (byte *)dst_px_orig[z]; // all are aligned to 4B boundary
		lengths[z][1] = (byte *)dst_n_ln  - (byte *)dst_n_orig[z];
	}

	uint len = 0; // total length of data
	for (ZoomLevel z = zoom_min; z <= zoom_max; z++) {
		len += lengths[z][0] + lengths[z][1];
	}

	Sprite *dest_sprite = (Sprite *)allocator(sizeof(*dest_sprite) + sizeof(SpriteData) + len);

	dest_sprite->height = sprite[ZOOM_LVL_NORMAL].height;
	dest_sprite->width  = sprite[ZOOM_LVL_NORMAL].width;
	dest_sprite->x_offs = sprite[ZOOM_LVL_NORMAL].x_offs;
	dest_sprite->y_offs = sprite[ZOOM_LVL_NORMAL].y_offs;

	SpriteData *dst = (SpriteData *)dest_sprite->data;
	memset(dst, 0, sizeof(*dst));

	for (ZoomLevel z = zoom_min; z <= zoom_max; z++) {
		dst->offset[z][0] = z == zoom_min ? 0 : lengths[z - 1][1] + dst->offset[z - 1][1];
		dst->offset[z][1] = lengths[z][0] + dst->offset[z][0];

		memcpy(dst->data + dst->offset[z][0], dst_px_orig[z], lengths[z][0]);
		memcpy(dst->data + dst->offset[z][1], dst_n_orig[z],  lengths[z][1]);

		free(dst_px_orig[z]);
		free(dst_n_orig[z]);
	}

	return dest_sprite;
}

template Sprite *Blitter_32bppOptimized::EncodeInternal<true>(const SpriteLoader::SpriteCollection &sprite, AllocatorProc *allocator);
template Sprite *Blitter_32bppOptimized::EncodeInternal<false>(const SpriteLoader::SpriteCollection &sprite, AllocatorProc *allocator);

Sprite *Blitter_32bppOptimized::Encode(const SpriteLoader::SpriteCollection &sprite, AllocatorProc *allocator)
{
	return this->EncodeInternal<true>(sprite, allocator);
}
