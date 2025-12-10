/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file bmp.cpp Read and write support for bmps. */

#include "stdafx.h"
#include "random_access_file_type.h"
#include "bmp.h"
#include "core/bitmath_func.hpp"

#include "safeguards.h"

/**
 * Reads a 1 bpp uncompressed bitmap
 * The bitmap is converted to a 8 bpp bitmap
 */
static inline bool BmpRead1(RandomAccessFile &file, BmpInfo &info, BmpData &data)
{
	uint8_t pad = GB(4 - info.width / 8, 0, 2);
	for (uint y = info.height; y > 0; y--) {
		uint x = 0;
		uint8_t *pixel_row = &data.bitmap[(y - 1) * static_cast<size_t>(info.width)];
		while (x < info.width) {
			if (file.AtEndOfFile()) return false; // the file is shorter than expected
			uint8_t b = file.ReadByte();
			for (uint i = 8; i > 0; i--) {
				if (x < info.width) *pixel_row++ = GB(b, i - 1, 1);
				x++;
			}
		}
		/* Padding for 32 bit align */
		file.SkipBytes(pad);
	}
	return true;
}

/**
 * Reads a 4 bpp uncompressed bitmap
 * The bitmap is converted to a 8 bpp bitmap
 */
static inline bool BmpRead4(RandomAccessFile &file, BmpInfo &info, BmpData &data)
{
	uint8_t pad = GB(4 - info.width / 2, 0, 2);
	for (uint y = info.height; y > 0; y--) {
		uint x = 0;
		uint8_t *pixel_row = &data.bitmap[(y - 1) * static_cast<size_t>(info.width)];
		while (x < info.width) {
			if (file.AtEndOfFile()) return false;  // the file is shorter than expected
			uint8_t b = file.ReadByte();
			*pixel_row++ = GB(b, 4, 4);
			x++;
			if (x < info.width) {
				*pixel_row++ = GB(b, 0, 4);
				x++;
			}
		}
		/* Padding for 32 bit align */
		file.SkipBytes(pad);
	}
	return true;
}

/**
 * Reads a 4-bit RLE compressed bitmap
 * The bitmap is converted to a 8 bpp bitmap
 */
