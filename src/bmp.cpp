/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bmp.cpp Read and write support for bmps. */

#include "stdafx.h"
#include "bmp.h"
#include "core/bitmath_func.hpp"
#include "core/alloc_func.hpp"
#include "core/mem_func.hpp"

#include "safeguards.h"

void BmpInitializeBuffer(BmpBuffer *buffer, FILE *file)
{
	buffer->pos      = -1;
	buffer->file     = file;
	buffer->read     = 0;
	buffer->real_pos = ftell(file);
}

static inline void AdvanceBuffer(BmpBuffer *buffer)
{
	if (buffer->read < 0) return;

	buffer->read = (int)fread(buffer->data, 1, BMP_BUFFER_SIZE, buffer->file);
	buffer->pos  = 0;
}

static inline bool EndOfBuffer(BmpBuffer *buffer)
{
	if (buffer->read < 0) return false;

	if (buffer->pos == buffer->read || buffer->pos < 0) AdvanceBuffer(buffer);
	return buffer->pos == buffer->read;
}

static inline byte ReadByte(BmpBuffer *buffer)
{
	if (buffer->read < 0) return 0;

	if (buffer->pos == buffer->read || buffer->pos < 0) AdvanceBuffer(buffer);
	buffer->real_pos++;
	return buffer->data[buffer->pos++];
}

static inline uint16_t ReadWord(BmpBuffer *buffer)
{
	uint16_t var = ReadByte(buffer);
	return var | (ReadByte(buffer) << 8);
}

static inline uint32_t ReadDword(BmpBuffer *buffer)
{
	uint32_t var = ReadWord(buffer);
	return var | (ReadWord(buffer) << 16);
}

static inline void SkipBytes(BmpBuffer *buffer, int bytes)
{
	int i;
	for (i = 0; i < bytes; i++) ReadByte(buffer);
}

static inline void SetStreamOffset(BmpBuffer *buffer, int offset)
{
	if (fseek(buffer->file, offset, SEEK_SET) < 0) {
		buffer->read = -1;
	}
	buffer->pos = -1;
	buffer->real_pos = offset;
	AdvanceBuffer(buffer);
}

/**
 * Reads a 1 bpp uncompressed bitmap
 * The bitmap is converted to a 8 bpp bitmap
 */
static inline bool BmpRead1(BmpBuffer *buffer, BmpInfo *info, BmpData *data)
{
	uint x, y, i;
	byte pad = GB(4 - info->width / 8, 0, 2);
	byte *pixel_row;
	byte b;
	for (y = info->height; y > 0; y--) {
		x = 0;
		pixel_row = &data->bitmap[(y - 1) * info->width];
		while (x < info->width) {
			if (EndOfBuffer(buffer)) return false; // the file is shorter than expected
			b = ReadByte(buffer);
			for (i = 8; i > 0; i--) {
				if (x < info->width) *pixel_row++ = GB(b, i - 1, 1);
				x++;
			}
		}
		/* Padding for 32 bit align */
		SkipBytes(buffer, pad);
	}
	return true;
}

/**
 * Reads a 4 bpp uncompressed bitmap
 * The bitmap is converted to a 8 bpp bitmap
 */
static inline bool BmpRead4(BmpBuffer *buffer, BmpInfo *info, BmpData *data)
{
	uint x, y;
	byte pad = GB(4 - info->width / 2, 0, 2);
	byte *pixel_row;
	byte b;
	for (y = info->height; y > 0; y--) {
		x = 0;
		pixel_row = &data->bitmap[(y - 1) * info->width];
		while (x < info->width) {
			if (EndOfBuffer(buffer)) return false;  // the file is shorter than expected
			b = ReadByte(buffer);
			*pixel_row++ = GB(b, 4, 4);
			x++;
			if (x < info->width) {
				*pixel_row++ = GB(b, 0, 4);
				x++;
			}
		}
		/* Padding for 32 bit align */
		SkipBytes(buffer, pad);
	}
	return true;
}

/**
 * Reads a 4-bit RLE compressed bitmap
 * The bitmap is converted to a 8 bpp bitmap
 */
