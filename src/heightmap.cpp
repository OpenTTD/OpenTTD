/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file heightmap.cpp Creating of maps from heightmaps. */

#include <iostream> // SFTODO TEMP
#include "stdafx.h"
#include <memory> // SFTODO?
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
#include "ini_type.h" // SFTODO!?
#include "command_func.h" // SFTODO!?
#include "strings_func.h" // SFTODO!?

#include "table/strings.h"

#include "safeguards.h"
#define SFTODOMAXMAXDIMENSION 4096 // SFTODO SUPER TEMP JUST SO I HAVE A PLACEHOLDER TO USE, VALUE PROB NOT CORRECT AND PROB SHOULDN'T BE A #DEFINE

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
		case DFT_HEIGHTMAP_EHM:
			NOT_REACHED();

#ifdef WITH_PNG
		case DFT_HEIGHTMAP_PNG:
			return ReadHeightmapPNG(filename, x, y, map);
#endif /* WITH_PNG */

		case DFT_HEIGHTMAP_BMP:
			return ReadHeightmapBMP(filename, x, y, map);
	}
}

/** Class for parsing metadata.txt in an extended heightmap. */
struct MetadataIniFile : IniLoadFile {
	bool error;

	MetadataIniFile() : error(false) {}

        virtual FILE *OpenFile(const char *filename, Subdirectory subdir, size_t *size) {
		return FioFOpenFile(filename, "rb", subdir, size);
	}

	virtual void ReportFileError(const char * const pre, const char * const buffer, const char * const post)
        {
		// EHTODO: Is there any way I can include pre/buffer/post in the error message?
		std::cout << "SFTODOERROR: " << pre << buffer << post << std::endl;
		error = true;
        }
};

// SFTODO DOXYGEN
static bool GetStrGroupItem(IniGroup *group, const char *item_name, const char *default_value, const char **result)
{
	assert(item_name != nullptr);
	assert(result != nullptr);

	IniItem *item = group->GetItem(item_name, false);
	const char *item_value;
	if (item == nullptr) {
		if (default_value == nullptr) {
			SetDParamStr(0, group->name);
			SetDParamStr(1, item_name);
			ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_GROUP_MISSING_ITEM, INVALID_STRING_ID, WL_ERROR);
			return false;
		}
		item_value = default_value;
	} else {
		item_value = item->value;
	}

	*result = item_value;
	return true;
}

// SFTODO: DOXYGEN
static bool GetUIntGroupItemWithValidation(IniGroup *group, const char *item_name, const char *default_value, uint max_valid, uint *result)
{
	const char *item_value;
	if (!GetStrGroupItem(group, item_name, default_value, &item_value)) return false;

	for (const char *p = item_value; *p != '\0'; ++p) {
		if (!isdigit(static_cast<unsigned char>(*p))) {
			SetDParamStr(0, group->name);
			SetDParamStr(1, item_name);
			ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_GROUP_NONNUMERIC_ITEM, INVALID_STRING_ID, WL_ERROR);
			return false;
		}
	}

	uint u = static_cast<uint>(strtoul(item_value, nullptr, 10));
	std::cout << "SFTODOU " << u << std::endl;
	if (u > max_valid) {
		SetDParamStr(0, group->name);
		SetDParamStr(1, item_name);
		SetDParam(2, u);
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_GROUP_ITEM_TOO_LARGE, INVALID_STRING_ID, WL_ERROR);
		return false;
	}

	*result = u;
	return true;
}

/**
 * Allows to create an extended heightmap from a .ehm file (a tar file containing special
 * files).
 * @param file_path Full path to the .ehm file to load.
 * @param file_name Name of the file.
 */
