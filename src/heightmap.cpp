/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file heightmap.cpp Creating of maps from heightmaps. */

#include "stdafx.h"
#include "heightmap_type.h"
#include "heightmap_base.h"
#include "clear_map.h"
#include "void_map.h"
#include "error.h"
#include "saveload/saveload.h"
#include "bmp.h"
#include "gfx_func.h"
#include "fios.h"
#include "fileio_func.h"

#include "table/strings.h"

#include "safeguards.h"

/**
 * Convert RGB colours to Grayscale using 29.9% Red, 58.7% Green, 11.4% Blue
 *  (average luminosity formula, NTSC Colour Space)
 */
static inline byte RGBToGrayscale(byte red, byte green, byte blue)
{
	/* To avoid doubles and stuff, multiply it with a total of 65536 (16bits), then
	 *  divide by it to normalize the value to a byte again. */
	return ((red * 19595) + (green * 38470) + (blue * 7471)) / 65536;
}


#ifdef WITH_PNG

#include <png.h>

/**
 * The PNG Heightmap loader.
 */
static void ReadHeightmapPNGImageData(byte *map, png_structp png_ptr, png_infop info_ptr)
{
	uint x, y;
	byte gray_palette[256];
	png_bytep *row_pointers = nullptr;
	bool has_palette = png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_PALETTE;
	uint channels = png_get_channels(png_ptr, info_ptr);

	/* Get palette and convert it to grayscale */
	if (has_palette) {
		int i;
		int palette_size;
		png_color *palette;
		bool all_gray = true;

		png_get_PLTE(png_ptr, info_ptr, &palette, &palette_size);
		for (i = 0; i < palette_size && (palette_size != 16 || all_gray); i++) {
			all_gray &= palette[i].red == palette[i].green && palette[i].red == palette[i].blue;
			gray_palette[i] = RGBToGrayscale(palette[i].red, palette[i].green, palette[i].blue);
		}

		/**
		 * For a non-gray palette of size 16 we assume that
		 * the order of the palette determines the height;
		 * the first entry is the sea (level 0), the second one
		 * level 1, etc.
		 */
		if (palette_size == 16 && !all_gray) {
			for (i = 0; i < palette_size; i++) {
				gray_palette[i] = 256 * i / palette_size;
			}
		}
	}

	row_pointers = png_get_rows(png_ptr, info_ptr);

	/* Read the raw image data and convert in 8-bit grayscale */
	for (x = 0; x < png_get_image_width(png_ptr, info_ptr); x++) {
		for (y = 0; y < png_get_image_height(png_ptr, info_ptr); y++) {
			byte *pixel = &map[y * png_get_image_width(png_ptr, info_ptr) + x];
			uint x_offset = x * channels;

			if (has_palette) {
				*pixel = gray_palette[row_pointers[y][x_offset]];
			} else if (channels == 3) {
				*pixel = RGBToGrayscale(row_pointers[y][x_offset + 0],
						row_pointers[y][x_offset + 1], row_pointers[y][x_offset + 2]);
			} else {
				*pixel = row_pointers[y][x_offset];
			}
		}
	}
}

/**
 * Reads the heightmap and/or size of the heightmap from a PNG file.
 * If map == nullptr only the size of the PNG is read, otherwise a map
 * with grayscale pixels is allocated and assigned to *map.
 */