static inline bool BmpRead4Rle(BmpBuffer *buffer, BmpInfo *info, BmpData *data)
{
	uint x = 0;
	uint y = info->height - 1;
	byte *pixel = &data->bitmap[y * info->width];
	while (y != 0 || x < info->width) {
		if (EndOfBuffer(buffer)) return false; // the file is shorter than expected

		byte n = ReadByte(buffer);
		byte c = ReadByte(buffer);
		if (n == 0) {
			switch (c) {
				case 0: // end of line
					x = 0;
					if (y == 0) return false;
					pixel = &data->bitmap[--y * info->width];
					break;

				case 1: // end of bitmap
					return true;

				case 2: { // delta
					if (EndOfBuffer(buffer)) return false;
					byte dx = ReadByte(buffer);
					byte dy = ReadByte(buffer);

					/* Check for over- and underflow. */
					if (x + dx >= info->width || x + dx < x || dy > y) return false;

					x += dx;
					y -= dy;
					pixel = &data->bitmap[y * info->width + x];
					break;
				}

				default: { // uncompressed
					uint i = 0;
					while (i++ < c) {
						if (EndOfBuffer(buffer) || x >= info->width) return false;
						byte b = ReadByte(buffer);
						*pixel++ = GB(b, 4, 4);
						x++;
						if (i++ < c) {
							if (x >= info->width) return false;
							*pixel++ = GB(b, 0, 4);
							x++;
						}
					}
					/* Padding for 16 bit align */
					SkipBytes(buffer, ((c + 1) / 2) % 2);
					break;
				}
			}
		} else {
			/* Apparently it is common to encounter BMPs where the count of
			 * pixels to be written is higher than the remaining line width.
			 * Ignore the superfluous pixels instead of reporting an error. */
			uint i = 0;
			while (x < info->width && i++ < n) {
				*pixel++ = GB(c, 4, 4);
				x++;
				if (x < info->width && i++ < n) {
					*pixel++ = GB(c, 0, 4);
					x++;
				}
			}
		}
	}
	return true;
}

/**
 * Reads a 8 bpp bitmap
 */
static inline bool BmpRead8(BmpBuffer *buffer, BmpInfo *info, BmpData *data)
{
	uint i;
	uint y;
	byte pad = GB(4 - info->width, 0, 2);
	byte *pixel;
	for (y = info->height; y > 0; y--) {
		if (EndOfBuffer(buffer)) return false; // the file is shorter than expected
		pixel = &data->bitmap[(y - 1) * info->width];
		for (i = 0; i < info->width; i++) *pixel++ = ReadByte(buffer);
		/* Padding for 32 bit align */
		SkipBytes(buffer, pad);
	}
	return true;
}

/**
 * Reads a 8-bit RLE compressed bpp bitmap
 */
static inline bool BmpRead8Rle(BmpBuffer *buffer, BmpInfo *info, BmpData *data)
{
	uint x = 0;
	uint y = info->height - 1;
	byte *pixel = &data->bitmap[y * info->width];
	while (y != 0 || x < info->width) {
		if (EndOfBuffer(buffer)) return false; // the file is shorter than expected

		byte n = ReadByte(buffer);
		byte c = ReadByte(buffer);
		if (n == 0) {
			switch (c) {
				case 0: // end of line
					x = 0;
					if (y == 0) return false;
					pixel = &data->bitmap[--y * info->width];
					break;

				case 1: // end of bitmap
					return true;

				case 2: { // delta
					if (EndOfBuffer(buffer)) return false;
					byte dx = ReadByte(buffer);
					byte dy = ReadByte(buffer);

					/* Check for over- and underflow. */
					if (x + dx >= info->width || x + dx < x || dy > y) return false;

					x += dx;
					y -= dy;
					pixel = &data->bitmap[y * info->width + x];
					break;
				}

				default: { // uncompressed
					for (uint i = 0; i < c; i++) {
						if (EndOfBuffer(buffer) || x >= info->width) return false;
						*pixel++ = ReadByte(buffer);
						x++;
					}
					/* Padding for 16 bit align */
					SkipBytes(buffer, c % 2);
					break;
				}
			}
		} else {
			/* Apparently it is common to encounter BMPs where the count of
			 * pixels to be written is higher than the remaining line width.
			 * Ignore the superfluous pixels instead of reporting an error. */
			for (uint i = 0; x < info->width && i < n; i++) {
				*pixel++ = c;
				x++;
			}
		}
	}
	return true;
}

/**
 * Reads a 24 bpp uncompressed bitmap
 */
