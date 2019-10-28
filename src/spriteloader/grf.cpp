/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file grf.cpp Reading graphics data from (New)GRF files. */

#include "../stdafx.h"
#include "../gfx_func.h"
#include "../fileio_func.h"
#include "../debug.h"
#include "../strings_func.h"
#include "table/strings.h"
#include "../error.h"
#include "../core/math_func.hpp"
#include "../core/alloc_type.hpp"
#include "../core/bitmath_func.hpp"
#include "grf.hpp"

#include "../safeguards.h"

extern const byte _palmap_w2d[];

/** The different colour components a sprite can have. */
enum SpriteColourComponent {
	SCC_RGB   = 1 << 0, ///< Sprite has RGB.
	SCC_ALPHA = 1 << 1, ///< Sprite has alpha.
	SCC_PAL   = 1 << 2, ///< Sprite has palette data.
	SCC_MASK  = SCC_RGB | SCC_ALPHA | SCC_PAL, ///< Mask of valid colour bits.
};
DECLARE_ENUM_AS_BIT_SET(SpriteColourComponent)

/**
 * We found a corrupted sprite. This means that the sprite itself
 * contains invalid data or is too small for the given dimensions.
 * @param file_slot the file the errored sprite is in
 * @param file_pos the location in the file of the errored sprite
 * @param line the line where the error occurs.
 * @return always false (to tell loading the sprite failed)
 */
static bool WarnCorruptSprite(uint8 file_slot, size_t file_pos, int line)
{
	static byte warning_level = 0;
	if (warning_level == 0) {
		SetDParamStr(0, FioGetFilename(file_slot));
		ShowErrorMessage(STR_NEWGRF_ERROR_CORRUPT_SPRITE, INVALID_STRING_ID, WL_ERROR);
	}
	DEBUG(sprite, warning_level, "[%i] Loading corrupted sprite from %s at position %i", line, FioGetFilename(file_slot), (int)file_pos);
	warning_level = 6;
	return false;
}

/**
 * Decode the image data of a single sprite.
 * @param[in,out] sprite Filled with the sprite image data.
 * @param file_slot File slot.
 * @param file_pos File position.
 * @param sprite_type Type of the sprite we're decoding.
 * @param num Size of the decompressed sprite.
 * @param type Type of the encoded sprite.
 * @param zoom_lvl Requested zoom level.
 * @param colour_fmt Colour format of the sprite.
 * @param container_format Container format of the GRF this sprite is in.
 * @return True if the sprite was successfully loaded.
 */
