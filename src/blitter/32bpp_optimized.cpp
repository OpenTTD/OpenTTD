/* $Id$ */

/** @file 32bpp_optimized.cpp Implementation of the optimized 32 bpp blitter. */

#include "../stdafx.h"
#include "../zoom_func.h"
#include "../gfx_func.h"
#include "../debug.h"
#include "32bpp_optimized.hpp"

static FBlitter_32bppOptimized iFBlitter_32bppOptimized;

void Blitter_32bppOptimized::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
{
	const SpriteLoader::CommonPixel *src, *src_line;
	uint32 *dst, *dst_line;

	/* Find where to start reading in the source sprite */
	src_line = (const SpriteLoader::CommonPixel *)bp->sprite + (bp->skip_top * bp->sprite_width + bp->skip_left) * ScaleByZoom(1, zoom);
	dst_line = (uint32 *)bp->dst + bp->top * bp->pitch + bp->left;

	for (int y = 0; y < bp->height; y++) {
		dst = dst_line;
		dst_line += bp->pitch;

		src = src_line;
		src_line += bp->sprite_width * ScaleByZoom(1, zoom);

		for (int x = 0; x < bp->width; x++) {
			if (src->a == 0) {
				/* src->r is 'misused' here to indicate how much more pixels are following with an alpha of 0 */
				int skip = UnScaleByZoom(src->r, zoom);

				dst += skip;
				x   += skip - 1;
				src += ScaleByZoom(1, zoom) * skip;
				continue;
			}

			switch (mode) {
				case BM_COLOUR_REMAP:
					/* In case the m-channel is zero, do not remap this pixel in any way */
					if (src->m == 0) {
						*dst = ComposeColourRGBA(src->r, src->g, src->b, src->a, *dst);
					} else {
						if (bp->remap[src->m] != 0) *dst = ComposeColourPA(this->LookupColourInPalette(bp->remap[src->m]), src->a, *dst);
					}
					break;

				case BM_TRANSPARENT:
					/* TODO -- We make an assumption here that the remap in fact is transparency, not some color.
					 *  This is never a problem with the code we produce, but newgrfs can make it fail... or at least:
					 *  we produce a result the newgrf maker didn't expect ;) */

					/* Make the current color a bit more black, so it looks like this image is transparent */
					*dst = MakeTransparent(*dst, 192);
					break;

				default:
					*dst = ComposeColourRGBA(src->r, src->g, src->b, src->a, *dst);
					break;
			}
			dst++;
			src += ScaleByZoom(1, zoom);
		}
	}
}

Sprite *Blitter_32bppOptimized::Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator)
{
	Sprite *dest_sprite;
	SpriteLoader::CommonPixel *dst;
	dest_sprite = (Sprite *)allocator(sizeof(*dest_sprite) + sprite->height * sprite->width * sizeof(SpriteLoader::CommonPixel));

	dest_sprite->height = sprite->height;
	dest_sprite->width  = sprite->width;
	dest_sprite->x_offs = sprite->x_offs;
	dest_sprite->y_offs = sprite->y_offs;

	dst = (SpriteLoader::CommonPixel *)dest_sprite->data;

	memcpy(dst, sprite->data, sprite->height * sprite->width * sizeof(SpriteLoader::CommonPixel));
	/* Skip to the end of the array, and work backwards to find transparent blocks */
	dst = dst + sprite->height * sprite->width - 1;

	for (uint y = sprite->height; y > 0; y--) {
		int trans = 0;
		/* Process sprite line backwards, to compute lengths of transparent blocks */
		for (uint x = sprite->width; x > 0; x--) {
			if (dst->a == 0) {
				/* Save transparent block length in red channel; max value is 255 the red channel can contain */
				if (trans < 255) trans++;
				dst->r = trans;
				dst->g = 0;
				dst->b = 0;
				dst->m = 0;
			} else {
				trans = 0;
				if (dst->m != 0) {
					/* Pre-convert the mapping channel to a RGB value */
					uint color = this->LookupColourInPalette(dst->m);
					dst->r = GB(color, 16, 8);
					dst->g = GB(color, 8,  8);
					dst->b = GB(color, 0,  8);
				}
			}
			dst--;
		}
	}
	return dest_sprite;
}
