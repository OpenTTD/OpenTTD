/* $Id$ */

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

void BmpInitializeBuffer(BmpBuffer *buffer, FILE *file)
{
	buffer->pos      = -1;
	buffer->file     = file;
	buffer->read     = 0;
	buffer->real_pos = ftell(file);
}

static inline void AdvanceBuffer(BmpBuffer *buffer)
{
	buffer->read = (int)fread(buffer->data, 1, BMP_BUFFER_SIZE, buffer->file);
	buffer->pos  = 0;
}

static inline bool EndOfBuffer(BmpBuffer *buffer)
{
	if (buffer->pos == buffer->read || buffer->pos < 0) AdvanceBuffer(buffer);
	return buffer->pos == buffer->read;
}

static inline byte ReadByte(BmpBuffer *buffer)
{
	if (buffer->pos == buffer->read || buffer->pos < 0) AdvanceBuffer(buffer);
	buffer->real_pos++;
	return buffer->data[buffer->pos++];
}

static inline uint16 ReadWord(BmpBuffer *buffer)
{
	uint16 var = ReadByte(buffer);
	return var | (ReadByte(buffer) << 8);
}

static inline uint32 ReadDword(BmpBuffer *buffer)
{
	uint32 var = ReadWord(buffer);
	return var | (ReadWord(buffer) << 16);
}

static inline void SkipBytes(BmpBuffer *buffer, int bytes)
{
	int i;
	for (i = 0; i < bytes; i++) ReadByte(buffer);
}

static inline void SetStreamOffset(BmpBuffer *buffer, int offset)
{
	fseek(buffer->file, offset, SEEK_SET);
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
	uint i;
	uint x = 0;
	uint y = info->height - 1;
	byte n, c, b;
	byte *pixel = &data->bitmap[y * info->width];
	while (y != 0 || x < info->width) {
		if (EndOfBuffer(buffer)) return false; // the file is shorter than expected
		n = ReadByte(buffer);
		c = ReadByte(buffer);
		if (n == 0) {
			switch (c) {
			case 0: // end of line
				x = 0;
				pixel = &data->bitmap[--y * info->width];
				break;
			case 1: // end of bitmap
				x = info->width;
				y = 0;
				pixel = NULL;
				break;
			case 2: // delta
				x += ReadByte(buffer);
				i = ReadByte(buffer);
				if (x >= info->width || (y == 0 && i > 0)) return false;
				y -= i;
				pixel = &data->bitmap[y * info->width + x];
				break;
			default: // uncompressed
				i = 0;
				while (i++ < c) {
					if (EndOfBuffer(buffer) || x >= info->width) return false;
					b = ReadByte(buffer);
					*pixel++ = GB(b, 4, 4);
					x++;
					if (x < info->width && i++ < c) {
						*pixel++ = GB(b, 0, 4);
						x++;
					}
				}
				/* Padding for 16 bit align */
				SkipBytes(buffer, ((c + 1) / 2) % 2);
				break;
			}
		} else {
			i = 0;
			while (i++ < n) {
				if (EndOfBuffer(buffer) || x >= info->width) return false;
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
	uint i;
	uint x = 0;
	uint y = info->height - 1;
	byte n, c;
	byte *pixel = &data->bitmap[y * info->width];
	while (y != 0 || x < info->width) {
		if (EndOfBuffer(buffer)) return false; // the file is shorter than expected
		n = ReadByte(buffer);
		c = ReadByte(buffer);
		if (n == 0) {
			switch (c) {
			case 0: // end of line
				x = 0;
				pixel = &data->bitmap[--y * info->width];
				break;
			case 1: // end of bitmap
				x = info->width;
				y = 0;
				pixel = NULL;
				break;
			case 2: // delta
				x += ReadByte(buffer);
				i = ReadByte(buffer);
				if (x >= info->width || (y == 0 && i > 0)) return false;
				y -= i;
				pixel = &data->bitmap[y * info->width + x];
				break;
			default: // uncompressed
				if ((x += c) > info->width) return false;
				for (i = 0; i < c; i++) *pixel++ = ReadByte(buffer);
				/* Padding for 16 bit align */
				SkipBytes(buffer, c % 2);
				break;
			}
		} else {
			for (i = 0; i < n; i++) {
				if (x >= info->width) return false;
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
	uint32 header_size;
	assert(info != NULL);
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
		if (info->palette_size == 0) info->palette_size = 1 << info->bpp;

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
	assert(info != NULL && data != NULL);

	data->bitmap = CallocT<byte>(info->width * info->height * ((info->bpp == 24) ? 3 : 1));

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
	assert(data != NULL);
	free(data->palette);
	free(data->bitmap);
}