void ExtendedHeightmap::LoadExtendedHeightmap(char *file_path, char *file_name)
{
	strecpy(this->filename, file_name, lastof(this->filename));
	this->freeform_edges = true;

	// EHTODO: I am sure I'm misusing TarScanner; it seems to be oriented around scanning for tar files, but I want to use it to
	// access a specific tar file. What I have here seems to work, but it doesn't feel right. I have to put "./" at the start of
	// filenames within the tar file to be able to access them, this may be normal but I'm not sure.
	TarScanner ts;
	ts.Reset(HEIGHTMAP_DIR);
	std::cout << "SFTODOX1: " << file_path << std::endl;
	std::cout << "SFTODOX2: " << file_name << std::endl;
	if (!ts.AddFile(HEIGHTMAP_DIR, file_path)) {
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_OPENING_EHM, INVALID_STRING_ID, WL_ERROR);
		return;
	}
	MetadataIniFile metadata;
	metadata.LoadFromDisk("./metadata.txt", HEIGHTMAP_DIR);
	if (metadata.error) {
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_PARSING_METADATA, INVALID_STRING_ID, WL_ERROR);
		return;
	}
	IniGroup *extended_heightmap_group = metadata.GetGroup("extended_heightmap", 0, false);
	if (extended_heightmap_group == nullptr) {
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_MISSING_EXTENDED_HEIGHTMAP_GROUP, INVALID_STRING_ID, WL_ERROR);
		return;
	}
	IniItem *format_version = extended_heightmap_group->GetItem("format_version", false);
	if (format_version == nullptr) {
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_MISSING_FORMAT_VERSION, INVALID_STRING_ID, WL_ERROR);
		return;
	}
	if (strcmp(format_version->value, "1") != 0) {
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_UNSUPPORTED_VERSION, INVALID_STRING_ID, WL_ERROR);
		return;
	}

	// EHTODO: Should this be "rotation" or "orientation"? Wiki uses both in different places... I've used
	// rotation for the variable as that matches the enum name HeightmapRotation.
	IniItem *rotation = extended_heightmap_group->GetItem("orientation", false);
	if (rotation == nullptr) {
		// EHTODO: Because we take whatever happens to be the default, in practice an extended heightmap
		// creator should always specify a rotation if they are also specifying an explicit height and/or
		// width and the heightmap layer is non-square, otherwise the rotation may be sub-optimal. It may
		// simply be best to make this property mandatory.
		this->rotation = static_cast<HeightmapRotation>(_settings_newgame.game_creation.heightmap_rotation);
	} else {
		// SFTODO: CASE SENSITIVITY!?
		if (strcmp(rotation->value, "ccw") == 0) {
			this->rotation = HM_COUNTER_CLOCKWISE;
		} else if (strcmp(rotation->value, "cw") == 0) {
			this->rotation = HM_CLOCKWISE;
		} else {
			ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_INVALID_ROTATION, INVALID_STRING_ID, WL_ERROR);
			return;
		}
	}
	std::cout << "SFTODOQ9: " << static_cast<int>(this->rotation) << std::endl;

	IniItem *climate = extended_heightmap_group->GetItem("climate", false);
	if (climate == nullptr) {
		this->landscape = static_cast<LandscapeType>(_settings_newgame.game_creation.landscape);
	} else {
		// SFTODO: CASE SENSITIVITY!?
		if (strcmp(climate->value, "temperate") == 0) {
			this->landscape = LT_TEMPERATE;
		} else if (strcmp(climate->value, "arctic") == 0) {
			this->landscape = LT_ARCTIC;
		} else if (strcmp(climate->value, "tropical") == 0) {
			this->landscape = LT_TROPIC;
		} else if (strcmp(climate->value, "toyland") == 0) {
			this->landscape = LT_TOYLAND;
		} else {
			ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_INVALID_CLIMATE, INVALID_STRING_ID, WL_ERROR);
			return;
		}
	}

	uint metadata_width = 0;
	uint metadata_height = 0;
	IniItem *width = extended_heightmap_group->GetItem("width", false);
	if (width != nullptr) {
		metadata_width = atoi(width->value); // SFTODO: NO ERROR CHECKING!
	}
	IniItem *height = extended_heightmap_group->GetItem("height", false);
	if (height != nullptr) {
		metadata_height = atoi(height->value); // SFTODO: NO ERROR CHECKING!
	}

	/* Try to load the heightmap layer. */
	IniGroup *height_layer_group = metadata.GetGroup("height_layer", 0, false);
	if (height_layer_group == nullptr) {
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_MISSING_HEIGHT_LAYER_GROUP, INVALID_STRING_ID, WL_ERROR);
		return;
	}
	// EHTODO: It's probably not a big deal, but do we need to sanitise heightmap_filename so a malicious .ehm file can't
	// access random files on the filesystem?
	IniItem *heightmap_filename = height_layer_group->GetItem("file", false);
	if (heightmap_filename == nullptr) {
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_MISSING_HEIGHT_LAYER_FILE, INVALID_STRING_ID, WL_ERROR);
		return;
	}
	std::auto_ptr<HeightmapLayer> height_layer(new HeightmapLayer(HLT_HEIGHTMAP));
	char *ext = strrchr(heightmap_filename->value, '.');
	if (ext == nullptr) {
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_NO_HEIGHT_LAYER_EXTENSION, INVALID_STRING_ID, WL_ERROR);
		return;
	}
	// SFTODO: CASE SENSITIVITY
	DetailedFileType heightmap_dft;
	if (strcmp(ext, ".png") == 0) {
		heightmap_dft = DFT_HEIGHTMAP_PNG;
	} else if (strcmp(ext, ".bmp") == 0) {
		heightmap_dft = DFT_HEIGHTMAP_BMP;
	} else {
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_UNSUPPORTED_HEIGHT_LAYER_EXTENSION, INVALID_STRING_ID, WL_ERROR);
		return;
	}
	char *heightmap_filename2 = str_fmt("./%s", heightmap_filename->value);
	bool ok = ReadHeightMap(heightmap_dft, heightmap_filename2, &height_layer->width, &height_layer->height, &height_layer->information);
	free(heightmap_filename2);
	if (!ok) {
		// ReadHeightMap() will have displayed an error itself
		return;
	}

	IniItem *min_desired_height = height_layer_group->GetItem("min_desired_height", false);
	if (min_desired_height == nullptr) {
		this->min_map_desired_height = 0;
	} else {
		this->min_map_desired_height = atoi(min_desired_height->value); // SFTODO NO ERROR CHECKING!
	}

	IniItem *max_desired_height = height_layer_group->GetItem("max_desired_height", false);
	if (max_desired_height == nullptr) {
		this->max_map_desired_height = _settings_newgame.construction.max_heightlevel;
	} else {
		this->max_map_desired_height = atoi(max_desired_height->value); // SFTODO NO ERROR CHECKING!
	}
	std::cout << "SFTODOX1 " << static_cast<int>(this->max_map_desired_height) << std::endl;

	IniItem *max_height = height_layer_group->GetItem("max_height", false);
	if (max_height == nullptr) {
		this->max_map_height = 255;
	} else {
		this->max_map_height = atoi(max_height->value); // SFTODO NO ERROR CHECKING!
	}

	IniItem *snow_line_height = height_layer_group->GetItem("snowline_height", false);
	if (snow_line_height == nullptr) {
		this->snow_line_height = _settings_newgame.game_creation.snow_line_height;
	} else {
		this->snow_line_height = atoi(snow_line_height->value); // SFTODO NO ERROR CHECKING!
	}

	if ((metadata_width == 0) && (metadata_height == 0)) {
		// If there's no width/height metadata, take the size of the height layer.
		this->width = height_layer->width;
		this->height = height_layer->height;
	} else {
		// If the extended heightmap creator specified a height and/or width, they know best. This is why
		// they should also specify a rotation if they are specifying height/width and have a non-square
		// height layer. EHTODO: We should probably say you have to specify all of height+width+rotation or
		// none of them. Or maybe we should say if you don't specify a rotation, this code will choose one
		// for you rather than going with whatever happens to be the current default value (though if
		// heightmap is square we can use whatever the default is).
		this->width = (metadata_width != 0) ? metadata_width : height_layer->width;
		this->height = (metadata_height != 0) ? metadata_height : height_layer->height;
	}

	std::cout << "SFTODO: AT THIS PROBABLY TOO EARLY POINT EHM WIDTH IS " << this->width << ", HEIGHT IS " << this->height << std::endl;

	/* Try to load the town layer. */
	std::auto_ptr<TownLayer> town_layer;
	IniGroup *town_layer_group = metadata.GetGroup("town_layer", 0, false);
	if (town_layer_group != nullptr) {
		uint town_layer_width;
		if (!GetUIntGroupItemWithValidation(town_layer_group, "width", nullptr, SFTODOMAXMAXDIMENSION, &town_layer_width)) return;
		uint town_layer_height;
		if (!GetUIntGroupItemWithValidation(town_layer_group, "height", nullptr, SFTODOMAXMAXDIMENSION, &town_layer_height)) return;
		const char *town_layer_file;
		if (!GetStrGroupItem(town_layer_group, "file", nullptr, &town_layer_file)) return;
		uint default_radius;
		if (!GetUIntGroupItemWithValidation(town_layer_group, "radius", "5", 32, &default_radius)) return;

		town_layer = std::auto_ptr<TownLayer>(new TownLayer(town_layer_width, town_layer_height, default_radius, town_layer_file));
		if (!town_layer->valid) {
			// TownLayer()'s constructor will have reported the error
			return;
		}
	}




	// SFTODO: NO LATER THAN THIS POINT, I SHOULD DO OTHER VALIDATION (EG MIN HEIGHT < MAX HEIGHT AND STUFF LIKE THAT)

	/* Now we've loaded everything, populate the layers in this object. SFTODO: BEST APPROACH? */
	this->layers[HLT_HEIGHTMAP] = height_layer.release();
	if (town_layer.get() != nullptr) {
		this->layers[HLT_TOWN] = town_layer.release();
	}

