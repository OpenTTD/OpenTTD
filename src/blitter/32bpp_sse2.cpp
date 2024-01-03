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
#include "32bpp_sse_func.hpp"

#include "../safeguards.h"

/** Instantiation of the SSE2 32bpp blitter factory. */
static FBlitter_32bppSSE2 iFBlitter_32bppSSE2;

Sprite *Blitter_32bppSSE_Base::Encode(const SpriteLoader::SpriteCollection &sprite, AllocatorProc *allocator)
{
	/* First uint32_t of a line = the number of transparent pixels from the left.
	 * Second uint32_t of a line = the number of transparent pixels from the right.
	 * Then all RGBA then all MV.
	 */
	ZoomLevel zoom_min = ZOOM_LVL_NORMAL;
	ZoomLevel zoom_max = ZOOM_LVL_NORMAL;
	if (sprite[ZOOM_LVL_NORMAL].type != SpriteType::Font) {
		zoom_min = _settings_client.gui.zoom_min;
		zoom_max = _settings_client.gui.zoom_max;
		if (zoom_max == zoom_min) zoom_max = ZOOM_LVL_MAX;
	}

	/* Calculate sizes and allocate. */
	SpriteData sd;
	memset(&sd, 0, sizeof(sd));
	uint all_sprites_size = 0;
	for (ZoomLevel z = zoom_min; z <= zoom_max; z++) {
		const SpriteLoader::Sprite *src_sprite = &sprite[z];
		sd.infos[z].sprite_width = src_sprite->width;
		sd.infos[z].sprite_offset = all_sprites_size;
		sd.infos[z].sprite_line_size = sizeof(Colour) * src_sprite->width + sizeof(uint32_t) * META_LENGTH;

		const uint rgba_size = sd.infos[z].sprite_line_size * src_sprite->height;
		sd.infos[z].mv_offset = all_sprites_size + rgba_size;

		const uint mv_size = sizeof(MapValue) * src_sprite->width * src_sprite->height;
		all_sprites_size += rgba_size + mv_size;
	}

	Sprite *dst_sprite = (Sprite *) allocator(sizeof(Sprite) + sizeof(SpriteData) + all_sprites_size);
	dst_sprite->height = sprite[ZOOM_LVL_NORMAL].height;
	dst_sprite->width  = sprite[ZOOM_LVL_NORMAL].width;
	dst_sprite->x_offs = sprite[ZOOM_LVL_NORMAL].x_offs;
	dst_sprite->y_offs = sprite[ZOOM_LVL_NORMAL].y_offs;
	memcpy(dst_sprite->data, &sd, sizeof(SpriteData));

	/* Copy colours and determine flags. */
	bool has_remap = false;
	bool has_anim = false;
	bool has_translucency = false;
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
					if (src->a != 0 && src->a != 255) has_translucency = true;
					dst_mv->m = src->m;
					if (src->m != 0) {
						/* Do some accounting for flags. */
						has_remap = true;
						if (src->m >= PALETTE_ANIM_START) has_anim = true;

						/* Get brightest value (or default brightness if it's a black pixel). */
						const uint8_t rgb_max = std::max({src->r, src->g, src->b});
						dst_mv->v = (rgb_max == 0) ? Blitter_32bppBase::DEFAULT_BRIGHTNESS : rgb_max;

						/* Pre-convert the mapping channel to a RGB value. */
						const Colour colour = AdjustBrightneSSE(Blitter_32bppBase::LookupColourInPalette(src->m), dst_mv->v);
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
					*(uint16_t*) dst_mv = 0;
				}
				dst_rgba++;
				dst_mv++;
				src++;
			}

			/* Count the number of transparent pixels from the left. */
			dst_rgba = dst_rgba_line + META_LENGTH;
			uint32_t nb_pix_transp = 0;
			for (uint x = src_sprite->width; x != 0; x--) {
				if (dst_rgba->a == 0) nb_pix_transp++;
				else break;
				dst_rgba++;
			}
			(*dst_rgba_line).data = nb_pix_transp;

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
			(*nb_right).data = nb_pix_transp;
		}
	}

	/* Store sprite flags. */
	sd.flags = SF_NONE;
	if (has_translucency) sd.flags |= SF_TRANSLUCENT;
	if (!has_remap) sd.flags |= SF_NO_REMAP;
	if (!has_anim) sd.flags |= SF_NO_ANIM;
	memcpy(dst_sprite->data, &sd, sizeof(SpriteData));

	return dst_sprite;
}

#endif /* WITH_SSE */
