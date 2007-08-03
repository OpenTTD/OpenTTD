/* $Id$ */

#include "../../stdafx.h"
#include "../../openttd.h"
#include "../../variables.h"
#include "../../macros.h"
#include "../../debug.h"
#include "../../functions.h"
#include "../../gfx.h"
#include "../../fileio.h"

#include "splash.h"

#ifdef WITH_PNG

#include <png.h>

static void PNGAPI png_my_error(png_structp png_ptr, png_const_charp message)
{
	DEBUG(misc, 0, "[libpng] error: %s - %s", message, (char *)png_get_error_ptr(png_ptr));
	longjmp(png_ptr->jmpbuf, 1);
}

static void PNGAPI png_my_warning(png_structp png_ptr, png_const_charp message)
{
	DEBUG(misc, 1, "[libpng] warning: %s - %s", message, (char *)png_get_error_ptr(png_ptr));
}

void DisplaySplashImage()
{
	png_byte header[8];
	FILE *f;
	png_structp png_ptr;
	png_infop info_ptr, end_info;
	uint width, height, bit_depth, color_type;
	png_colorp palette;
	int num_palette;
	png_bytep *row_pointers;
	uint8 *src, *dst;
	uint y;
	uint xoff, yoff;
	int i;

	f = FioFOpenFile(SPLASH_IMAGE_FILE);
	if (f == NULL) return;

	fread(header, 1, 8, f);
	if (png_sig_cmp(header, 0, 8) != 0) {
		fclose(f);
		return;
	}

	png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, (png_voidp) NULL, png_my_error, png_my_warning);

	if (png_ptr == NULL) {
		fclose(f);
		return;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		fclose(f);
		return;
	}

	end_info = png_create_info_struct(png_ptr);
	if (end_info == NULL) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
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

	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

	width            = png_get_image_width(png_ptr, info_ptr);
	height           = png_get_image_height(png_ptr, info_ptr);
	bit_depth        = png_get_bit_depth(png_ptr, info_ptr);
	color_type       = png_get_color_type(png_ptr, info_ptr);

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

	png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette);

	row_pointers = png_get_rows(png_ptr, info_ptr);

	memset(_screen.dst_ptr, 0xff, _screen.pitch * _screen.height);

	if (width > (uint) _screen.width) width = _screen.width;
	if (height > (uint) _screen.height) height = _screen.height;

	xoff = (_screen.width - width) / 2;
	yoff = (_screen.height - height) / 2;
	for (y = 0; y < height; y++) {
		src = row_pointers[y];
		dst = ((uint8 *) _screen.dst_ptr) + (yoff + y) * _screen.pitch + xoff;

		memcpy(dst, src, width);
	}

	for (i = 0; i < num_palette; i++) {
		_cur_palette[i].r = palette[i].red;
		_cur_palette[i].g = palette[i].green;
		_cur_palette[i].b = palette[i].blue;
	}

	_cur_palette[0xff].r = 0;
	_cur_palette[0xff].g = 0;
	_cur_palette[0xff].b = 0;

	_pal_first_dirty = 0;
	_pal_count_dirty = 256;

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	fclose(f);
	return;
}



#else /* WITH_PNG */

void DisplaySplashImage() {}

#endif /* WITH_PNG */