#if 0 // SFTODO: IT'S FAR TOO EARLY TO DO THIS - THE USER *HASN'T* HAD THE CHANCE YET
	/* Now the user has had a chance to adjust the parameters, scale the layers. */
	for (auto &layer : this->layers) {
		if (!layer.second->Scale(this->rotation, this->width, this->height)) {
			assert(false); // SFTODO PROPER ERROR
			// Remove the height layer to make the extended heightmap invalid.
			delete this->layers[HLT_HEIGHTMAP]; this->layers.Erase(HLT_HEIGHTMAP);
			return;
		}
	}
#endif

	assert(IsValid());
}

/**
 * Allows to create an extended heightmap from a single height layer in png or bmp format.
 * @param dft Type of legacy heightmap to load.
 * @param file_path Full path to the legacy heightmap to load.
 * @param file_name Name of the file.
 */
void ExtendedHeightmap::LoadLegacyHeightmap(DetailedFileType dft, char *file_path, char *file_name)
{
	/* Try to load the legacy heightmap first. */
	HeightmapLayer *height_layer = new HeightmapLayer(HLT_HEIGHTMAP);

	if (!ReadHeightMap(dft, file_path, &height_layer->width, &height_layer->height, &height_layer->information)) {
		free(height_layer->information);
		delete height_layer;
		return;
	}

	this->layers[HLT_HEIGHTMAP] = height_layer;

	/* Initialize some extended heightmap parameters to be consistent with the old behavior.
	 * The dialog hasn't been shown to the user yet so we can't initialize everything. SFTODO LAST SENTENCE PROBABLY NOT NEEDED/TRUE ANY MORE. */
	strecpy(this->filename, file_name, lastof(this->filename));
	this->max_map_height = 255;
	this->min_map_desired_height = 0;
	this->max_map_desired_height = _settings_newgame.construction.max_heightlevel;
	this->snow_line_height = _settings_newgame.game_creation.snow_line_height;
	this->width = height_layer->width;
	this->height = height_layer->height;
	this->rotation = static_cast<HeightmapRotation>(_settings_newgame.game_creation.heightmap_rotation);
	this->landscape = static_cast<LandscapeType>(_settings_newgame.game_creation.landscape);
	this->freeform_edges = _settings_newgame.construction.freeform_edges;
}