static inline bool BmpRead24(BmpBuffer *buffer, BmpInfo *info, BmpData *data)
{
	uint x, y;
	byte pad = GB(4 - info->width * 3, 0, 2);
	byte *pixel_row;
	for (y = info->height; y > 0; y--) {
		pixel_row = &data->bitmap[(y - 1) * info->width * 3];
		for (x = 0; x < info->width; x++) {
			if (EndOfBuffer(buffer)) return false; // the file is shorter than expected
			*(pixel_row + 2) = ReadByte(buffer); // green
			*(pixel_row + 1) = ReadByte(buffer); // blue
			*pixel_row       = ReadByte(buffer); // red
			pixel_row += 3;
		}
		/* Padding for 32 bit align */
		SkipBytes(buffer, pad);
	}
	return true;
}

/*
 * Reads bitmap headers, and palette (if any)
 */
bool BmpReadHeader(BmpBuffer *buffer, BmpInfo *info, BmpData *data)
{
	uint32_t header_size;
	assert(info != nullptr);
	MemSetT(info, 0);

	/* Reading BMP header */
	if (ReadWord(buffer) != 0x4D42) return false; // signature should be 'BM'
	SkipBytes(buffer, 8); // skip file size and reserved
	info->offset = ReadDword(buffer);

	/* Reading info header */
	header_size = ReadDword(buffer);
	if (header_size < 12) return false; // info header should be at least 12 bytes long

	info->os2_bmp = (header_size == 12); // OS/2 1.x or windows 2.x info header is 12 bytes long

	if (info->os2_bmp) {
		info->width = ReadWord(buffer);
		info->height = ReadWord(buffer);
		header_size -= 8;
	} else {
		info->width = ReadDword(buffer);
		info->height = ReadDword(buffer);
		header_size -= 12;
	}

	if (ReadWord(buffer) != 1) return false; // BMP can have only 1 plane

	info->bpp = ReadWord(buffer);
	if (info->bpp != 1 && info->bpp != 4 && info->bpp != 8 && info->bpp != 24) {
		/* Only 1 bpp, 4 bpp, 8bpp and 24 bpp bitmaps are supported */
		return false;
	}

	/* Reads compression method if available in info header*/
	if ((header_size -= 4) >= 4) {
		info->compression = ReadDword(buffer);
		header_size -= 4;
	}

	/* Only 4-bit and 8-bit rle compression is supported */
	if (info->compression > 2 || (info->compression > 0 && !(info->bpp == 4 || info->bpp == 8))) return false;

	if (info->bpp <= 8) {
		uint i;

		/* Reads number of colours if available in info header */
		if (header_size >= 16) {
			SkipBytes(buffer, 12);                  // skip image size and resolution
			info->palette_size = ReadDword(buffer); // number of colours in palette
			SkipBytes(buffer, header_size - 16);    // skip the end of info header
		}

		uint maximum_palette_size = 1U << info->bpp;
		if (info->palette_size == 0) info->palette_size = maximum_palette_size;

		/* More palette colours than palette indices is not supported. */
		if (info->palette_size > maximum_palette_size) return false;

		data->palette = CallocT<Colour>(info->palette_size);

		for (i = 0; i < info->palette_size; i++) {
			data->palette[i].b = ReadByte(buffer);
			data->palette[i].g = ReadByte(buffer);
			data->palette[i].r = ReadByte(buffer);
			if (!info->os2_bmp) SkipBytes(buffer, 1); // unused
		}
	}

	return buffer->real_pos <= info->offset;
}

/*
 * Reads the bitmap
 * 1 bpp and 4 bpp bitmaps are converted to 8 bpp bitmaps
 */
bool BmpReadBitmap(BmpBuffer *buffer, BmpInfo *info, BmpData *data)
{
	assert(info != nullptr && data != nullptr);

	data->bitmap = CallocT<byte>(static_cast<size_t>(info->width) * info->height * ((info->bpp == 24) ? 3 : 1));

	/* Load image */
	SetStreamOffset(buffer, info->offset);
	switch (info->compression) {
	case 0: // no compression
		switch (info->bpp) {
		case 1:  return BmpRead1(buffer, info, data);
		case 4:  return BmpRead4(buffer, info, data);
		case 8:  return BmpRead8(buffer, info, data);
		case 24: return BmpRead24(buffer, info, data);
		default: NOT_REACHED();
		}
	case 1:  return BmpRead8Rle(buffer, info, data); // 8-bit RLE compression
	case 2:  return BmpRead4Rle(buffer, info, data); // 4-bit RLE compression
	default: NOT_REACHED();
	}
}

void BmpDestroyData(BmpData *data)
{
	assert(data != nullptr);
	free(data->palette);
	free(data->bitmap);
}