static bool ReadHeightmapPNG(const char *filename, uint *x, uint *y, byte **map)
{
	FILE *fp;
	png_structp png_ptr = nullptr;
	png_infop info_ptr  = nullptr;

	fp = FioFOpenFile(filename, "rb", HEIGHTMAP_DIR);
	if (fp == nullptr) {
		ShowErrorMessage(STR_ERROR_PNGMAP, STR_ERROR_PNGMAP_FILE_NOT_FOUND, WL_ERROR);
		return false;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if (png_ptr == nullptr) {
		ShowErrorMessage(STR_ERROR_PNGMAP, STR_ERROR_PNGMAP_MISC, WL_ERROR);
		fclose(fp);
		return false;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == nullptr || setjmp(png_jmpbuf(png_ptr))) {
		ShowErrorMessage(STR_ERROR_PNGMAP, STR_ERROR_PNGMAP_MISC, WL_ERROR);
		fclose(fp);
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		return false;
	}

	png_init_io(png_ptr, fp);

	/* Allocate memory and read image, without alpha or 16-bit samples
	 * (result is either 8-bit indexed/grayscale or 24-bit RGB) */
	png_set_packing(png_ptr);
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_PACKING | PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_STRIP_16, nullptr);

	/* Maps of wrong colour-depth are not used.
	 * (this should have been taken care of by stripping alpha and 16-bit samples on load) */
	if ((png_get_channels(png_ptr, info_ptr) != 1) && (png_get_channels(png_ptr, info_ptr) != 3) && (png_get_bit_depth(png_ptr, info_ptr) != 8)) {
		ShowErrorMessage(STR_ERROR_PNGMAP, STR_ERROR_PNGMAP_IMAGE_TYPE, WL_ERROR);
		fclose(fp);
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		return false;
	}

	uint width = png_get_image_width(png_ptr, info_ptr);
	uint height = png_get_image_height(png_ptr, info_ptr);

	/* Check if image dimensions don't overflow a size_t to avoid memory corruption. */
	if ((uint64)width * height >= (size_t)-1) {
		ShowErrorMessage(STR_ERROR_PNGMAP, STR_ERROR_HEIGHTMAP_TOO_LARGE, WL_ERROR);
		fclose(fp);
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		return false;
	}

	if (map != nullptr) {
		*map = MallocT<byte>(width * height);
		ReadHeightmapPNGImageData(*map, png_ptr, info_ptr);
	}

	*x = width;
	*y = height;

	fclose(fp);
	png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
	return true;
}

#endif /* WITH_PNG */


/**
 * The BMP Heightmap loader.
 */
static void ReadHeightmapBMPImageData(byte *map, BmpInfo *info, BmpData *data)
{
	uint x, y;
	byte gray_palette[256];

	if (data->palette != nullptr) {
		uint i;
		bool all_gray = true;

		if (info->palette_size != 2) {
			for (i = 0; i < info->palette_size && (info->palette_size != 16 || all_gray); i++) {
				all_gray &= data->palette[i].r == data->palette[i].g && data->palette[i].r == data->palette[i].b;
				gray_palette[i] = RGBToGrayscale(data->palette[i].r, data->palette[i].g, data->palette[i].b);
			}

			/**
			 * For a non-gray palette of size 16 we assume that
			 * the order of the palette determines the height;
			 * the first entry is the sea (level 0), the second one
			 * level 1, etc.
			 */
			if (info->palette_size == 16 && !all_gray) {
				for (i = 0; i < info->palette_size; i++) {
					gray_palette[i] = 256 * i / info->palette_size;
				}
			}
		} else {
			/**
			 * For a palette of size 2 we assume that the order of the palette determines the height;
			 * the first entry is the sea (level 0), the second one is the land (level 1)
			 */
			gray_palette[0] = 0;
			gray_palette[1] = 16;
		}
	}

	/* Read the raw image data and convert in 8-bit grayscale */
	for (y = 0; y < info->height; y++) {
		byte *pixel = &map[y * info->width];
		byte *bitmap = &data->bitmap[y * info->width * (info->bpp == 24 ? 3 : 1)];

		for (x = 0; x < info->width; x++) {
			if (info->bpp != 24) {
				*pixel++ = gray_palette[*bitmap++];
			} else {
				*pixel++ = RGBToGrayscale(*bitmap, *(bitmap + 1), *(bitmap + 2));
				bitmap += 3;
			}
		}
	}
}

/**
 * Reads the heightmap and/or size of the heightmap from a BMP file.
 * If map == nullptr only the size of the BMP is read, otherwise a map
 * with grayscale pixels is allocated and assigned to *map.
 */