/**
 * Creates an OpenTTD map based on the information contained in the extended heightmap.
 */
void ExtendedHeightmap::CreateMap()
{
	/* The extended heightmap should be valid before we actually start applying data to the OpenTTD map. */
	assert(this->IsValid());

#if 0 // SFTODO: I THINK THIS IS "DUPLICATING" (INCOMPLETELY) THE CODE IN StartGeneratingLandscapeFromExtendedHeightmap - DELETE IF THIS WORKS OK
	/* The relevant dialog has been shown to the user now so we can populate these members. */
	// EHTODO: This is for legacy heightmap support, but we probably need to
	// do something similar for these (as well as other) members for
	// extended heightmaps.
	this->max_map_desired_height = _settings_game.construction.max_heightlevel;
	this->rotation = (HeightmapRotation) _settings_newgame.game_creation.heightmap_rotation;
#endif

	/* The game map size must have been set up at this point, and the extended heightmap must be correctly initialized. */
	assert((this->rotation == HM_COUNTER_CLOCKWISE && this->width == MapSizeX() && this->height == MapSizeY()) ||
			(this->rotation == HM_CLOCKWISE && this->width == MapSizeY() && this->height == MapSizeX()));
	std::cout << "SFTODO: MAP SIZES ARE NOW SET UP - PRESUMABLY COULDN'T TRUST THEM UP TO THIS POINT" << std::endl;

	/* Apply general extended heightmap properties to the current map. */
	_settings_game.construction.freeform_edges = this->freeform_edges;

	/* Apply all layers. */
	// EHTODO: Currently this is called via GenerateLandscape(); is it really appropriate to generate non-landscape
	// things like towns here? For the moment I've tried to avoid excessive refactoring in this respect.
	this->ApplyLayers();
}

