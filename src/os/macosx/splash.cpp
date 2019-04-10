/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file splash.cpp Splash screen support for OSX. */

#include "../../stdafx.h"
#include "../../openttd.h"
#include "../../debug.h"
#include "../../gfx_func.h"
#include "../../fileio_func.h"
#include "../../blitter/factory.hpp"
#include "../../core/mem_func.hpp"

#include "splash.h"

#ifdef WITH_PNG

#include <png.h>

#include "../../safeguards.h"

/**
 * Handle pnglib error.
 *
 * @param png_ptr Pointer to png struct.
 * @param message Error message text.
 */
static void PNGAPI png_my_error(png_structp png_ptr, png_const_charp message)
{
	DEBUG(misc, 0, "[libpng] error: %s - %s", message, (char *)png_get_error_ptr(png_ptr));
	longjmp(png_jmpbuf(png_ptr), 1);
}

/**
 * Handle warning in pnglib.
 *
 * @param png_ptr Pointer to png struct.
 * @param message Warning message text.
 */
static void PNGAPI png_my_warning(png_structp png_ptr, png_const_charp message)
{
	DEBUG(misc, 1, "[libpng] warning: %s - %s", message, (char *)png_get_error_ptr(png_ptr));
}

/**
 * Display a splash image shown on startup (WITH_PNG).
 */
void DisplaySplashImage()
{
	FILE *f = FioFOpenFile(SPLASH_IMAGE_FILE, "r", BASESET_DIR);
	if (f == nullptr) return;

	png_byte header[8];
	fread(header, sizeof(png_byte), 8, f);
	if (png_sig_cmp(header, 0, 8) != 0) {
		fclose(f);
		return;
	}

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp) nullptr, png_my_error, png_my_warning);

	if (png_ptr == nullptr) {
		fclose(f);
		return;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == nullptr) {
		png_destroy_read_struct(&png_ptr, (png_infopp)nullptr, (png_infopp)nullptr);
		fclose(f);
		return;
	}

	png_infop end_info = png_create_info_struct(png_ptr);
	if (end_info == nullptr) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)nullptr);
		fclose(f);
		return;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		fclose(f);
		return;
	}

	png_init_io(png_ptr, f);
	png_set_sig_bytes(png_ptr, 8);

	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, nullptr);

	uint width      = png_get_image_width(png_ptr, info_ptr);
	uint height     = png_get_image_height(png_ptr, info_ptr);
	uint bit_depth  = png_get_bit_depth(png_ptr, info_ptr);
	uint color_type = png_get_color_type(png_ptr, info_ptr);

	if (color_type != PNG_COLOR_TYPE_PALETTE || bit_depth != 8) {
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		fclose(f);
		return;
	}

	if (!png_get_valid(png_ptr, info_ptr, PNG_INFO_PLTE)) {
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		fclose(f);
		return;
	}

	png_colorp palette;
	int num_palette;
	png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette);

	png_bytep *row_pointers = png_get_rows(png_ptr, info_ptr);

	if (width > (uint) _screen.width) width = _screen.width;
	if (height > (uint) _screen.height) height = _screen.height;

	uint xoff = (_screen.width - width) / 2;
	uint yoff = (_screen.height - height) / 2;

	switch (BlitterFactory::GetCurrentBlitter()->GetScreenDepth()) {
		case 8: {
				uint8 *dst_ptr = (uint8 *)_screen.dst_ptr;
				/* Initialize buffer */
				MemSetT(dst_ptr, 0xff, _screen.pitch * _screen.height);

				for (uint y = 0; y < height; y++) {
					uint8 *src = row_pointers[y];
					uint8 *dst = dst_ptr + (yoff + y) * _screen.pitch + xoff;

					memcpy(dst, src, width);
				}

				for (int i = 0; i < num_palette; i++) {
					_cur_palette.palette[i].a = i == 0 ? 0 : 0xff;
					_cur_palette.palette[i].r = palette[i].red;
					_cur_palette.palette[i].g = palette[i].green;
					_cur_palette.palette[i].b = palette[i].blue;
				}

				_cur_palette.palette[0xff].a = 0xff;
				_cur_palette.palette[0xff].r = 0;
				_cur_palette.palette[0xff].g = 0;
				_cur_palette.palette[0xff].b = 0;

				_cur_palette.first_dirty = 0;
				_cur_palette.count_dirty = 256;
				break;
			}
		case 32: {
				uint32 *dst_ptr = (uint32 *)_screen.dst_ptr;
				/* Initialize buffer */
				MemSetT(dst_ptr, 0, _screen.pitch * _screen.height);

				for (uint y = 0; y < height; y++) {
					uint8 *src = row_pointers[y];
					uint32 *dst = dst_ptr + (yoff + y) * _screen.pitch + xoff;

					for (uint x = 0; x < width; x++) {
						dst[x] = palette[src[x]].blue | (palette[src[x]].green << 8) | (palette[src[x]].red << 16) | 0xff000000;
					}
				}
				break;
			}
	}

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	fclose(f);
	return;
}



#else /* WITH_PNG */

/**
 * Empty 'Display a splash image' routine (WITHOUT_PNG).
 */
void DisplaySplashImage() {}

#endif /* WITH_PNG */