static bool ReadHeightmapBMP(const char *filename, uint *x, uint *y, byte **map)
{
	FILE *f;
	BmpInfo info;
	BmpData data;
	BmpBuffer buffer;

	/* Init BmpData */
	memset(&data, 0, sizeof(data));

	f = FioFOpenFile(filename, "rb", HEIGHTMAP_DIR);
	if (f == nullptr) {
		ShowErrorMessage(STR_ERROR_BMPMAP, STR_ERROR_PNGMAP_FILE_NOT_FOUND, WL_ERROR);
		return false;
	}

	BmpInitializeBuffer(&buffer, f);

	if (!BmpReadHeader(&buffer, &info, &data)) {
		ShowErrorMessage(STR_ERROR_BMPMAP, STR_ERROR_BMPMAP_IMAGE_TYPE, WL_ERROR);
		fclose(f);
		BmpDestroyData(&data);
		return false;
	}

	/* Check if image dimensions don't overflow a size_t to avoid memory corruption. */
	if ((uint64)info.width * info.height >= (size_t)-1 / (info.bpp == 24 ? 3 : 1)) {
		ShowErrorMessage(STR_ERROR_BMPMAP, STR_ERROR_HEIGHTMAP_TOO_LARGE, WL_ERROR);
		fclose(f);
		BmpDestroyData(&data);
		return false;
	}

	if (map != nullptr) {
		if (!BmpReadBitmap(&buffer, &info, &data)) {
			ShowErrorMessage(STR_ERROR_BMPMAP, STR_ERROR_BMPMAP_IMAGE_TYPE, WL_ERROR);
			fclose(f);
			BmpDestroyData(&data);
			return false;
		}

		*map = MallocT<byte>(info.width * info.height);
		ReadHeightmapBMPImageData(*map, &info, &data);
	}

	BmpDestroyData(&data);

	*x = info.width;
	*y = info.height;

	fclose(f);
	return true;
}

/**
 * Reads the heightmap with the correct file reader.
 * @param dft Type of image file.
 * @param filename Name of the file to load.
 * @param[out] x Length of the image.
 * @param[out] y Height of the image.
 * @param[in,out] map If not \c nullptr, destination to store the loaded block of image data.
 * @return Whether loading was successful.
 */
static bool ReadHeightMap(DetailedFileType dft, const char *filename, uint *x, uint *y, byte **map)
{
	switch (dft) {
		default:
			NOT_REACHED();

#ifdef WITH_PNG
		case DFT_HEIGHTMAP_PNG:
			return ReadHeightmapPNG(filename, x, y, map);
#endif /* WITH_PNG */

		case DFT_HEIGHTMAP_BMP:
			return ReadHeightmapBMP(filename, x, y, map);
	}
}

/**
 * Allows to create an extended heightmap from a single height layer in png or bmp format.
 * @param file_path Full path to the legacy heightmap to load.
 * @param file_name Name of the file.
 */
void ExtendedHeightmap::LoadLegacyHeightmap(DetailedFileType dft, char *file_path, char *file_name)
{
	/* Try to load the legacy heightmap first. */
	HeightmapLayer *height_layer = new HeightmapLayer();
	height_layer->type = HLT_HEIGHTMAP;
	height_layer->information = NULL;

	if (!ReadHeightMap(dft, file_path, &height_layer->width, &height_layer->height, &height_layer->information)) {
		free(height_layer->information);
		delete height_layer;
		return;
	}

	this->layers[HLT_HEIGHTMAP] = height_layer;

	/* Initialize all extended heightmap parameters to be consistent with the old behavior. */
	strecpy(this->filename, file_name, lastof(this->filename));
	this->max_map_height = 255;
	this->min_map_desired_height = 0;
	this->max_map_desired_height = 15;
	this->width = height_layer->width;
	this->height = height_layer->height;
	this->rotation = (HeightmapRotation) _settings_newgame.game_creation.heightmap_rotation;
	this->freeform_edges = _settings_newgame.construction.freeform_edges;
}

/**
 * Creates an OpenTTD map based on the information contained in the extended heightmap.
 */
void ExtendedHeightmap::CreateMap()
{
	/* The extended heightmap should be valid before we actually start applying data to the OpenTTD map. */
	assert(this->IsValid());

	/* The game map size must have been set up at this point, and the extended heightmap must be correctly initialized. */
	assert((this->rotation == HM_COUNTER_CLOCKWISE && this->width == MapSizeX() && this->height == MapSizeY()) ||
			(this->rotation == HM_CLOCKWISE && this->width == MapSizeY() && this->height == MapSizeX()));

	/* Apply general extended heightmap properties to the current map. */
	_settings_game.construction.freeform_edges = this->freeform_edges;

	/* Apply all layers. */
	this->ApplyLayers();
}

