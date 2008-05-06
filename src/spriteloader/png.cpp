/* $Id$ */

/** @file png.cpp Reading sprites from png files. */

#ifdef WITH_PNG

#include "../stdafx.h"
#include "../gfx_func.h"
#include "../fileio.h"
#include "../debug.h"
#include "../core/alloc_func.hpp"
#include "png.hpp"
#include <png.h>

#define PNG_SLOT 62

static void PNGAPI png_my_read(png_structp png_ptr, png_bytep data, png_size_t length)
{
	FioReadBlock(data, length);
}

static void PNGAPI png_my_error(png_structp png_ptr, png_const_charp message)
{
	DEBUG(sprite, 0, "ERROR (libpng): %s - %s", message, (char *)png_get_error_ptr(png_ptr));
	longjmp(png_ptr->jmpbuf, 1);
}

static void PNGAPI png_my_warning(png_structp png_ptr, png_const_charp message)
{
	DEBUG(sprite, 0, "WARNING (libpng): %s - %s", message, (char *)png_get_error_ptr(png_ptr));
}

static bool OpenPNGFile(const char *filename, uint32 id, bool mask)
{
	char png_file[MAX_PATH];

	snprintf(png_file, sizeof(png_file), "sprites" PATHSEP "%s" PATHSEP "%d%s.png", filename, id, mask ? "m" : "");
	if (FioCheckFileExists(png_file)) {
		FioOpenFile(PNG_SLOT, png_file);
		return true;
	}

	/* TODO -- Add TAR support */
	return false;
}

static bool LoadPNG(SpriteLoader::Sprite *sprite, const char *filename, uint32 id, bool mask)
{
	png_byte header[8];
	png_structp png_ptr;
	png_infop info_ptr, end_info;
	uint bit_depth, color_type;
	uint i, pixelsize;
	png_bytep row_pointer;
	SpriteLoader::CommonPixel *dst;

	if (!OpenPNGFile(filename, id, mask)) return mask; // If mask is true, and file not found, continue true anyway, as it isn't a show-stopper

	/* Check the header */
	FioReadBlock(header, 8);
	if (png_sig_cmp(header, 0, 8) != 0) return false;

	/* Create the reader */
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL, png_my_error, png_my_warning);
	if (png_ptr == NULL) return false;

	/* Create initial stuff */
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		return false;
	}
	end_info = png_create_info_struct(png_ptr);
	if (end_info == NULL) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		return false;
	}

	/* Make sure that upon error, we can clean up graceful */
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return false;
	}

	/* Read the file */
	png_set_read_fn(png_ptr, NULL, png_my_read);
	png_set_sig_bytes(png_ptr, 8);

	png_read_info(png_ptr, info_ptr);

	if (!mask) {
		/* Read the text chunks */
		png_textp text_ptr;
		int num_text = 0;
		png_get_text(png_ptr, info_ptr, &text_ptr, &num_text);
		if (num_text == 0) DEBUG(misc, 0, "Warning: PNG Sprite '%s/%d.png' doesn't have x_offs and y_offs; expect graphical problems", filename, id);
		for (int i = 0; i < num_text; i++) {
			/* x_offs and y_offs are in the text-chunk of PNG */
			if (strcmp("x_offs", text_ptr[i].key) == 0) sprite->x_offs = strtol(text_ptr[i].text, NULL, 0);
			if (strcmp("y_offs", text_ptr[i].key) == 0) sprite->y_offs = strtol(text_ptr[i].text, NULL, 0);
		}

		sprite->height = info_ptr->height;
		sprite->width  = info_ptr->width;
		sprite->data = CallocT<SpriteLoader::CommonPixel>(sprite->width * sprite->height);
	}

	bit_depth  = png_get_bit_depth(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);

	if (mask && (bit_depth != 8 || color_type != PNG_COLOR_TYPE_PALETTE)) {
		DEBUG(misc, 0, "Ignoring mask for SpriteID %d as it isn't a 8 bit palette image", id);
		return true;
	}

	if (!mask) {
		if (bit_depth == 16) png_set_strip_16(png_ptr);

		if (color_type == PNG_COLOR_TYPE_PALETTE) {
			png_set_palette_to_rgb(png_ptr);
			color_type = PNG_COLOR_TYPE_RGB;
		}
		if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
			png_set_gray_to_rgb(png_ptr);
			color_type = PNG_COLOR_TYPE_RGB;
		}

		if (color_type == PNG_COLOR_TYPE_RGB) {
			png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
		}

		pixelsize = sizeof(uint32);
	} else {
		pixelsize = sizeof(uint8);
	}

	row_pointer = (png_byte *)MallocT<byte>(info_ptr->width * pixelsize);
	if (row_pointer == NULL) {
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return false;
	}

	for (i = 0; i < info_ptr->height; i++) {
		png_read_row(png_ptr, row_pointer, NULL);

		dst = sprite->data + i * info_ptr->width;

		for (uint x = 0; x < info_ptr->width; x++) {
			if (mask) {
				if (row_pointer[x * sizeof(uint8)] != 0) {
					dst[x].r = 0;
					dst[x].g = 0;
					dst[x].b = 0;
					/* Alpha channel is used from the original image (to allow transparency in remap colors) */
					dst[x].m = row_pointer[x * sizeof(uint8)];
				}
			} else {
				dst[x].r = row_pointer[x * sizeof(uint32) + 0];
				dst[x].g = row_pointer[x * sizeof(uint32) + 1];
				dst[x].b = row_pointer[x * sizeof(uint32) + 2];
				dst[x].a = row_pointer[x * sizeof(uint32) + 3];
				dst[x].m = 0;
			}
		}
	}

	free(row_pointer);
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	return true;
}

bool SpriteLoaderPNG::LoadSprite(SpriteLoader::Sprite *sprite, uint8 file_slot, uint32 file_pos)
{
	const char *filename = FioGetFilename(file_slot);
	if (!LoadPNG(sprite, filename, file_pos, false)) return false;
	if (!LoadPNG(sprite, filename, file_pos, true)) return false;
	return true;
}

#endif /* WITH_PNG */
