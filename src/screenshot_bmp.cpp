/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file screenshot_bmp.cpp BMP screenshot provider. */

#include "stdafx.h"
#include "core/endian_func.hpp"
#include "core/math_func.hpp"
#include "fileio_func.h"
#include "screenshot_type.h"

#include "safeguards.h"

/** BMP File Header (stored in little endian) */
PACK(struct BitmapFileHeader {
	uint16_t type;
	uint32_t size;
	uint32_t reserved;
	uint32_t off_bits;
});
static_assert(sizeof(BitmapFileHeader) == 14);

/** BMP Info Header (stored in little endian) */
struct BitmapInfoHeader {
	uint32_t size;
	int32_t width, height;
	uint16_t planes, bitcount;
	uint32_t compression, sizeimage, xpels, ypels, clrused, clrimp;
};
static_assert(sizeof(BitmapInfoHeader) == 40);

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

		/* Setup the file header */
		BitmapFileHeader bfh;
		bfh.type = TO_LE16('MB');
		bfh.size = TO_LE32(sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader) + pal_size + static_cast<size_t>(bytewidth) * h);
		bfh.reserved = 0;
		bfh.off_bits = TO_LE32(sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader) + pal_size);

		/* Setup the info header */
		BitmapInfoHeader bih;
		bih.size = TO_LE32(sizeof(BitmapInfoHeader));
		bih.width = TO_LE32(w);
		bih.height = TO_LE32(h);
		bih.planes = TO_LE16(1);
		bih.bitcount = TO_LE16(bpp * 8);
		bih.compression = 0;
		bih.sizeimage = 0;
		bih.xpels = 0;
		bih.ypels = 0;
		bih.clrused = 0;
		bih.clrimp = 0;

		/* Write file header and info header */
		if (fwrite(&bfh, sizeof(bfh), 1, f) != 1 || fwrite(&bih, sizeof(bih), 1, f) != 1) {
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