/**
 * Applies all layers to the current map, in the right order.
 */
void ExtendedHeightmap::ApplyLayers()
{
	/* The height layer must always go first. */
	const HeightmapLayer *height_layer = this->layers[HLT_HEIGHTMAP];

	/* Create the terrain with the height specified by the layer. */
	this->ApplyHeightLayer(height_layer);
}

/**
 * Applies the height layer to the current map.
 * @todo Check if the scaling process can be generalized somehow.
 */
void ExtendedHeightmap::ApplyHeightLayer(const HeightmapLayer *height_layer)
{
	/* This function is meant for heightmap layers only. */
	assert(height_layer->type == HLT_HEIGHTMAP);

	/* Defines the detail of the aspect ratio (to avoid doubles) */
	const uint num_div = 16384;

	uint row, col;
	uint row_pad = 0, col_pad = 0;
	uint img_scale;
	uint img_row, img_col;
	TileIndex tile;

	if ((height_layer->width * num_div) / height_layer->height > ((this->width * num_div) / this->height)) {
		/* Image is wider than map - center vertically */
		img_scale = (this->width * num_div) / height_layer->width;
		row_pad = (1 + this->height - ((height_layer->height * img_scale) / num_div)) / 2;
	} else {
		/* Image is taller than map - center horizontally */
		img_scale = (this->height * num_div) / height_layer->height;
		col_pad = (1 + this->width - ((height_layer->width * img_scale) / num_div)) / 2;
	}

	if (this->freeform_edges) {
		for (uint x = 0; x < MapSizeX(); x++) MakeVoid(TileXY(x, 0));
		for (uint y = 0; y < MapSizeY(); y++) MakeVoid(TileXY(0, y));
	}

	/* Form the landscape */
	for (row = 0; row < this->height; row++) {
		for (col = 0; col < this->width; col++) {
			switch (this->rotation) {
				case HM_COUNTER_CLOCKWISE: tile = TileXY(col, row); break;

				case HM_CLOCKWISE:         tile = TileXY(row, col); break;

				default: NOT_REACHED();
			}

			/* Check if current tile is within the 1-pixel map edge or padding regions */
			if ((!this->freeform_edges && DistanceFromEdge(tile) <= 1) ||
					(row < row_pad) || (row >= (this->height - row_pad - (_settings_game.construction.freeform_edges ? 0 : 1))) ||
					(col < col_pad) || (col >= (this->width  - col_pad - (_settings_game.construction.freeform_edges ? 0 : 1)))) {
				SetTileHeight(tile, 0);
			} else {
				/* Use nearest neighbour resizing to scale map data.
				 *  We rotate the map 45 degrees (counter)clockwise */
				img_row = (((row - row_pad) * num_div) / img_scale);
				switch (this->rotation) {
					case HM_COUNTER_CLOCKWISE:
						img_col = (((this->width - 1 - col - col_pad) * num_div) / img_scale);
						break;

					case HM_CLOCKWISE:
						img_col = (((col - col_pad) * num_div) / img_scale);
						break;

					default: NOT_REACHED();
				}

				assert(img_row < height_layer->height);
				assert(img_col < height_layer->width);

				uint tile_height = height_layer->information[img_row * height_layer->width + img_col] * num_div;
				/* Colour scales from 0 to 255, OpenTTD height scales from min_map_desired_height to max_map_desired_height */
				tile_height = (tile_height * ((max_map_desired_height + 1) - min_map_desired_height) + min_map_desired_height * num_div) / 256;
				SetTileHeight(tile, tile_height / num_div);
			}
			/* Only clear the tiles within the map area. */
			if (TileX(tile) != MapMaxX() && TileY(tile) != MapMaxY() &&
					(!this->freeform_edges || (TileX(tile) != 0 && TileY(tile) != 0))) {
				MakeClear(tile, CLEAR_GRASS, 3);
			}
		}
	}
}

/**
 * Checks if an extended heightmap can be used for generating an OpenTTD map.
 * @return False if the extended heightmap fails to met any of the conditions required for generating a valid OpenTTD map. True otherwise.
 */
bool ExtendedHeightmap::IsValid()
{
	/* All extended heightmaps must have a height layer. */
	if (this->layers[HLT_HEIGHTMAP] == NULL) return false;

	return true;
}