/**
 * Applies all layers to the current map, in the right order.
 */
void ExtendedHeightmap::ApplyLayers()
{
	/* The height layer must always go first. */
	const HeightmapLayer *height_layer = this->layers[HLT_HEIGHTMAP];

	/* Determine scale factors based on the height layer. */
	this->CalculateScaleFactors();

	/* Create the terrain with the height specified by the layer. */
	this->ApplyHeightLayer(height_layer);

	// SFTODO: WE SHOULD PROBABLY ADD AN 'EXTENDED HEIGHT MAP' OPTION TO THE TOWN GENERATION IN THE DIALOG, AND SELECT THAT BY DEFAULT IF WE HAVE A TOWNS LAYER, AND IF THAT IS IN FORCE WE SHOULD INHIBIT AUTO-GENERATION OF TOWNS - THIS WILL ALLOW THE USER TO SUPPLEMENT THE TOWN LAYER IF THEY REALLY WANT, BUT IT WON'T HAPPEN BY DEFAULT

#if 0 // SFTODO DELETE
	const TownLayer *town_layer = static_cast<TownLayer*>(this->layers[HLT_TOWN]);
	if (town_layer) {
		for (const auto &town : town_layer->towns) {
			// SFTODO: NEED TO SCALE X/Y AND ALSO FLIP THEM IF WE'RE IN CLOCKWISE ORIENTATION
			uint32 p1 = TSZ_SMALL; // SFTODO: SET THESE FLAGS APPROPRIATELY
			CommandCost result = CmdFoundTown(TileXY(town.posx, town.posy), DC_EXEC, p1, 0, town.name.c_str());
			if (result.Failed()) {
				char buffer[256];
				GetString(buffer, result.GetErrorMessage(), lastof(buffer));
				std::cout << "TOWN:" << town.name << ": " << buffer << std::endl; // SFTODO!
				// SFTODO!? PERHAPS CREATE AN "XXX:TOWNNAME" SIGN?
			}
		}
	}
#endif
}

// SFTODO: DOXYGEN
// SFTODO: RENAME height_layer TO heightmap_layer HERE? NOT SURE ABOUT THIS CASE YET...
void ExtendedHeightmap::CalculateScaleFactors()
{
	const HeightmapLayer *height_layer = this->layers[HLT_HEIGHTMAP];

	this->row_pad = 0;
	this->col_pad = 0;
	if ((height_layer->width * num_div) / height_layer->height > ((this->width * num_div) / this->height)) {
		/* Image is wider than map - center vertically */
		this->img_scale = (this->width * num_div) / height_layer->width;
		this->row_pad = (1 + this->height - ((height_layer->height * this->img_scale) / num_div)) / 2;
	} else {
		/* Image is taller than map - center horizontally */
		this->img_scale = (this->height * num_div) / height_layer->height;
		this->col_pad = (1 + this->width - ((height_layer->width * this->img_scale) / num_div)) / 2;
	}
}

