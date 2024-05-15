/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bmp.h Read and write support for bmps. */

#ifndef BMP_H
#define BMP_H

#include "gfx_type.h"
#include "random_access_file_type.h"

struct BmpInfo {
	size_t offset; ///< offset of bitmap data from .bmp file beginning
	uint32_t width;        ///< bitmap width
	uint32_t height;       ///< bitmap height
	bool os2_bmp;        ///< true if OS/2 1.x or windows 2.x bitmap
	uint16_t bpp;          ///< bits per pixel
	uint32_t compression;  ///< compression method (0 = none, 1 = 8-bit RLE, 2 = 4-bit RLE)
	uint32_t palette_size; ///< number of colours in palette
};

struct BmpData {
	std::vector<Colour> palette;
	std::vector<uint8_t> bitmap;
};

bool BmpReadHeader(RandomAccessFile &file, BmpInfo &info, BmpData &data);
bool BmpReadBitmap(RandomAccessFile &file, BmpInfo &info, BmpData &data);

#endif /* BMP_H */