static inline bool BmpRead4Rle(RandomAccessFile &file, BmpInfo &info, BmpData &data)
{
	uint x = 0;
	uint y = info.height - 1;
	uint8_t *pixel = &data.bitmap[y * static_cast<size_t>(info.width)];
	while (y != 0 || x < info.width) {
		if (file.AtEndOfFile()) return false; // the file is shorter than expected

		uint8_t n = file.ReadByte();
		uint8_t c = file.ReadByte();
		if (n == 0) {
			switch (c) {
				case 0: // end of line
					x = 0;
					if (y == 0) return false;
					pixel = &data.bitmap[--y * static_cast<size_t>(info.width)];
					break;

				case 1: // end of bitmap
					return true;

				case 2: { // delta
					if (file.AtEndOfFile()) return false;
					uint8_t dx = file.ReadByte();
					uint8_t dy = file.ReadByte();

					/* Check for over- and underflow. */
					if (x + dx >= info.width || x + dx < x || dy > y) return false;

					x += dx;
					y -= dy;
					pixel = &data.bitmap[y * info.width + x];
					break;
				}

				default: { // uncompressed
					uint i = 0;
					while (i++ < c) {
						if (file.AtEndOfFile() || x >= info.width) return false;
						uint8_t b = file.ReadByte();
						*pixel++ = GB(b, 4, 4);
						x++;
						if (i++ < c) {
							if (x >= info.width) return false;
							*pixel++ = GB(b, 0, 4);
							x++;
						}
					}
					/* Padding for 16 bit align */
					file.SkipBytes(((c + 1) / 2) % 2);
					break;
				}
			}
		} else {
			/* Apparently it is common to encounter BMPs where the count of
			 * pixels to be written is higher than the remaining line width.
			 * Ignore the superfluous pixels instead of reporting an error. */
			uint i = 0;
			while (x < info.width && i++ < n) {
				*pixel++ = GB(c, 4, 4);
				x++;
				if (x < info.width && i++ < n) {
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
static inline bool BmpRead8(RandomAccessFile &file, BmpInfo &info, BmpData &data)
{
	uint8_t pad = GB(4 - info.width, 0, 2);
	for (uint y = info.height; y > 0; y--) {
		if (file.AtEndOfFile()) return false; // the file is shorter than expected
		uint8_t *pixel = &data.bitmap[(y - 1) * static_cast<size_t>(info.width)];
		for (uint i = 0; i < info.width; i++) *pixel++ = file.ReadByte();
		/* Padding for 32 bit align */
		file.SkipBytes(pad);
	}
	return true;
}

/**
 * Reads a 8-bit RLE compressed bpp bitmap
 */
static inline bool BmpRead8Rle(RandomAccessFile &file, BmpInfo &info, BmpData &data)
{
	uint x = 0;
	uint y = info.height - 1;
	uint8_t *pixel = &data.bitmap[y * static_cast<size_t>(info.width)];
	while (y != 0 || x < info.width) {
		if (file.AtEndOfFile()) return false; // the file is shorter than expected

		uint8_t n = file.ReadByte();
		uint8_t c = file.ReadByte();
		if (n == 0) {
			switch (c) {
				case 0: // end of line
					x = 0;
					if (y == 0) return false;
					pixel = &data.bitmap[--y * static_cast<size_t>(info.width)];
					break;

				case 1: // end of bitmap
					return true;

				case 2: { // delta
					if (file.AtEndOfFile()) return false;
					uint8_t dx = file.ReadByte();
					uint8_t dy = file.ReadByte();

					/* Check for over- and underflow. */
					if (x + dx >= info.width || x + dx < x || dy > y) return false;

					x += dx;
					y -= dy;
					pixel = &data.bitmap[y * static_cast<size_t>(info.width) + x];
					break;
				}

				default: { // uncompressed
					for (uint i = 0; i < c; i++) {
						if (file.AtEndOfFile() || x >= info.width) return false;
						*pixel++ = file.ReadByte();
						x++;
					}
					/* Padding for 16 bit align */
					file.SkipBytes(c % 2);
					break;
				}
			}
		} else {
			/* Apparently it is common to encounter BMPs where the count of
			 * pixels to be written is higher than the remaining line width.
			 * Ignore the superfluous pixels instead of reporting an error. */
			for (uint i = 0; x < info.width && i < n; i++) {
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
static inline bool BmpRead24(RandomAccessFile &file, BmpInfo &info, BmpData &data)
{
	uint8_t pad = GB(4 - info.width * 3, 0, 2);
	for (uint y = info.height; y > 0; --y) {
		uint8_t *pixel_row = &data.bitmap[(y - 1) * static_cast<size_t>(info.width) * 3];
		for (uint x = 0; x < info.width; ++x) {
			if (file.AtEndOfFile()) return false; // the file is shorter than expected
			*(pixel_row + 2) = file.ReadByte(); // green
			*(pixel_row + 1) = file.ReadByte(); // blue
			*pixel_row       = file.ReadByte(); // red
			pixel_row += 3;
		}
		/* Padding for 32 bit align */
		file.SkipBytes(pad);
	}
	return true;
}

/*
 * Reads bitmap headers, and palette (if any)
 */
bool BmpReadHeader(RandomAccessFile &file, BmpInfo &info, BmpData &data)
{
	info = {};

	/* Reading BMP header */
	if (file.ReadWord() != 0x4D42) return false; // signature should be 'BM'
	file.SkipBytes(8); // skip file size and reserved
	info.offset = file.ReadDword() + file.GetStartPos();

	/* Reading info header */
	uint32_t header_size = file.ReadDword();
	if (header_size < 12) return false; // info header should be at least 12 bytes long

	info.os2_bmp = (header_size == 12); // OS/2 1.x or windows 2.x info header is 12 bytes long

	if (info.os2_bmp) {
		info.width = file.ReadWord();
		info.height = file.ReadWord();
		header_size -= 8;
	} else {
		info.width = file.ReadDword();
		info.height = file.ReadDword();
		header_size -= 12;
	}

	if (file.ReadWord() != 1) return false; // BMP can have only 1 plane

	info.bpp = file.ReadWord();
	if (info.bpp != 1 && info.bpp != 4 && info.bpp != 8 && info.bpp != 24) {
		/* Only 1 bpp, 4 bpp, 8bpp and 24 bpp bitmaps are supported */
		return false;
	}

	/* Reads compression method if available in info header*/
	if ((header_size -= 4) >= 4) {
		info.compression = file.ReadDword();
		header_size -= 4;
	}

	/* Only 4-bit and 8-bit rle compression is supported */
	if (info.compression > 2 || (info.compression > 0 && !(info.bpp == 4 || info.bpp == 8))) return false;

	if (info.bpp <= 8) {
		/* Reads number of colours if available in info header */
		if (header_size >= 16) {
			file.SkipBytes(12);                  // skip image size and resolution
			info.palette_size = file.ReadDword(); // number of colours in palette
			file.SkipBytes(header_size - 16);    // skip the end of info header
		}

		uint maximum_palette_size = 1U << info.bpp;
		if (info.palette_size == 0) info.palette_size = maximum_palette_size;

		/* More palette colours than palette indices is not supported. */
		if (info.palette_size > maximum_palette_size) return false;

		data.palette.resize(info.palette_size);

		for (auto &colour : data.palette) {
			colour.b = file.ReadByte();
			colour.g = file.ReadByte();
			colour.r = file.ReadByte();
			if (!info.os2_bmp) file.SkipBytes(1); // unused
		}
	}

	return file.GetPos() <= info.offset;
}

/*
 * Reads the bitmap
 * 1 bpp and 4 bpp bitmaps are converted to 8 bpp bitmaps
 */
bool BmpReadBitmap(RandomAccessFile &file, BmpInfo &info, BmpData &data)
{
	data.bitmap.resize(static_cast<size_t>(info.width) * info.height * ((info.bpp == 24) ? 3 : 1));

	/* Load image */
	file.SeekTo(info.offset, SEEK_SET);
	switch (info.compression) {
		case 0: // no compression
			switch (info.bpp) {
				case 1: return BmpRead1(file, info, data);
				case 4: return BmpRead4(file, info, data);
				case 8: return BmpRead8(file, info, data);
				case 24: return BmpRead24(file, info, data);
				default: NOT_REACHED();
			}
			break;

		case 1: return BmpRead8Rle(file, info, data); // 8-bit RLE compression
		case 2: return BmpRead4Rle(file, info, data); // 4-bit RLE compression
		default: NOT_REACHED();
	}
}
