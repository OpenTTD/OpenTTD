/* $Id$ */

/** @file grf.cpp */

#include "../stdafx.h"
#include "../gfx.h"
#include "../fileio.h"
#include "../debug.h"
#include "grf.hpp"

bool SpriteLoaderGrf::LoadSprite(SpriteLoader::Sprite *sprite, const char *filename, uint8 file_slot, uint32 file_pos)
{
	/* Open the right file and go to the correct position */
	FioSeekToFile(file_slot, file_pos);

	/* Read the size and type */
	int num = FioReadWord();
	byte type = FioReadByte();

	/* Type 0xFF indicates either a colormap or some other non-sprite info; we do not handle them here */
	if (type == 0xFF) return false;

	sprite->height = FioReadByte();
	sprite->width  = FioReadWord();
	sprite->x_offs = FioReadWord();
	sprite->y_offs = FioReadWord();

	/* 0x02 indicates it is a compressed sprite, so we can't rely on 'num' to be valid.
	 *  In case it is uncompressed, the size is 'num' - 8 (header-size). */
	num = (type & 0x02) ? sprite->width * sprite->height : num - 8;

	/* XXX -- We should use a pre-located memory segment for this, malloc/free is pretty expensive */
	byte *dest_orig = MallocT<byte>(num);
	byte *dest = dest_orig;

	/* Read the file, which has some kind of compression */
	while (num > 0) {
		int8 code = FioReadByte();

		if (code >= 0) {
			/* Plain bytes to read */
			int size = (code == 0) ? 0x80 : code;
			num -= size;
			for (; size > 0; size--) {
				*dest = FioReadByte();
				dest++;
			}
		} else {
			/* Copy bytes from earlier in the sprite */
			const uint data_offset = ((code & 7) << 8) | FioReadByte();
			int size = -(code >> 3);
			num -= size;
			for (; size > 0; size--) {
				*dest = *(dest - data_offset);
				dest++;
			}
		}
	}

	assert(num == 0);

	sprite->data = CallocT<SpriteLoader::CommonPixel>(sprite->width * sprite->height);

	/* When there are transparency pixels, this format has an other trick.. decode it */
	if (type & 0x08) {
		for (int y = 0; y < sprite->height; y++) {
			bool last_item = false;
			/* Look up in the header-table where the real data is stored for this row */
			int offset = (dest_orig[y * 2 + 1] << 8) | dest_orig[y * 2];
			/* Go to that row */
			dest = &dest_orig[offset];

			do {
				SpriteLoader::CommonPixel *data;
				/* Read the header:
				 *  0 .. 14  - length
				 *  15       - last_item
				 *  16 .. 31 - transparency bytes */
				last_item  = ((*dest) & 0x80) != 0;
				int length =  (*dest++) & 0x7F;
				int skip   =   *dest++;

				data = &sprite->data[y * sprite->width + skip];

				for (int x = 0; x < length; x++) {
					data->m = *dest;
					dest++;
					data++;
				}
			} while (!last_item);
		}
	} else {
		dest = dest_orig;
		for (int i = 0; i < sprite->width * sprite->height; i++)
			sprite->data[i].m = dest[i];
	}

	/* Make sure to mark all transparent pixels transparent on the alpha channel too */
	for (int i = 0; i < sprite->width * sprite->height; i++)
		if (sprite->data[i].m != 0) sprite->data[i].a = 0xFF;

	free(dest_orig);
	return true;
}
