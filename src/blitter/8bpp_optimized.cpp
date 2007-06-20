/* $Id$ */

/** @file 8bpp_optimized.cpp */

#include "../stdafx.h"
#include "../zoom.hpp"
#include "../gfx.h"
#include "../debug.h"
#include "8bpp_optimized.hpp"

static FBlitter_8bppOptimized iFBlitter_8bppOptimized;

void Blitter_8bppOptimized::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
{
	const uint8 *src, *src_next;
	uint8 *dst, *dst_line;
	uint offset = 0;

	/* Find the offset of this zoom-level */
	offset = ((const uint8 *)bp->sprite)[(int)zoom * 2] | ((const byte *)bp->sprite)[(int)zoom * 2 + 1] << 8;

	/* Find where to start reading in the source sprite */
	src = (const uint8 *)bp->sprite + offset;
	dst_line = (uint8 *)bp->dst + bp->top * bp->pitch + bp->left;

	/* Skip over the top lines in the source image */
	for (int y = 0; y < bp->skip_top; y++) {
		uint trans, pixels;
		for (;;) {
			trans = *src++;
			pixels = *src++;
			if (trans == 0 && pixels == 0) break;
			src += pixels;
		}
	}

	src_next = src;

	for (int y = 0; y < bp->height; y++) {
		dst = dst_line;
		dst_line += bp->pitch;

		uint skip_left = bp->skip_left;
		int width = bp->width;

		for (;;) {
			src = src_next;
			uint8 trans = *src++;
			uint8 pixels = *src++;
			src_next = src + pixels;
			if (trans == 0 && pixels == 0) break;
			if (width <= 0) continue;

			if (skip_left != 0) {
				if (skip_left < trans) {
					trans -= skip_left;
					skip_left = 0;
				} else {
					skip_left -= trans;
					trans = 0;
				}
				if (skip_left < pixels) {
					src += skip_left;
					pixels -= skip_left;
					skip_left = 0;
				} else {
					src += pixels;
					skip_left -= pixels;
					pixels = 0;
				}
			}
			if (skip_left != 0) continue;

			/* Skip transparent pixels */
			dst += trans;
			width -= trans;
			if (width <= 0) continue;
			if (pixels > width) pixels = width;
			width -= pixels;

			switch (mode) {
				case BM_COLOUR_REMAP:
					for (uint x = 0; x < pixels; x++) {
						if (bp->remap[*src] != 0) *dst = bp->remap[*src];
						dst++; src++;
					}
					break;

				case BM_TRANSPARENT:
					for (uint x = 0; x < pixels; x++) {
						*dst = bp->remap[*dst];
						dst++; src++;
					}
					break;

				default:
					memcpy(dst, src, pixels);
					dst += pixels; src += pixels;
					break;
			}
		}
	}
}

Sprite *Blitter_8bppOptimized::Encode(SpriteLoader::Sprite *sprite, Blitter::AllocatorProc *allocator)
{
	Sprite *dest_sprite;
	byte *temp_dst;
	uint memory = 0;
	uint index = 0;

	/* Make memory for all zoom-levels */
	memory += (int)ZOOM_LVL_END * sizeof(uint16);
	for (int i = 0; i < (int)ZOOM_LVL_END; i++) {
		memory += UnScaleByZoom(sprite->height, (ZoomLevel)i) * UnScaleByZoom(sprite->width, (ZoomLevel)i);
		index += 2;
	}

	/* We have no idea how much memory we really need, so just guess something */
	memory *= 5;
	temp_dst = MallocT<byte>(memory);

	/* Make the sprites per zoom-level */
	for (int i = 0; i < (int)ZOOM_LVL_END; i++) {
		/* Store the scaled image */
		const SpriteLoader::CommonPixel *src;

		/* Store the index table */
		temp_dst[i * 2] = index & 0xFF;
		temp_dst[i * 2 + 1] = (index >> 8) & 0xFF;

		byte *dst = &temp_dst[index];

		for (int y = 0; y < UnScaleByZoom(sprite->height, (ZoomLevel)i); y++) {
			uint trans = 0;
			uint pixels = 0;
			uint last_color = 0;
			uint count_index = 0;
			uint rx = 0;
			src = &sprite->data[ScaleByZoom(y, (ZoomLevel)i) * sprite->width];

			for (int x = 0; x < UnScaleByZoom(sprite->width, (ZoomLevel)i); x++) {
				uint color = 0;

				/* Get the color keeping in mind the zoom-level */
				for (int j = 0; j < ScaleByZoom(1, (ZoomLevel)i); j++) {
					if (src->m != 0) color = src->m;
					src++;
					rx++;
					/* Because of the scaling it might happen we read outside the buffer. Avoid that. */
					if (rx == sprite->width) break;
				}

				if (last_color == 0 || color == 0 || pixels == 255) {
					if (count_index != 0) {
						/* Write how many non-transparent bytes we get */
						temp_dst[count_index] = pixels;
						pixels = 0;
						count_index = 0;
					}
					/* As long as we find transparency bytes, keep counting */
					if (color == 0) {
						last_color = 0;
						trans++;
						continue;
					}
					/* No longer transparency, so write the amount of transparent bytes */
					*dst = trans;
					dst++; index++;
					trans = 0;
					/* Reserve a byte for the pixel counter */
					count_index = index;
					dst++; index++;
				}
				last_color = color;
				pixels++;
				*dst = color;
				dst++; index++;
			}

			if (count_index != 0) temp_dst[count_index] = pixels;

			/* Write line-ending */
			*dst = 0; dst++; index++;
			*dst = 0; dst++; index++;
		}
	}

	/* Safety check, to make sure we guessed the size correctly */
	assert(index < memory);

	/* Allocate the exact amount of memory we need */
	dest_sprite = (Sprite *)allocator(sizeof(*dest_sprite) + index);

	dest_sprite->height = sprite->height;
	dest_sprite->width  = sprite->width;
	dest_sprite->x_offs = sprite->x_offs;
	dest_sprite->y_offs = sprite->y_offs;
	memcpy(dest_sprite->data, temp_dst, index);

	return dest_sprite;
}
