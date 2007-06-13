#include "../stdafx.h"
#include "../zoom.hpp"
#include "../gfx.h"
#include "../debug.h"
#include "../table/sprites.h"
#include "../renderer/32bpp.hpp"
#include "32bpp_simple.hpp"

static FBlitter_32bppSimple iFBlitter_32bppSimple;

/**
 * Compose a color based on RGB values.
 */
static inline uint ComposeColor(uint r, uint g, uint b)
{
	return (r & 0xFF) << 16 | (g & 0xFF) << 8 | (b & 0xFF) << 0;
}

/**
 * Make a pixel looks like it is transparent.
 * @param color the color already on the screen.
 * @param amount the amount of transparency, times 100.
 * @return the new color for the screen.
 */
static inline uint MakeTransparent(uint color, uint amount)
{
	uint r, g, b;
	r = GB(color, 16, 8);
	g = GB(color, 8,  8);
	b = GB(color, 0,  8);

	return ComposeColor(r * amount / 100, g * amount / 100, b * amount / 100);
}

/**
 * Make a color grey-based.
 * @param color the color to make grey.
 * @return the new color, now grey.
 */
static inline uint MakeGrey(uint color)
{
	uint r, g, b;
	r = GB(color, 16, 8);
	g = GB(color, 8,  8);
	b = GB(color, 0,  8);

	/* To avoid doubles and stuff, multiple it with a total of 65536 (16bits), then
	 *  divide by it to normalize the value to a byte again. See heightmap.cpp for
	 *  information about the formula. */
	color = ((r * 19595) + (g * 38470) + (b * 7471)) / 65536;

	return ComposeColor(color, color, color);
}

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
						if (src->a != 0) *dst = ComposeColor(src->r, src->g, src->b);
					} else {
						if (bp->remap[src->m] != 0) *dst = Renderer_32bpp::LookupColourInPalette(bp->remap[src->m]);
					}
					break;

				case BM_TRANSPARENT:
					/* TODO -- We make an assumption here that the remap in fact is transparency, not some color.
					 *  This is never a problem with the code we produce, but newgrfs can make it fail... or at least:
					 *  we produce a result the newgrf maker didn't expect ;) */

					/* Make the current color a bit more black, so it looks like this image is transparent */
					if (src->a != 0) *dst = MakeTransparent(*dst, 75);
					break;

				default:
					if (src->a != 0) *dst = ComposeColor(src->r, src->g, src->b);
					break;
			}
			dst++;
			src += ScaleByZoom(1, zoom);
		}
	}
}

void Blitter_32bppSimple::DrawColorMappingRect(void *dst, int width, int height, int pal)
{
	uint32 *udst = (uint32 *)dst;

	if (pal == PALETTE_TO_TRANSPARENT) {
		do {
			for (int i = 0; i != width; i++) {
				*udst = MakeTransparent(*udst, 60);
				udst++;
			}
			udst = udst - width + _screen.pitch;
		} while (height--);
		return;
	}
	if (pal == PALETTE_TO_STRUCT_GREY) {
		do {
			for (int i = 0; i != width; i++) {
				*udst = MakeGrey(*udst);
				udst++;
			}
			udst = udst - width + _screen.pitch;
		} while (height--);
		return;
	}

	DEBUG(misc, 0, "32bpp blitter doesn't know how to draw this color table ('%d')", pal);
}

Sprite *Blitter_32bppSimple::Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator)
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
			uint color = Renderer_32bpp::LookupColourInPalette(dst[i].m);
			dst[i].r = GB(color, 16, 8);
			dst[i].g = GB(color, 8,  8);
			dst[i].b = GB(color, 0,  8);
		}
	}

	return dest_sprite;
}
