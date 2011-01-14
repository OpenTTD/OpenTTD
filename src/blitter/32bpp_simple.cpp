/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_simple.cpp Implementation of the simple 32 bpp blitter. */

#include "../stdafx.h"
#include "../zoom_func.h"
#include "32bpp_simple.hpp"

#include "../table/sprites.h"

static FBlitter_32bppSimple iFBlitter_32bppSimple;

void Blitter_32bppSimple::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
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
			switch (mode) {
				case BM_COLOUR_REMAP:
					/* In case the m-channel is zero, do not remap this pixel in any way */
					if (src->m == 0) {
						if (src->a != 0) *dst = ComposeColourRGBA(src->r, src->g, src->b, src->a, *dst);
					} else {
						if (bp->remap[src->m] != 0) *dst = ComposeColourPA(this->LookupColourInPalette(bp->remap[src->m]), src->a, *dst);
					}
					break;

				case BM_TRANSPARENT:
					/* TODO -- We make an assumption here that the remap in fact is transparency, not some colour.
					 *  This is never a problem with the code we produce, but newgrfs can make it fail... or at least:
					 *  we produce a result the newgrf maker didn't expect ;) */

					/* Make the current colour a bit more black, so it looks like this image is transparent */
					if (src->a != 0) *dst = MakeTransparent(*dst, 192);
					break;

				default:
					if (src->a != 0) *dst = ComposeColourRGBA(src->r, src->g, src->b, src->a, *dst);
					break;
			}
			dst++;
			src += ScaleByZoom(1, zoom);
		}
	}
}

void Blitter_32bppSimple::DrawColourMappingRect(void *dst, int width, int height, PaletteID pal)
{
	uint32 *udst = (uint32 *)dst;

	if (pal == PALETTE_TO_TRANSPARENT) {
		do {
			for (int i = 0; i != width; i++) {
				*udst = MakeTransparent(*udst, 154);
				udst++;
			}
			udst = udst - width + _screen.pitch;
		} while (--height);
		return;
	}
	if (pal == PALETTE_TO_STRUCT_GREY) {
		do {
			for (int i = 0; i != width; i++) {
				*udst = MakeGrey(*udst);
				udst++;
			}
			udst = udst - width + _screen.pitch;
		} while (--height);
		return;
	}

	DEBUG(misc, 0, "32bpp blitter doesn't know how to draw this colour table ('%d')", pal);
}

Sprite *Blitter_32bppSimple::Encode(SpriteLoader::Sprite *sprite, AllocatorProc *allocator)
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
	for (int i = 0; i < sprite->height * sprite->width; i++) {
		if (dst[i].m != 0) {
			/* Pre-convert the mapping channel to a RGB value */
			uint colour = this->LookupColourInPalette(dst[i].m);
			dst[i].r = GB(colour, 16, 8);
			dst[i].g = GB(colour, 8,  8);
			dst[i].b = GB(colour, 0,  8);
		}
	}

	return dest_sprite;
}
