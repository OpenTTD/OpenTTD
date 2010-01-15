/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_optimized.cpp Implementation of the optimized 32 bpp blitter. */

#include "../stdafx.h"
#include "../zoom_func.h"
#include "../core/math_func.hpp"
#include "32bpp_optimized.hpp"

static FBlitter_32bppOptimized iFBlitter_32bppOptimized;

/**
 * Draws a sprite to a (screen) buffer. It is templated to allow faster operation.
 *
 * @tparam mode blitter mode
 * @param bp further blitting parameters
 * @param zoom zoom level at which we are drawing
 */
template <BlitterMode mode>
inline void Blitter_32bppOptimized::Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom)
{
	const SpriteData *src = (const SpriteData *)bp->sprite;

	/* src_px : each line begins with uint32 n = 'number of bytes in this line',
	 *          then n times is the Colour struct for this line */
	const Colour *src_px = (const Colour *)(src->data + src->offset[zoom][0]);
	/* src_n  : each line begins with uint32 n = 'number of bytes in this line',
	 *          then interleaved stream of 'm' and 'n' channels. 'm' is remap,
	 *          'n' is number of bytes with the same alpha channel class */
	const uint8  *src_n  = (const uint8  *)(src->data + src->offset[zoom][1]);

	/* skip upper lines in src_px and src_n */
	for (uint i = bp->skip_top; i != 0; i--) {
		src_px = (const Colour *)((const byte *)src_px + *(const uint32 *)src_px);
		src_n += *(uint32 *)src_n;
	}

	/* skip lines in dst */
	uint32 *dst = (uint32 *)bp->dst + bp->top * bp->pitch + bp->left;

	/* store so we don't have to access it via bp everytime (compiler assumes pointer aliasing) */
	const byte *remap = bp->remap;

	for (int y = 0; y < bp->height; y++) {
		/* next dst line begins here */
		uint32 *dst_ln = dst + bp->pitch;

		/* next src line begins here */
		const Colour *src_px_ln = (const Colour *)((const byte *)src_px + *(const uint32 *)src_px);
		src_px++;

		/* next src_n line begins here */
		const uint8 *src_n_ln = src_n + *(uint32 *)src_n;
		src_n += 4;

		/* we will end this line when we reach this point */
		uint32 *dst_end = dst + bp->skip_left;

		/* number of pixels with the same aplha channel class */
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

					n = min<uint>(n - d, (uint)bp->width);
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
			n = min<uint>(*src_n++, (uint)(dst_end - dst));

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
								uint r = remap[m];
								if (r != 0) *dst = this->LookupColourInPalette(r);
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
								uint r = remap[m];
								if (r != 0) *dst = ComposeColourPANoCheck(this->LookupColourInPalette(r), src_px->a, *dst);
							}
							dst++;
							src_px++;
							src_n++;
						} while (--n != 0);
					}
					break;

				case BM_TRANSPARENT:
					/* TODO -- We make an assumption here that the remap in fact is transparency, not some colour.
					 *  This is never a problem with the code we produce, but newgrfs can make it fail... or at least:
					 *  we produce a result the newgrf maker didn't expect ;) */

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

				default:
					if (src_px->a == 255) {
						/* faster than memcpy(), n is usually low */
						src_n += n;
						do {
							*dst = src_px->data;
							dst++;
							src_px++;
						} while (--n != 0);
					} else {
						src_n += n;
						do {
							*dst = ComposeColourRGBANoCheck(src_px->r, src_px->g, src_px->b, src_px->a, *dst);
							dst++;
							src_px++;
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

/**
 * Draws a sprite to a (screen) buffer. Calls adequate templated function.
 *
 * @param bp further blitting parameters
 * @param mode blitter mode
 * @param zoom zoom level at which we are drawing
 */
void Blitter_32bppOptimized::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
{
	switch (mode) {
		default: NOT_REACHED();
		case BM_NORMAL:       Draw<BM_NORMAL>      (bp, zoom); return;
		case BM_COLOUR_REMAP: Draw<BM_COLOUR_REMAP>(bp, zoom); return;
		case BM_TRANSPARENT:  Draw<BM_TRANSPARENT> (bp, zoom); return;
	}
}

/**
 * Resizes the sprite in a very simple way, takes every n-th pixel and every n-th row
 *
 * @param sprite_src sprite to resize
 * @param zoom resizing scale
 * @return resized sprite
 */
static const SpriteLoader::Sprite *ResizeSprite(const SpriteLoader::Sprite *sprite_src, ZoomLevel zoom)
{
	SpriteLoader::Sprite *sprite = MallocT<SpriteLoader::Sprite>(1);

	if (zoom == ZOOM_LVL_NORMAL) {
		memcpy(sprite, sprite_src, sizeof(*sprite));
		uint size = sprite_src->height * sprite_src->width;
		sprite->data = MallocT<SpriteLoader::CommonPixel>(size);
		memcpy(sprite->data, sprite_src->data, size * sizeof(SpriteLoader::CommonPixel));
		return sprite;
	}

	sprite->height = UnScaleByZoom(sprite_src->height, zoom);
	sprite->width  = UnScaleByZoom(sprite_src->width,  zoom);
	sprite->x_offs = UnScaleByZoom(sprite_src->x_offs, zoom);
	sprite->y_offs = UnScaleByZoom(sprite_src->y_offs, zoom);

	uint size = sprite->height * sprite->width;
	SpriteLoader::CommonPixel *dst = sprite->data = CallocT<SpriteLoader::CommonPixel>(size);

	const SpriteLoader::CommonPixel *src = (SpriteLoader::CommonPixel *)sprite_src->data;
	const SpriteLoader::CommonPixel *src_end = src + sprite_src->height * sprite_src->width;

	uint scaled_1 = ScaleByZoom(1, zoom);

	for (uint y = 0; y < sprite->height; y++) {
		if (src >= src_end) src = src_end - sprite_src->width;

		const SpriteLoader::CommonPixel *src_ln = src + sprite_src->width * scaled_1;
		for (uint x = 0; x < sprite->width; x++) {
			if (src >= src_ln) src = src_ln - 1;
			*dst = *src;
			dst++;
			src += scaled_1;
		}

		src = src_ln;
	}

	return sprite;
}

Sprite *Blitter_32bppOptimized::Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator)
{
	/* streams of pixels (a, r, g, b channels)
	 *
	 * stored in separated stream so data are always aligned on 4B boundary */
	Colour *dst_px_orig[ZOOM_LVL_COUNT];

	/* interleaved stream of 'm' channel and 'n' channel
	 * 'n' is number if following pixels with the same alpha channel class
	 * there are 3 classes: 0, 255, others
	 *
	 * it has to be stored in one stream so fewer registers are used -
	 * x86 has problems with register allocation even with this solution */
	uint8  *dst_n_orig[ZOOM_LVL_COUNT];

	/* lengths of streams */
	uint32 lengths[ZOOM_LVL_COUNT][2];

	for (ZoomLevel z = ZOOM_LVL_BEGIN; z < ZOOM_LVL_END; z++) {
		const SpriteLoader::Sprite *src_orig = ResizeSprite(sprite, z);

		uint size = src_orig->height * src_orig->width;

		dst_px_orig[z] = CallocT<Colour>(size + src_orig->height * 2);
		dst_n_orig[z]  = CallocT<uint8>(size * 2 + src_orig->height * 4 * 2);

		uint32 *dst_px_ln = (uint32 *)dst_px_orig[z];
		uint32 *dst_n_ln  = (uint32 *)dst_n_orig[z];

		const SpriteLoader::CommonPixel *src = (const SpriteLoader::CommonPixel *)src_orig->data;

		for (uint y = src_orig->height; y > 0; y--) {
			Colour *dst_px = (Colour *)(dst_px_ln + 1);
			uint8 *dst_n = (uint8 *)(dst_n_ln + 1);

			uint8 *dst_len = dst_n++;

			uint last = 3;
			int len = 0;

			for (uint x = src_orig->width; x > 0; x--) {
				uint8 a = src->a;
				uint t = a > 0 && a < 255 ? 1 : a;

				if (last != t || len == 255) {
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
						/* Pre-convert the mapping channel to a RGB value */
						uint32 colour = this->LookupColourInPalette(src->m);
						dst_px->r = GB(colour, 16, 8);
						dst_px->g = GB(colour, 8,  8);
						dst_px->b = GB(colour, 0,  8);
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
			dst_n  = (uint8 *)AlignPtr(dst_n, 4);

			*dst_px_ln = (uint8 *)dst_px - (uint8 *)dst_px_ln;
			*dst_n_ln  = (uint8 *)dst_n  - (uint8 *)dst_n_ln;

			dst_px_ln = (uint32 *)dst_px;
			dst_n_ln =  (uint32 *)dst_n;
		}

		lengths[z][0] = (byte *)dst_px_ln - (byte *)dst_px_orig[z]; // all are aligned to 4B boundary
		lengths[z][1] = (byte *)dst_n_ln  - (byte *)dst_n_orig[z];

		free(src_orig->data);
		free((void *)src_orig);
	}

	uint len = 0; // total length of data
	for (ZoomLevel z = ZOOM_LVL_BEGIN; z < ZOOM_LVL_END; z++) {
		len += lengths[z][0] + lengths[z][1];
	}

	Sprite *dest_sprite = (Sprite *)allocator(sizeof(*dest_sprite) + sizeof(SpriteData) + len);

	dest_sprite->height = sprite->height;
	dest_sprite->width  = sprite->width;
	dest_sprite->x_offs = sprite->x_offs;
	dest_sprite->y_offs = sprite->y_offs;

	SpriteData *dst = (SpriteData *)dest_sprite->data;

	for (ZoomLevel z = ZOOM_LVL_BEGIN; z < ZOOM_LVL_END; z++) {
		dst->offset[z][0] = z == ZOOM_LVL_BEGIN ? 0 : lengths[z - 1][1] + dst->offset[z - 1][1];
		dst->offset[z][1] = lengths[z][0] + dst->offset[z][0];

		memcpy(dst->data + dst->offset[z][0], dst_px_orig[z], lengths[z][0]);
		memcpy(dst->data + dst->offset[z][1], dst_n_orig[z],  lengths[z][1]);

		free(dst_px_orig[z]);
		free(dst_n_orig[z]);
	}

	return dest_sprite;
}