bool DecodeSingleSprite(SpriteLoader::Sprite *sprite, uint8 file_slot, size_t file_pos, SpriteType sprite_type, int64 num, byte type, ZoomLevel zoom_lvl, byte colour_fmt, byte container_format)
{
	std::unique_ptr<byte[]> dest_orig(new byte[num]);
	byte *dest = dest_orig.get();
	const int64 dest_size = num;

	/* Read the file, which has some kind of compression */
	while (num > 0) {
		int8 code = FioReadByte();

		if (code >= 0) {
			/* Plain bytes to read */
			int size = (code == 0) ? 0x80 : code;
			num -= size;
			if (num < 0) return WarnCorruptSprite(file_slot, file_pos, __LINE__);
			for (; size > 0; size--) {
				*dest = FioReadByte();
				dest++;
			}
		} else {
			/* Copy bytes from earlier in the sprite */
			const uint data_offset = ((code & 7) << 8) | FioReadByte();
			if (dest - data_offset < dest_orig.get()) return WarnCorruptSprite(file_slot, file_pos, __LINE__);
			int size = -(code >> 3);
			num -= size;
			if (num < 0) return WarnCorruptSprite(file_slot, file_pos, __LINE__);
			for (; size > 0; size--) {
				*dest = *(dest - data_offset);
				dest++;
			}
		}
	}

	if (num != 0) return WarnCorruptSprite(file_slot, file_pos, __LINE__);

	sprite->AllocateData(zoom_lvl, sprite->width * sprite->height);

	/* Convert colour depth to pixel size. */
	int bpp = 0;
	if (colour_fmt & SCC_RGB)   bpp += 3; // Has RGB data.
	if (colour_fmt & SCC_ALPHA) bpp++;    // Has alpha data.
	if (colour_fmt & SCC_PAL)   bpp++;    // Has palette data.

	/* When there are transparency pixels, this format has another trick.. decode it */
	if (type & 0x08) {
		for (int y = 0; y < sprite->height; y++) {
			bool last_item = false;
			/* Look up in the header-table where the real data is stored for this row */
			int offset;
			if (container_format >= 2 && dest_size > UINT16_MAX) {
				offset = (dest_orig[y * 4 + 3] << 24) | (dest_orig[y * 4 + 2] << 16) | (dest_orig[y * 4 + 1] << 8) | dest_orig[y * 4];
			} else {
				offset = (dest_orig[y * 2 + 1] << 8) | dest_orig[y * 2];
			}

			/* Go to that row */
			dest = dest_orig.get() + offset;

			do {
				if (dest + (container_format >= 2 && sprite->width > 256 ? 4 : 2) > dest_orig.get() + dest_size) {
					return WarnCorruptSprite(file_slot, file_pos, __LINE__);
				}

				SpriteLoader::CommonPixel *data;
				/* Read the header. */
				int length, skip;
				if (container_format >= 2 && sprite->width > 256) {
					/*  0 .. 14  - length
					 *  15       - last_item
					 *  16 .. 31 - transparency bytes */
					last_item = (dest[1] & 0x80) != 0;
					length    = ((dest[1] & 0x7F) << 8) | dest[0];
					skip      = (dest[3] << 8) | dest[2];
					dest += 4;
				} else {
					/*  0 .. 6  - length
					 *  7       - last_item
					 *  8 .. 15 - transparency bytes */
					last_item  = ((*dest) & 0x80) != 0;
					length =  (*dest++) & 0x7F;
					skip   =   *dest++;
				}

				data = &sprite->data[y * sprite->width + skip];

				if (skip + length > sprite->width || dest + length * bpp > dest_orig.get() + dest_size) {
					return WarnCorruptSprite(file_slot, file_pos, __LINE__);
				}

				for (int x = 0; x < length; x++) {
					if (colour_fmt & SCC_RGB) {
						data->r = *dest++;
						data->g = *dest++;
						data->b = *dest++;
					}
					data->a = (colour_fmt & SCC_ALPHA) ? *dest++ : 0xFF;
					if (colour_fmt & SCC_PAL) {
						switch (sprite_type) {
							case ST_NORMAL: data->m = _palette_remap_grf[file_slot] ? _palmap_w2d[*dest] : *dest; break;
							case ST_FONT:   data->m = min(*dest, 2u); break;
							default:        data->m = *dest; break;
						}
						/* Magic blue. */
						if (colour_fmt == SCC_PAL && *dest == 0) data->a = 0x00;
						dest++;
					}
					data++;
				}
			} while (!last_item);
		}
	} else {
		if (dest_size < sprite->width * sprite->height * bpp) {
			return WarnCorruptSprite(file_slot, file_pos, __LINE__);
		}

		if (dest_size > sprite->width * sprite->height * bpp) {
			static byte warning_level = 0;
			DEBUG(sprite, warning_level, "Ignoring " OTTD_PRINTF64 " unused extra bytes from the sprite from %s at position %i", dest_size - sprite->width * sprite->height * bpp, FioGetFilename(file_slot), (int)file_pos);
			warning_level = 6;
		}

		dest = dest_orig.get();

		for (int i = 0; i < sprite->width * sprite->height; i++) {
			byte *pixel = &dest[i * bpp];

			if (colour_fmt & SCC_RGB) {
				sprite->data[i].r = *pixel++;
				sprite->data[i].g = *pixel++;
				sprite->data[i].b = *pixel++;
			}
			sprite->data[i].a = (colour_fmt & SCC_ALPHA) ? *pixel++ : 0xFF;
			if (colour_fmt & SCC_PAL) {
				switch (sprite_type) {
					case ST_NORMAL: sprite->data[i].m = _palette_remap_grf[file_slot] ? _palmap_w2d[*pixel] : *pixel; break;
					case ST_FONT:   sprite->data[i].m = min(*pixel, 2u); break;
					default:        sprite->data[i].m = *pixel; break;
				}
				/* Magic blue. */
				if (colour_fmt == SCC_PAL && *pixel == 0) sprite->data[i].a = 0x00;
				pixel++;
			}
		}
	}

	return true;
}

uint8 LoadSpriteV1(SpriteLoader::Sprite *sprite, uint8 file_slot, size_t file_pos, SpriteType sprite_type, bool load_32bpp)
{
	/* Check the requested colour depth. */
	if (load_32bpp) return 0;

	/* Open the right file and go to the correct position */
	FioSeekToFile(file_slot, file_pos);

	/* Read the size and type */
	int num = FioReadWord();
	byte type = FioReadByte();

	/* Type 0xFF indicates either a colourmap or some other non-sprite info; we do not handle them here */
	if (type == 0xFF) return 0;

	ZoomLevel zoom_lvl = (sprite_type != ST_MAPGEN) ? ZOOM_LVL_OUT_4X : ZOOM_LVL_NORMAL;

	sprite[zoom_lvl].height = FioReadByte();
	sprite[zoom_lvl].width  = FioReadWord();
	sprite[zoom_lvl].x_offs = FioReadWord();
	sprite[zoom_lvl].y_offs = FioReadWord();

	if (sprite[zoom_lvl].width > INT16_MAX) {
		WarnCorruptSprite(file_slot, file_pos, __LINE__);
		return 0;
	}

	/* 0x02 indicates it is a compressed sprite, so we can't rely on 'num' to be valid.
	 * In case it is uncompressed, the size is 'num' - 8 (header-size). */
	num = (type & 0x02) ? sprite[zoom_lvl].width * sprite[zoom_lvl].height : num - 8;

	if (DecodeSingleSprite(&sprite[zoom_lvl], file_slot, file_pos, sprite_type, num, type, zoom_lvl, SCC_PAL, 1)) return 1 << zoom_lvl;

	return 0;
}

