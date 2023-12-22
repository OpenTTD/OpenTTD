/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 8bpp_optimized.cpp Implementation of the optimized 8 bpp blitter. */

#include "../stdafx.h"
#include "../zoom_func.h"
#include "../settings_type.h"
#include "../core/math_func.hpp"
#include "../core/mem_func.hpp"
#include "8bpp_optimized.hpp"

#include "../safeguards.h"

/** Instantiation of the 8bpp optimised blitter factory. */
static FBlitter_8bppOptimized iFBlitter_8bppOptimized;

void Blitter_8bppOptimized::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
{
	/* Find the offset of this zoom-level */
	const SpriteData *sprite_src = (const SpriteData *)bp->sprite;
	uint offset = sprite_src->offset[zoom];

	/* Find where to start reading in the source sprite */
	const uint8_t *src = sprite_src->data + offset;
	uint8_t *dst_line = (uint8_t *)bp->dst + bp->top * bp->pitch + bp->left;

	/* Skip over the top lines in the source image */
	for (int y = 0; y < bp->skip_top; y++) {
		for (;;) {
			uint trans = *src++;
			uint pixels = *src++;
			if (trans == 0 && pixels == 0) break;
			src += pixels;
		}
	}

	const uint8_t *src_next = src;

	for (int y = 0; y < bp->height; y++) {
		uint8_t *dst = dst_line;
		dst_line += bp->pitch;

		uint skip_left = bp->skip_left;
		int width = bp->width;

		for (;;) {
			src = src_next;
			uint trans = *src++;
			uint pixels = *src++;
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
			if (width <= 0 || pixels == 0) continue;
			pixels = std::min<uint>(pixels, width);
			width -= pixels;

			switch (mode) {
				case BM_COLOUR_REMAP:
				case BM_CRASH_REMAP: {
					const uint8_t *remap = bp->remap;
					do {
						uint m = remap[*src];
						if (m != 0) *dst = m;
						dst++; src++;
					} while (--pixels != 0);
					break;
				}

				case BM_BLACK_REMAP:
					MemSetT(dst, 0, pixels);
					dst += pixels;
					break;

				case BM_TRANSPARENT:
				case BM_TRANSPARENT_REMAP: {
					const uint8_t *remap = bp->remap;
					src += pixels;
					do {
						*dst = remap[*dst];
						dst++;
					} while (--pixels != 0);
					break;
				}

				default:
					MemCpyT(dst, src, pixels);
					dst += pixels; src += pixels;
					break;
			}
		}
	}
}

Sprite *Blitter_8bppOptimized::Encode(const SpriteLoader::SpriteCollection &sprite, AllocatorProc *allocator)
{
	/* Make memory for all zoom-levels */
	uint memory = sizeof(SpriteData);

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

	for (ZoomLevel i = zoom_min; i <= zoom_max; i++) {
		memory += sprite[i].width * sprite[i].height;
	}

	/* We have no idea how much memory we really need, so just guess something */
	memory *= 5;

	/* Don't allocate memory each time, but just keep some
	 * memory around as this function is called quite often
	 * and the memory usage is quite low. */
	static ReusableBuffer<byte> temp_buffer;
	SpriteData *temp_dst = (SpriteData *)temp_buffer.Allocate(memory);
	memset(temp_dst, 0, sizeof(*temp_dst));
	byte *dst = temp_dst->data;

	/* Make the sprites per zoom-level */
	for (ZoomLevel i = zoom_min; i <= zoom_max; i++) {
		/* Store the index table */
		uint offset = dst - temp_dst->data;
		temp_dst->offset[i] = offset;

		/* cache values, because compiler can't cache it */
		int scaled_height = sprite[i].height;
		int scaled_width  = sprite[i].width;

		for (int y = 0; y < scaled_height; y++) {
			uint trans = 0;
			uint pixels = 0;
			uint last_colour = 0;
			byte *count_dst = nullptr;

			/* Store the scaled image */
			const SpriteLoader::CommonPixel *src = &sprite[i].data[y * sprite[i].width];

			for (int x = 0; x < scaled_width; x++) {
				uint colour = src++->m;

				if (last_colour == 0 || colour == 0 || pixels == 255) {
					if (count_dst != nullptr) {
						/* Write how many non-transparent bytes we get */
						*count_dst = pixels;
						pixels = 0;
						count_dst = nullptr;
					}
					/* As long as we find transparency bytes, keep counting */
					if (colour == 0 && trans != 255) {
						last_colour = 0;
						trans++;
						continue;
					}
					/* No longer transparency, so write the amount of transparent bytes */
					*dst = trans;
					dst++;
					trans = 0;
					/* Reserve a byte for the pixel counter */
					count_dst = dst;
					dst++;
				}
				last_colour = colour;
				if (colour == 0) {
					trans++;
				} else {
					pixels++;
					*dst = colour;
					dst++;
				}
			}

			if (count_dst != nullptr) *count_dst = pixels;

			/* Write line-ending */
			*dst = 0; dst++;
			*dst = 0; dst++;
		}
	}

	uint size = dst - (byte *)temp_dst;

	/* Safety check, to make sure we guessed the size correctly */
	assert(size < memory);

	/* Allocate the exact amount of memory we need */
	Sprite *dest_sprite = (Sprite *)allocator(sizeof(*dest_sprite) + size);

	dest_sprite->height = sprite[ZOOM_LVL_NORMAL].height;
	dest_sprite->width  = sprite[ZOOM_LVL_NORMAL].width;
	dest_sprite->x_offs = sprite[ZOOM_LVL_NORMAL].x_offs;
	dest_sprite->y_offs = sprite[ZOOM_LVL_NORMAL].y_offs;
	memcpy(dest_sprite->data, temp_dst, size);

	return dest_sprite;
}
