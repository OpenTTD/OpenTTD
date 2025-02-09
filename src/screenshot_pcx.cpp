/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file screenshot_pcx.cpp PCX screenshots provider. */

#include "stdafx.h"
#include "core/endian_func.hpp"
#include "core/math_func.hpp"
#include "debug.h"
#include "fileio_func.h"
#include "screenshot_type.h"

#include "safeguards.h"


/** Definition of a PCX file header. */
struct PcxHeader {
	uint8_t manufacturer;
	uint8_t version;
	uint8_t rle;
	uint8_t bpp;
	uint32_t unused;
	uint16_t xmax, ymax;
	uint16_t hdpi, vdpi;
	uint8_t pal_small[16 * 3];
	uint8_t reserved;
	uint8_t planes;
	uint16_t pitch;
	uint16_t cpal;
	uint16_t width;
	uint16_t height;
	uint8_t filler[54];
};
static_assert(sizeof(PcxHeader) == 128);

class ScreenshotProvider_Pcx : public ScreenshotProvider {
public:
	ScreenshotProvider_Pcx() : ScreenshotProvider("pcx", "PCX", 20) {}

	bool MakeImage(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const Colour *palette) override
	{
		uint maxlines;
		uint y;
		PcxHeader pcx;
		bool success;

		if (pixelformat == 32) {
			Debug(misc, 0, "Can't convert a 32bpp screenshot to PCX format. Please pick another format.");
			return false;
		}
		if (pixelformat != 8 || w == 0) return false;

		auto of = FileHandle::Open(name, "wb");
		if (!of.has_value()) return false;
		auto &f = *of;

		memset(&pcx, 0, sizeof(pcx));

		/* setup pcx header */
		pcx.manufacturer = 10;
		pcx.version = 5;
		pcx.rle = 1;
		pcx.bpp = 8;
		pcx.xmax = TO_LE16(w - 1);
		pcx.ymax = TO_LE16(h - 1);
		pcx.hdpi = TO_LE16(320);
		pcx.vdpi = TO_LE16(320);

		pcx.planes = 1;
		pcx.cpal = TO_LE16(1);
		pcx.width = pcx.pitch = TO_LE16(w);
		pcx.height = TO_LE16(h);

		/* write pcx header */
		if (fwrite(&pcx, sizeof(pcx), 1, f) != 1) {
			return false;
		}

		/* use by default 64k temp memory */
		maxlines = Clamp(65536 / w, 16, 128);

		/* now generate the bitmap bits */
		std::vector<uint8_t> buff(static_cast<size_t>(w) * maxlines); // by default generate 128 lines at a time.

		y = 0;
		do {
			/* determine # lines to write */
			uint n = std::min(h - y, maxlines);
			uint i;

			/* render the pixels into the buffer */
			callb(userdata, buff.data(), y, w, n);
			y += n;

			/* write them to pcx */
			for (i = 0; i != n; i++) {
				const uint8_t *bufp = buff.data() + i * w;
				uint8_t runchar = bufp[0];
				uint runcount = 1;
				uint j;

				/* for each pixel... */
				for (j = 1; j < w; j++) {
					uint8_t ch = bufp[j];

					if (ch != runchar || runcount >= 0x3f) {
						if (runcount > 1 || (runchar & 0xC0) == 0xC0) {
							if (fputc(0xC0 | runcount, f) == EOF) {
								return false;
							}
						}
						if (fputc(runchar, f) == EOF) {
							return false;
						}
						runcount = 0;
						runchar = ch;
					}
					runcount++;
				}

				/* write remaining bytes.. */
				if (runcount > 1 || (runchar & 0xC0) == 0xC0) {
					if (fputc(0xC0 | runcount, f) == EOF) {
						return false;
					}
				}
				if (fputc(runchar, f) == EOF) {
					return false;
				}
			}
		} while (y != h);

		/* write 8-bit colour palette */
		if (fputc(12, f) == EOF) {
			return false;
		}

		/* Palette is word-aligned, copy it to a temporary byte array */
		uint8_t tmp[256 * 3];

		for (uint i = 0; i < 256; i++) {
			tmp[i * 3 + 0] = palette[i].r;
			tmp[i * 3 + 1] = palette[i].g;
			tmp[i * 3 + 2] = palette[i].b;
		}
		success = fwrite(tmp, sizeof(tmp), 1, f) == 1;

		return success;
	}
};

static ScreenshotProvider_Pcx s_screenshot_provider_pcx;