uint8 LoadSpriteV2(SpriteLoader::Sprite *sprite, uint8 file_slot, size_t file_pos, SpriteType sprite_type, bool load_32bpp)
{
	static const ZoomLevel zoom_lvl_map[6] = {ZOOM_LVL_OUT_4X, ZOOM_LVL_NORMAL, ZOOM_LVL_OUT_2X, ZOOM_LVL_OUT_8X, ZOOM_LVL_OUT_16X, ZOOM_LVL_OUT_32X};

	/* Is the sprite not present/stripped in the GRF? */
	if (file_pos == SIZE_MAX) return 0;

	/* Open the right file and go to the correct position */
	FioSeekToFile(file_slot, file_pos);

	uint32 id = FioReadDword();

	uint8 loaded_sprites = 0;
	do {
		int64 num = FioReadDword();
		size_t start_pos = FioGetPos();
		byte type = FioReadByte();

		/* Type 0xFF indicates either a colourmap or some other non-sprite info; we do not handle them here. */
		if (type == 0xFF) return 0;

		byte colour = type & SCC_MASK;
		byte zoom = FioReadByte();

		if (colour != 0 && (load_32bpp ? colour != SCC_PAL : colour == SCC_PAL) && (sprite_type != ST_MAPGEN ? zoom < lengthof(zoom_lvl_map) : zoom == 0)) {
			ZoomLevel zoom_lvl = (sprite_type != ST_MAPGEN) ? zoom_lvl_map[zoom] : ZOOM_LVL_NORMAL;

			if (HasBit(loaded_sprites, zoom_lvl)) {
				/* We already have this zoom level, skip sprite. */
				DEBUG(sprite, 1, "Ignoring duplicate zoom level sprite %u from %s", id, FioGetFilename(file_slot));
				FioSkipBytes(num - 2);
				continue;
			}

			sprite[zoom_lvl].height = FioReadWord();
			sprite[zoom_lvl].width  = FioReadWord();
			sprite[zoom_lvl].x_offs = FioReadWord();
			sprite[zoom_lvl].y_offs = FioReadWord();

			if (sprite[zoom_lvl].width > INT16_MAX || sprite[zoom_lvl].height > INT16_MAX) {
				WarnCorruptSprite(file_slot, file_pos, __LINE__);
				return 0;
			}

			/* Mask out colour information. */
			type = type & ~SCC_MASK;

			/* Convert colour depth to pixel size. */
			int bpp = 0;
			if (colour & SCC_RGB)   bpp += 3; // Has RGB data.
			if (colour & SCC_ALPHA) bpp++;    // Has alpha data.
			if (colour & SCC_PAL)   bpp++;    // Has palette data.

			/* For chunked encoding we store the decompressed size in the file,
			 * otherwise we can calculate it from the image dimensions. */
			uint decomp_size = (type & 0x08) ? FioReadDword() : sprite[zoom_lvl].width * sprite[zoom_lvl].height * bpp;

			bool valid = DecodeSingleSprite(&sprite[zoom_lvl], file_slot, file_pos, sprite_type, decomp_size, type, zoom_lvl, colour, 2);
			if (FioGetPos() != start_pos + num) {
				WarnCorruptSprite(file_slot, file_pos, __LINE__);
				return 0;
			}

			if (valid) SetBit(loaded_sprites, zoom_lvl);
		} else {
			/* Not the wanted zoom level or colour depth, continue searching. */
			FioSkipBytes(num - 2);
		}

	} while (FioReadDword() == id);

	return loaded_sprites;
}

uint8 SpriteLoaderGrf::LoadSprite(SpriteLoader::Sprite *sprite, uint8 file_slot, size_t file_pos, SpriteType sprite_type, bool load_32bpp)
{
	if (this->container_ver >= 2) {
		return LoadSpriteV2(sprite, file_slot, file_pos, sprite_type, load_32bpp);
	} else {
		return LoadSpriteV1(sprite, file_slot, file_pos, sprite_type, load_32bpp);
	}
}
