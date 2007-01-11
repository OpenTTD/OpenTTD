/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "functions.h"
#include "heightmap.h"
#include "clear_map.h"
#include "table/strings.h"
#include "void_map.h"
#include "debug.h"
#include "gfx.h"
#include "gui.h"
#include "saveload.h"
#include "bmp.h"
#include "helpers.hpp"

/**
 * Convert RGB colors to Grayscale using 29.9% Red, 58.7% Green, 11.4% Blue
 *  (average luminosity formula) -- Dalestan
 * This in fact is the NTSC Color Space -- TrueLight
 */
static inline byte RGBToGrayscale(byte red, byte green, byte blue)
{
	/* To avoid doubles and stuff, multiple it with a total of 65536 (16bits), then
	 *  divide by it to normalize the value to a byte again. */
	return ((red * 19595) + (green * 38470) + (blue * 7471)) / 65536;
}


#ifdef WITH_PNG

#include "png.h"

/**
 * The PNG Heightmap loader.
 */
static void ReadHeightmapPNGImageData(byte *map, png_structp png_ptr, png_infop info_ptr)
{
	uint x, y;
	byte gray_palette[256];
	png_bytep *row_pointers = NULL;

	/* Get palette and convert it to grayscale */
	if (info_ptr->color_type == PNG_COLOR_TYPE_PALETTE) {
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
	for (x = 0; x < info_ptr->width; x++) {
		for (y = 0; y < info_ptr->height; y++) {
			byte *pixel = &map[y * info_ptr->width + x];
			uint x_offset = x * info_ptr->channels;

			if (info_ptr->color_type == PNG_COLOR_TYPE_PALETTE) {
				*pixel = gray_palette[row_pointers[y][x_offset]];
			} else if (info_ptr->channels == 3) {
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
 * If map == NULL only the size of the PNG is read, otherwise a map
 * with grayscale pixels is allocated and assigned to *map.
 */
static bool ReadHeightmapPNG(char *filename, uint *x, uint *y, byte **map)
{
	FILE *fp;
	png_structp png_ptr = NULL;
	png_infop info_ptr  = NULL;

	fp = fopen(filename, "rb");
	if (fp == NULL) {
		ShowErrorMessage(STR_PNGMAP_ERR_FILE_NOT_FOUND, STR_PNGMAP_ERROR, 0, 0);
		return false;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		ShowErrorMessage(STR_PNGMAP_ERR_MISC, STR_PNGMAP_ERROR, 0, 0);
		fclose(fp);
		return false;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL || setjmp(png_jmpbuf(png_ptr))) {
		ShowErrorMessage(STR_PNGMAP_ERR_MISC, STR_PNGMAP_ERROR, 0, 0);
		fclose(fp);
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return false;
	}

	png_init_io(png_ptr, fp);

	/* Allocate memory and read image, without alpha or 16-bit samples
	 * (result is either 8-bit indexed/grayscale or 24-bit RGB) */
	png_set_packing(png_ptr);
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_PACKING | PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_STRIP_16, NULL);

	/* Maps of wrong color-depth are not used.
	 * (this should have been taken care of by stripping alpha and 16-bit samples on load) */
	if ((info_ptr->channels != 1) && (info_ptr->channels != 3) && (info_ptr->bit_depth != 8)) {
		ShowErrorMessage(STR_PNGMAP_ERR_IMAGE_TYPE, STR_PNGMAP_ERROR, 0, 0);
		fclose(fp);
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return false;
	}

	if (map != NULL) {
		*map = MallocT<byte>(info_ptr->width * info_ptr->height);

		if (*map == NULL) {
			ShowErrorMessage(STR_PNGMAP_ERR_MISC, STR_PNGMAP_ERROR, 0, 0);
			fclose(fp);
			png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
			return false;
		}

		ReadHeightmapPNGImageData(*map, png_ptr, info_ptr);
	}

	*x = info_ptr->width;
	*y = info_ptr->height;

	fclose(fp);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
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

	if (data->palette != NULL) {
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
 * If map == NULL only the size of the BMP is read, otherwise a map
 * with grayscale pixels is allocated and assigned to *map.
 */
static bool ReadHeightmapBMP(char *filename, uint *x, uint *y, byte **map)
{
	FILE *f;
	BmpInfo info;
	BmpData data;
	BmpBuffer buffer;

	// Init BmpData
	memset(&data, 0, sizeof(data));

	f = fopen(filename, "rb");
	if (f == NULL) {
		ShowErrorMessage(STR_PNGMAP_ERR_FILE_NOT_FOUND, STR_BMPMAP_ERROR, 0, 0);
		return false;
	}

	BmpInitializeBuffer(&buffer, f);

	if (!BmpReadHeader(&buffer, &info, &data)) {
		ShowErrorMessage(STR_BMPMAP_ERR_IMAGE_TYPE, STR_BMPMAP_ERROR, 0, 0);
		fclose(f);
		BmpDestroyData(&data);
		return false;
	}

	if (map != NULL) {
		if (!BmpReadBitmap(&buffer, &info, &data)) {
			ShowErrorMessage(STR_BMPMAP_ERR_IMAGE_TYPE, STR_BMPMAP_ERROR, 0, 0);
			fclose(f);
			BmpDestroyData(&data);
			return false;
		}

		*map = MallocT<byte>(info.width * info.height);
		if (*map == NULL) {
			ShowErrorMessage(STR_PNGMAP_ERR_MISC, STR_BMPMAP_ERROR, 0, 0);
			fclose(f);
			BmpDestroyData(&data);
			return false;
		}

		ReadHeightmapBMPImageData(*map, &info, &data);

	}

	BmpDestroyData(&data);

	*x = info.width;
	*y = info.height;

	fclose(f);
	return true;
}

static void GrayscaleToMapHeights(uint img_width, uint img_height, byte *map)
{
	/* Defines the detail of the aspect ratio (to avoid doubles) */
	const uint num_div = 16384;

	uint width, height;
	uint row, col;
	uint row_pad = 0, col_pad = 0;
	uint img_scale;
	uint img_row, img_col;
	TileIndex tile;

	/* Get map size and calculate scale and padding values */
	switch (_patches.heightmap_rotation) {
	case HM_COUNTER_CLOCKWISE:
		width   = MapSizeX();
		height  = MapSizeY();
		break;
	case HM_CLOCKWISE:
		width   = MapSizeY();
		height  = MapSizeX();
		break;
	default:
		NOT_REACHED();
		/* Avoids compiler warnings */
		return;
	}

	if ((img_width * num_div) / img_height > ((width * num_div) / height)) {
		/* Image is wider than map - center vertically */
		img_scale = (width * num_div) / img_width;
		row_pad = (height - ((img_height * img_scale) / num_div)) / 2;
	} else {
		/* Image is taller than map - center horizontally */
		img_scale = (height * num_div) / img_height;
		col_pad = (width - ((img_width * img_scale) / num_div)) / 2;
	}

	/* Form the landscape */
	for (row = 0; row < height - 1; row++) {
		for (col = 0; col < width - 1; col++) {
			switch (_patches.heightmap_rotation) {
			case HM_COUNTER_CLOCKWISE: tile = TileXY(col, row); break;
			case HM_CLOCKWISE:         tile = TileXY(row, col); break;
			default:                   NOT_REACHED(); return;
			}

			/* Check if current tile is within the 1-pixel map edge or padding regions */
			if ((DistanceFromEdge(tile) <= 1) ||
					(row < row_pad) || (row >= (img_height + row_pad)) ||
					(col < col_pad) || (col >= (img_width  + col_pad))) {
				SetTileHeight(tile, 0);
			} else {
				/* Use nearest neighbor resizing to scale map data.
				 *  We rotate the map 45 degrees (counter)clockwise */
				img_row = (((row - row_pad) * num_div) / img_scale);
				switch (_patches.heightmap_rotation) {
				case HM_COUNTER_CLOCKWISE:
					img_col = (((width - 1 - col - col_pad) * num_div) / img_scale);
					break;
				case HM_CLOCKWISE:
					img_col = (((col - col_pad) * num_div) / img_scale);
					break;
				default:
					NOT_REACHED();
					/* Avoids compiler warnings */
					return;
				}

				assert(img_row < img_height);
				assert(img_col < img_width);

				/* Color scales from 0 to 255, OpenTTD height scales from 0 to 15 */
				SetTileHeight(tile, map[img_row * img_width + img_col] / 16);
			}
			MakeClear(tile, CLEAR_GRASS, 3);
		}
	}
}

/**
 * This function takes care of the fact that land in OpenTTD can never differ
 * more than 1 in height
 */
static void FixSlopes(void)
{
	uint width, height;
	uint row, col;
	byte current_tile;

	/* Adjust height difference to maximum one horizontal/vertical change. */
	width   = MapSizeX();
	height  = MapSizeY();

	/* Top and left edge */
	for (row = 1; row < height - 2; row++) {
		for (col = 1; col < width - 2; col++) {
			/* Find lowest tile; either the top or left one */
			current_tile = TileHeight(TileXY(col - 1, row)); // top edge
			if (TileHeight(TileXY(col, row - 1)) < current_tile) {
				current_tile = TileHeight(TileXY(col, row - 1)); // left edge
			}

			/* Does the height differ more than one? */
			if (TileHeight(TileXY(col, row)) >= (uint)current_tile + 2) {
				/* Then change the height to be no more than one */
				SetTileHeight(TileXY(col, row), current_tile + 1);
			}
		}
	}

	/* Bottom and right edge */
	for (row = height - 2; row > 0; row--) {
		for (col = width - 2; col > 0; col--) {
			/* Find lowest tile; either the bottom and right one */
			current_tile = TileHeight(TileXY(col + 1, row)); // bottom edge
			if (TileHeight(TileXY(col, row + 1)) < current_tile) {
				current_tile = TileHeight(TileXY(col, row + 1)); // right edge
			}

			/* Does the height differ more than one? */
			if (TileHeight(TileXY(col, row)) >= (uint)current_tile + 2) {
				/* Then change the height to be no more than one */
				SetTileHeight(TileXY(col, row), current_tile + 1);
			}
		}
	}
}

/**
 * Reads the heightmap with the correct file reader
 */
static bool ReadHeightMap(char *filename, uint *x, uint *y, byte **map)
{
	switch (_file_to_saveload.mode) {
#ifdef WITH_PNG
		case SL_PNG:
			return ReadHeightmapPNG(filename, x, y, map);
#endif /* WITH_PNG */
		case SL_BMP:
			return ReadHeightmapBMP(filename, x, y, map);

		default:
			NOT_REACHED();
			/* Avoids compiler warnings */
			return false;
	}
}

bool GetHeightmapDimensions(char *filename, uint *x, uint *y)
{
	return ReadHeightMap(filename, x, y, NULL);
}

void LoadHeightmap(char *filename)
{
	uint x, y;
	byte *map = NULL;

	if (!ReadHeightMap(filename, &x, &y, &map)) {
		free(map);
		return;
	}

	GrayscaleToMapHeights(x, y, map);
	free(map);

	FixSlopes();
	MarkWholeScreenDirty();
}

void FlatEmptyWorld(byte tile_height)
{
	uint width, height;
	uint row, col;

	width  = MapSizeX();
	height = MapSizeY();

	for (row = 2; row < height - 2; row++) {
		for (col = 2; col < width - 2; col++) {
			SetTileHeight(TileXY(col, row), tile_height);
		}
	}

	FixSlopes();
	MarkWholeScreenDirty();
}
