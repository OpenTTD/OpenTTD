/* $Id$ */

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
#include "../gui.h"
#include "../core/math_func.hpp"
#include "grf.hpp"

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

bool SpriteLoaderGrf::LoadSprite(SpriteLoader::Sprite *sprite, uint8 file_slot, size_t file_pos, SpriteType sprite_type)
{
	/* Open the right file and go to the correct position */
	FioSeekToFile(file_slot, file_pos);

	/* Read the size and type */
	int num = FioReadWord();
	byte type = FioReadByte();

	/* Type 0xFF indicates either a colourmap or some other non-sprite info; we do not handle them here */
	if (type == 0xFF) return false;

	sprite->height = FioReadByte();
	sprite->width  = FioReadWord();
	sprite->x_offs = FioReadWord();
	sprite->y_offs = FioReadWord();

	/* 0x02 indicates it is a compressed sprite, so we can't rely on 'num' to be valid.
	 *  In case it is uncompressed, the size is 'num' - 8 (header-size). */
	num = (type & 0x02) ? sprite->width * sprite->height : num - 8;

	byte *dest_orig = AllocaM(byte, num);
	byte *dest = dest_orig;
	const int dest_size = num;

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
			if (dest - data_offset < dest_orig) return WarnCorruptSprite(file_slot, file_pos, __LINE__);
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

	sprite->AllocateData(sprite->width * sprite->height);

	/* When there are transparency pixels, this format has another trick.. decode it */
	if (type & 0x08) {
		for (int y = 0; y < sprite->height; y++) {
			bool last_item = false;
			/* Look up in the header-table where the real data is stored for this row */
			int offset = (dest_orig[y * 2 + 1] << 8) | dest_orig[y * 2];

			/* Go to that row */
			dest = dest_orig + offset;

			do {
				if (dest + 2 > dest_orig + dest_size) {
					free(sprite->data);
					return WarnCorruptSprite(file_slot, file_pos, __LINE__);
				}

				SpriteLoader::CommonPixel *data;
				/* Read the header:
				 *  0 .. 14  - length
				 *  15       - last_item
				 *  16 .. 31 - transparency bytes */
				last_item  = ((*dest) & 0x80) != 0;
				int length =  (*dest++) & 0x7F;
				int skip   =   *dest++;

				data = &sprite->data[y * sprite->width + skip];

				if (skip + length > sprite->width || dest + length > dest_orig + dest_size) {
					free(sprite->data);
					return WarnCorruptSprite(file_slot, file_pos, __LINE__);
				}

				for (int x = 0; x < length; x++) {
					switch (sprite_type) {
						case ST_NORMAL: data->m = _palette_remap_grf[file_slot] ? _palette_remap[*dest] : *dest; break;
						case ST_FONT:   data->m = min(*dest, 2u); break;
						default:        data->m = *dest; break;
					}
					dest++;
					data++;
				}
			} while (!last_item);
		}
	} else {
		if (dest_size < sprite->width * sprite->height) {
			free(sprite->data);
			return WarnCorruptSprite(file_slot, file_pos, __LINE__);
		}

		if (dest_size > sprite->width * sprite->height) {
			static byte warning_level = 0;
			DEBUG(sprite, warning_level, "Ignoring %i unused extra bytes from the sprite from %s at position %i", dest_size - sprite->width * sprite->height, FioGetFilename(file_slot), (int)file_pos);
			warning_level = 6;
		}

		dest = dest_orig;

		for (int i = 0; i < sprite->width * sprite->height; i++) {
			switch (sprite_type) {
				case ST_NORMAL: sprite->data[i].m = _palette_remap_grf[file_slot] ? _palette_remap[dest[i]] : dest[i]; break;
				case ST_FONT:   sprite->data[i].m = min(dest[i], 2u); break;
				default:        sprite->data[i].m = dest[i]; break;
			}
		}
	}

	/* Make sure to mark all transparent pixels transparent on the alpha channel too */
	for (int i = 0; i < sprite->width * sprite->height; i++) {
		if (sprite->data[i].m != 0) sprite->data[i].a = 0xFF;
	}

	return true;
}