// SFTODO: DOXYGEN
TileIndex ExtendedHeightmap::TransformedTileXY(const HeightmapLayer *heightmap_layer, uint posx, uint posy)
{
	std::cout << "SFTODOWW BEFORE TRANSFORM POSX " << posx << " POSY " << posy << std::endl;
	assert(posx < heightmap_layer->width);
	assert(posy < heightmap_layer->height);

	// The height layer never distorts; it may be rotated and scaled, but it maintains its aspect
	// ratio. Other layers may have a different aspect ratio than the height layer, and they need
	// to be stretched to match the height layer before any further processing. (If we didn't allow
	// different aspect ratios, we could ignore the height layer here and just run the calculations
	// from CalculateScaleFactors() using this layer's width/height.)
	if (heightmap_layer->type != HLT_HEIGHTMAP) {
		const HeightmapLayer *height_layer = this->layers[HLT_HEIGHTMAP];
		posx = (posx * height_layer->width) / heightmap_layer->width;
		posy = (posy * height_layer->height) / heightmap_layer->height;
		std::cout << "SFTODOWW DISTORY-ONLY TRANSFORM POSX " << posx << " POSY " << posy << std::endl;
	}

	// (posx, posy) coordinates use the lower left corner as (0, 0). The following code is an inversion
	// of the logic in ApplyHeightLayer() so we want to work in terms of the internal bitmap
	// coordinates which have the upper left corner as (0, 0).
	const uint img_col = posx;
	const uint img_row = heightmap_layer->height - 1 - posy;

	// SFTODO INVERSION WIP
	uint row = this->row_pad + ((img_row * this->img_scale) / num_div);
	uint col;
	uint mapx;
	uint mapy;
	switch (this->rotation) {
		case HM_COUNTER_CLOCKWISE:
			col = this->width - 1 - this->col_pad - ((img_col * this->img_scale) / num_div);
			mapx = col;
			mapy = row;
			break;

		case HM_CLOCKWISE:
			col = this->col_pad + ((img_col * this->img_scale) / num_div);
			mapx = row;
			mapy = col;
			break;

		default: NOT_REACHED();
	}

	// Because (for example) a 512x512 heightmap only gives a 510x510 map, (mapx, mapy) may not lie within
	// the map bounds.
	if ((mapx > MapMaxX()) || (mapy > MapMaxY())) return INVALID_TILE;

	std::cout << "AFTER TRANSFORM MAPX " << mapx << " MAPY " << mapy << std::endl;
	return TileXY(mapx, mapy);
}

/**
 * Applies the height layer to the current map.
 * @todo Check if the scaling process can be generalized somehow.
 */
void ExtendedHeightmap::ApplyHeightLayer(const HeightmapLayer *height_layer)
{
	/* This function is meant for heightmap layers only. */
	assert(height_layer->type == HLT_HEIGHTMAP);

	uint row, col;
	uint img_row, img_col;
	TileIndex tile;

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
					(row < this->row_pad) || (row >= (this->height - this->row_pad - (_settings_game.construction.freeform_edges ? 0 : 1))) ||
					(col < this->col_pad) || (col >= (this->width  - this->col_pad - (_settings_game.construction.freeform_edges ? 0 : 1)))) {
				SetTileHeight(tile, 0);
			} else {
				/* Use nearest neighbour resizing to scale map data.
				 *  We rotate the map 45 degrees (counter)clockwise */
				img_row = (((row - this->row_pad) * num_div) / this->img_scale);
				switch (this->rotation) {
					case HM_COUNTER_CLOCKWISE:
						img_col = (((this->width - 1 - col - this->col_pad) * num_div) / this->img_scale);
						break;

					case HM_CLOCKWISE:
						img_col = (((col - this->col_pad) * num_div) / this->img_scale);
						break;

					default: NOT_REACHED();
				}

				assert(img_row < height_layer->height);
				assert(img_col < height_layer->width);

				uint tile_height = height_layer->information[img_row * height_layer->width + img_col];
				if (tile_height > max_map_height) {
					// EHTODO: log any kind of warning!?
					tile_height = max_map_height;
				}
				// If min_map_desired_height is 0 we use the same approach as legacy heightmaps, where 0 is sea
				// and anything above it is land. We need this for compatibility with legacy heightmaps,
				// because simply scaling the greyscale value into the range [min_map_desired_height, max_map_desired_height]
				// will map very-small-but-non-zero greyscales to sea, which may have a dramatic effect on the resulting map.
				// EHTODO: This might be worth tweaking, e.g. perhaps for extended heightmaps we always use the new-style
				// calculation, or maybe we use the new-style calculation iff there's a water layer in the extended heightmap.
				// I think it's likely to be clearer what's the right thing to do when water layer support is added, as it
				// forces some other questions to be addressed like "what happens if this code decides a tile is sea when
				// the water layer says it's land? (and vice versa)" and "how robust are the extended heightmap designer's
				// intentions for sea/land in the face of the user changing the number of heightlevels?".
				if (min_map_desired_height == 0) {
					/* 0 is sea level.
					 * Other grey scales are scaled evenly to the available height levels > 0.
					 * (The coastline is independent from the number of height levels) */
					if (tile_height > 0) {
						tile_height = (1 + (tile_height - 1) * max_map_desired_height / max_map_height);
					}
				} else {
					/* Colour scales from 0 to max_map_height, OpenTTD height scales from min_map_desired_height to max_map_desired_height */
					tile_height = min_map_desired_height + ((tile_height * (max_map_desired_height - min_map_desired_height)) / max_map_height);
				}
				SetTileHeight(tile, tile_height);
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
