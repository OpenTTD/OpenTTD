/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file screenshot_png.cpp PNG screenshot provider. */

#include "stdafx.h"
#include "core/math_func.hpp"
#include "debug.h"
#include "fileio_func.h"
#include "screenshot_type.h"

#include <png.h>

#ifdef PNG_TEXT_SUPPORTED
#include "rev.h"
#include "newgrf_config.h"
#include "ai/ai_info.hpp"
#include "company_base.h"
#include "base_media_base.h"
#include "base_media_graphics.h"
#endif /* PNG_TEXT_SUPPORTED */

#include "safeguards.h"

class ScreenshotProvider_Png : public ScreenshotProvider {
public:
	ScreenshotProvider_Png() : ScreenshotProvider("png", "PNG", 0) {}

	bool MakeImage(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const Colour *palette) override
	{
		png_color rq[256];
		uint i, y, n;
		uint maxlines;
		uint bpp = pixelformat / 8;
		png_structp png_ptr;
		png_infop info_ptr;

		/* only implemented for 8bit and 32bit images so far. */
		if (pixelformat != 8 && pixelformat != 32) return false;

		auto of = FileHandle::Open(name, "wb");
		if (!of.has_value()) return false;
		auto &f = *of;

		png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, const_cast<char *>(name), png_my_error, png_my_warning);

		if (png_ptr == nullptr) {
			return false;
		}

		info_ptr = png_create_info_struct(png_ptr);
		if (info_ptr == nullptr) {
			png_destroy_write_struct(&png_ptr, (png_infopp)nullptr);
			return false;
		}

		if (setjmp(png_jmpbuf(png_ptr))) {
			png_destroy_write_struct(&png_ptr, &info_ptr);
			return false;
		}

		png_init_io(png_ptr, f);

		png_set_filter(png_ptr, 0, PNG_FILTER_NONE);

		png_set_IHDR(png_ptr, info_ptr, w, h, 8, pixelformat == 8 ? PNG_COLOR_TYPE_PALETTE : PNG_COLOR_TYPE_RGB,
			PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

#ifdef PNG_TEXT_SUPPORTED

		/* Try to add some game metadata to the PNG screenshot so
		 * it's more useful for debugging and archival purposes. */
		png_text_struct text[2];
		memset(text, 0, sizeof(text));
		text[0].key = const_cast<char *>("Software");
		text[0].text = const_cast<char *>(_openttd_revision);
		text[0].text_length = strlen(_openttd_revision);
		text[0].compression = PNG_TEXT_COMPRESSION_NONE;

		std::string message;
		message.reserve(1024);
		fmt::format_to(std::back_inserter(message), "Graphics set: {} ({})\n", BaseGraphics::GetUsedSet()->name, BaseGraphics::GetUsedSet()->version);
		message += "NewGRFs:\n";
		if (_game_mode != GM_MENU) {
			for (const auto &c : _grfconfig) {
				fmt::format_to(std::back_inserter(message), "{:08X} {} {}\n", std::byteswap(c->ident.grfid), FormatArrayAsHex(c->ident.md5sum), c->filename);
			}
		}
		message += "\nCompanies:\n";
		for (const Company *c : Company::Iterate()) {
			if (c->ai_info == nullptr) {
				fmt::format_to(std::back_inserter(message), "{:2d}: Human\n", c->index);
			} else {
				fmt::format_to(std::back_inserter(message), "{:2d}: {} (v{})\n", c->index, c->ai_info->GetName(), c->ai_info->GetVersion());
			}
		}
		text[1].key = const_cast<char *>("Description");
		text[1].text = const_cast<char *>(message.c_str());
		text[1].text_length = message.size();
		text[1].compression = PNG_TEXT_COMPRESSION_zTXt;
		png_set_text(png_ptr, info_ptr, text, 2);

#endif /* PNG_TEXT_SUPPORTED */

		if (pixelformat == 8) {
			/* convert the palette to the .PNG format. */
			for (i = 0; i != 256; i++) {
				rq[i].red   = palette[i].r;
				rq[i].green = palette[i].g;
				rq[i].blue  = palette[i].b;
			}

			png_set_PLTE(png_ptr, info_ptr, rq, 256);
		}

		png_write_info(png_ptr, info_ptr);
		png_set_flush(png_ptr, 512);

		if (pixelformat == 32) {
			png_color_8 sig_bit;

			/* Save exact colour/alpha resolution */
			sig_bit.alpha = 0;
			sig_bit.blue  = 8;
			sig_bit.green = 8;
			sig_bit.red   = 8;
			sig_bit.gray  = 8;
			png_set_sBIT(png_ptr, info_ptr, &sig_bit);

			if constexpr (std::endian::native == std::endian::little) {
				png_set_bgr(png_ptr);
				png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
			} else {
				png_set_filler(png_ptr, 0, PNG_FILLER_BEFORE);
			}
		}

		/* use by default 64k temp memory */
		maxlines = Clamp(65536 / w, 16, 128);

		/* now generate the bitmap bits */
		std::vector<uint8_t> buff(static_cast<size_t>(w) * maxlines * bpp); // by default generate 128 lines at a time.

		y = 0;
		do {
			/* determine # lines to write */
			n = std::min(h - y, maxlines);

			/* render the pixels into the buffer */
			callb(userdata, buff.data(), y, w, n);
			y += n;

			/* write them to png */
			for (i = 0; i != n; i++) {
				png_write_row(png_ptr, (png_bytep)buff.data() + i * w * bpp);
			}
		} while (y != h);

		png_write_end(png_ptr, info_ptr);
		png_destroy_write_struct(&png_ptr, &info_ptr);

		return true;
	}

private:
	static void PNGAPI png_my_error(png_structp png_ptr, png_const_charp message)
	{
		Debug(misc, 0, "[libpng] error: {} - {}", message, (const char *)png_get_error_ptr(png_ptr));
		longjmp(png_jmpbuf(png_ptr), 1);
	}

	static void PNGAPI png_my_warning(png_structp png_ptr, png_const_charp message)
	{
		Debug(misc, 1, "[libpng] warning: {} - {}", message, (const char *)png_get_error_ptr(png_ptr));
	}
};

static ScreenshotProvider_Png s_screenshot_provider_png;
