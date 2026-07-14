/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file screenshot_bmp.cpp BMP screenshot provider. */

#include "stdafx.h"
#include "core/math_func.hpp"
#include "core/string_builder.hpp"
#include "fileio_func.h"
#include "screenshot_type.h"

#include "safeguards.h"

constexpr size_t BITMAP_FILE_HEADER_SIZE = 14; ///< The size of a bitmap file header.
constexpr size_t BITMAP_INFO_HEADER_SIZE = 40; ///< The size of a bitmap info header.

/** Format of palette data in BMP header */
struct RgbQuad {
	uint8_t blue, green, red, reserved;
};
static_assert(sizeof(RgbQuad) == 4);

class ScreenshotProvider_Bmp : public ScreenshotProvider {
public:
	ScreenshotProvider_Bmp() : ScreenshotProvider("bmp", "BMP", 10) {}

	bool MakeImage(std::string_view name, const ScreenshotCallback &callb, uint w, uint h, int pixelformat, const Colour *palette) const override
	{
		uint bpp; // bytes per pixel
		switch (pixelformat) {
			case 8:  bpp = 1; break;
			/* 32bpp mode is saved as 24bpp BMP */
			case 32: bpp = 3; break;
			/* Only implemented for 8bit and 32bit images so far */
			default: return false;
		}

		auto of = FileHandle::Open(name, "wb");
		if (!of.has_value()) return false;
		auto &f = *of;

		/* Each scanline must be aligned on a 32bit boundary */
		uint bytewidth = Align(w * bpp, 4); // bytes per line in file

		/* Size of palette. Only present for 8bpp mode */
		uint pal_size = pixelformat == 8 ? sizeof(RgbQuad) * 256 : 0;

		std::string header;
		StringBuilder sb(header);
		sb.Put("BM"); // header field
		sb.PutUint32LE(BITMAP_FILE_HEADER_SIZE + BITMAP_INFO_HEADER_SIZE + pal_size + static_cast<size_t>(bytewidth) * h); // full image size
		sb.PutUint16LE(0); // reserved 1
		sb.PutUint16LE(0); // reserved 2
		sb.PutUint32LE(BITMAP_FILE_HEADER_SIZE + BITMAP_INFO_HEADER_SIZE + pal_size); // offset of bitmap image data

		sb.PutUint32LE(BITMAP_INFO_HEADER_SIZE); // size of info header
		sb.PutUint32LE(w); // width of the image
		sb.PutUint32LE(h); // height of the image
		sb.PutUint16LE(1); // number of planes
		sb.PutUint16LE(bpp * 8); // bits per pixel
		sb.PutUint32LE(0); // compression
		sb.PutUint32LE(0); // size of raw image data, dummy of 0 is allowed
		sb.PutUint32LE(0); // vertical resolution in pixels per meter
		sb.PutUint32LE(0); // horizontal resolution in pixels per meter
		sb.PutUint32LE(0); // number of colours in the palette, 0 is allowed
		sb.PutUint32LE(0); // number of important colours, 0 is all colours are important
		assert(header.size() == BITMAP_FILE_HEADER_SIZE + BITMAP_INFO_HEADER_SIZE);

		/* Write headers */
		if (fwrite(header.data(), header.size(), 1, f) != 1) {
			return false;
		}

		if (pixelformat == 8) {
			/* Convert the palette to the windows format */
			RgbQuad rq[256];
			for (uint i = 0; i < 256; i++) {
				rq[i].red   = palette[i].r;
				rq[i].green = palette[i].g;
				rq[i].blue  = palette[i].b;
				rq[i].reserved = 0;
			}
			/* Write the palette */
			if (fwrite(rq, sizeof(rq), 1, f) != 1) {
				return false;
			}
		}

		/* Try to use 64k of memory, store between 16 and 128 lines */
		uint maxlines = Clamp(65536 / (w * pixelformat / 8), 16, 128); // number of lines per iteration

		std::vector<uint8_t> buff(maxlines * w * pixelformat / 8); // buffer which is rendered to
		std::vector<uint8_t> line(bytewidth); // one line, stored to file

		/* Start at the bottom, since bitmaps are stored bottom up */
		do {
			uint n = std::min(h, maxlines);
			h -= n;

			/* Render the pixels */
			callb(buff.data(), h, w, n);

			/* Write each line */
			while (n-- != 0) {
				if (pixelformat == 8) {
					/* Move to 'line', leave last few pixels in line zeroed */
					std::copy_n(buff.data() + n * w, w, line.data());
				} else {
					/* Convert from 'native' 32bpp to BMP-like 24bpp.
					 * Works for both big and little endian machines */
					Colour *src = ((Colour *)buff.data()) + n * w;
					uint8_t *dst = line.data();
					for (uint i = 0; i < w; i++) {
						dst[i * 3    ] = src[i].b;
						dst[i * 3 + 1] = src[i].g;
						dst[i * 3 + 2] = src[i].r;
					}
				}
				/* Write to file */
				if (fwrite(line.data(), bytewidth, 1, f) != 1) {
					return false;
				}
			}
		} while (h != 0);


		return true;
	}

private:
	static ScreenshotProvider_Bmp instance;
};

/* static */ ScreenshotProvider_Bmp ScreenshotProvider_Bmp::instance{};
