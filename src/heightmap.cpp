/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file heightmap.cpp Creating of maps from heightmaps. */

#include "stdafx.h"
#include <memory>
#include "heightmap_type.h"
#include "heightmap_base.h"
#include "bmp.h"
#include "clear_map.h"
#include "error.h"
#include "fileio_func.h"
#include "fios.h"
#include "gfx_func.h"
#include "ini_helper.h"
#include "ini_type.h"
#include "saveload/saveload.h"
#include "string_func.h"
#include "strings_func.h"
#include "void_map.h"

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
	std::string pre;
	std::string buffer;
	std::string post;

	MetadataIniFile() : error(false) {}

        virtual FILE *OpenFile(const char *filename, Subdirectory subdir, size_t *size) {
		return FioFOpenFile(filename, "rb", subdir, size);
	}

	virtual void ReportFileError(const char * const pre, const char * const buffer, const char * const post)
        {
		if (!this->error) {
			this->error = true;
			this->pre = pre;
			this->buffer = buffer;
			this->post = post;
		}
        }
};

/**
 * Check to see if map/layer dimensions are valid and generate an error message if they're not.
 *
 * @param name ini file group name (which won't be translated) for use in the error message
 * @param width width of map/layer
 * @param height height of map/layer
 * @return true if dimensions are valid, false if not
 */
static bool DimensionsValid(const char *name, uint width, uint height)
{
	if ((width  < MIN_MAP_SIZE) || (width  > MAX_MAP_SIZE) || (FindFirstBit(width ) != FindLastBit(width )) ||
	    (height < MIN_MAP_SIZE) || (height > MAX_MAP_SIZE) || (FindFirstBit(height) != FindLastBit(height))) {
		SetDParam(0, width);
		SetDParam(1, height);
		SetDParamStr(2, name);
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_INVALID_DIMENSIONS, INVALID_STRING_ID, WL_ERROR);
		return false;
	}
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
	if (!ts.AddFile(HEIGHTMAP_DIR, file_path)) {
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_OPENING_EHM, INVALID_STRING_ID, WL_ERROR);
		return;
	}
	MetadataIniFile metadata;
	metadata.LoadFromDisk("./metadata.txt", HEIGHTMAP_DIR);
	if (metadata.error) {
		SetDParamStr(0, metadata.pre.c_str());
		SetDParamStr(1, metadata.buffer.c_str());
		SetDParamStr(2, metadata.post.c_str());
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_PARSING_METADATA, INVALID_STRING_ID, WL_ERROR);
		return;
	}
	IniGroup *extended_heightmap_group;
	if (!GetGroup(metadata, "extended_heightmap", false, &extended_heightmap_group)) return;
	const char *format_version;
	if (!GetStrGroupItem(extended_heightmap_group, "format_version", nullptr, &format_version)) return;
	if (strcmp(format_version, "1") != 0) {
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_UNSUPPORTED_VERSION, INVALID_STRING_ID, WL_ERROR);
		return;
	}

	// EHTODO: Should this be "rotation" or "orientation"? Wiki uses both in different places... I've used
	// rotation for the variable as that matches the enum name HeightmapRotation.
	static EnumGroupMap rotation_lookup({
		{"ccw", HM_COUNTER_CLOCKWISE},
		{"cw",  HM_CLOCKWISE}});
	uint rotation;
	if (!GetEnumGroupItem(extended_heightmap_group, "orientation", _settings_newgame.game_creation.heightmap_rotation, rotation_lookup, &rotation)) return;
	this->rotation = static_cast<HeightmapRotation>(rotation);

	static EnumGroupMap climate_lookup({
		{"temperate", LT_TEMPERATE},
		{"arctic",    LT_ARCTIC},
		{"tropical",  LT_TROPIC},
		{"toyland",   LT_TOYLAND}});
	uint climate;
	if (!GetEnumGroupItem(extended_heightmap_group, "climate", _settings_newgame.game_creation.landscape, climate_lookup, &climate)) return;
	this->landscape = static_cast<LandscapeType>(climate);

	uint metadata_width;
	if (!GetUIntGroupItemWithValidation(extended_heightmap_group, "width", 0, MAX_MAP_SIZE, &metadata_width)) return;
	uint metadata_height;
	if (!GetUIntGroupItemWithValidation(extended_heightmap_group, "height", 0, MAX_MAP_SIZE, &metadata_height)) return;

	/* Try to load the heightmap layer. */
	IniGroup *height_layer_group;
	if (!GetGroup(metadata, "height_layer", false, &height_layer_group)) return;
	// EHTODO: It's probably not a big deal, but do we need to sanitise heightmap_filename so a malicious .ehm file can't
	// access random files on the filesystem?
	const char *heightmap_filename;
	if (!GetStrGroupItem(height_layer_group, "file", nullptr, &heightmap_filename)) return;
	std::auto_ptr<HeightmapLayer> height_layer(new HeightmapLayer(HLT_HEIGHTMAP));
	const char *ext = strrchr(heightmap_filename, '.');
	if (ext == nullptr) {
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_NO_HEIGHT_LAYER_EXTENSION, INVALID_STRING_ID, WL_ERROR);
		return;
	}
	DetailedFileType heightmap_dft;
	if (strnatcmp(ext, ".png") == 0) {
		heightmap_dft = DFT_HEIGHTMAP_PNG;
	} else if (strnatcmp(ext, ".bmp") == 0) {
		heightmap_dft = DFT_HEIGHTMAP_BMP;
	} else {
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_UNSUPPORTED_HEIGHT_LAYER_EXTENSION, INVALID_STRING_ID, WL_ERROR);
		return;
	}
	char *heightmap_filename2 = str_fmt("./%s", heightmap_filename);
	bool ok = ReadHeightMap(heightmap_dft, heightmap_filename2, &height_layer->width, &height_layer->height, &height_layer->information);
	free(heightmap_filename2);
	if (!ok) {
		// ReadHeightMap() will have displayed an error itself
		return;
	}

	if (!GetByteGroupItemWithValidation(height_layer_group, "max_desired_height", 15, MAX_TILE_HEIGHT, &this->max_map_desired_height)) return;

	if (!GetByteGroupItemWithValidation(height_layer_group, "min_desired_height", 0, this->max_map_desired_height - 2, &this->min_map_desired_height)) return;

	if (!GetByteGroupItemWithValidation(height_layer_group, "max_height", 255, 255, &this->max_map_height)) return;

	if (!GetByteGroupItemWithValidation(height_layer_group, "snowline_height", _settings_newgame.game_creation.snow_line_height, this->max_map_desired_height, &this->snow_line_height)) return;

	this->width = (metadata_width != 0) ? metadata_width : height_layer->width;
	this->height = (metadata_height != 0) ? metadata_height : height_layer->height;
	if (!DimensionsValid(extended_heightmap_group->name, this->width, this->height)) return;

	/* Try to load the town layer. */
	std::auto_ptr<TownLayer> town_layer;
	IniGroup *town_layer_group = nullptr;
	if (GetGroup(metadata, "town_layer", true, &town_layer_group) && (town_layer_group != nullptr)) {
		uint town_layer_width;
		if (!GetUIntGroupItemWithValidation(town_layer_group, "width", GET_ITEM_NO_DEFAULT, MAX_MAP_SIZE, &town_layer_width)) return;
		uint town_layer_height;
		if (!GetUIntGroupItemWithValidation(town_layer_group, "height", GET_ITEM_NO_DEFAULT, MAX_MAP_SIZE, &town_layer_height)) return;
		const char *town_layer_file;
		if (!GetStrGroupItem(town_layer_group, "file", nullptr, &town_layer_file)) return;
		uint default_radius;
		if (!GetUIntGroupItemWithValidation(town_layer_group, "radius", 5, 64, &default_radius)) return;

		if (!DimensionsValid(town_layer_group->name, town_layer_width, town_layer_height)) return;

		town_layer = std::auto_ptr<TownLayer>(new TownLayer(town_layer_width, town_layer_height, default_radius, town_layer_file));
		if (!town_layer->valid) {
			// TownLayer()'s constructor will have reported the error
			return;
		}
	}

	/* Now we've loaded everything, populate the layers in this object. This way it won't be valid if we returned earlier. */
	this->layers[HLT_HEIGHTMAP] = height_layer.release();
	if (town_layer.get() != nullptr) {
		this->layers[HLT_TOWN] = town_layer.release();
	}

	assert(this->IsValid());
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

	/* Initialize some extended heightmap parameters to be consistent with the old behavior. */
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

	/* The game map size must have been set up at this point, and the extended heightmap must be correctly initialized. */
	assert((this->rotation == HM_COUNTER_CLOCKWISE && this->width == MapSizeX() && this->height == MapSizeY()) ||
			(this->rotation == HM_CLOCKWISE && this->width == MapSizeY() && this->height == MapSizeX()));

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

	/* Town generation is handled in GenerateTowns(). */
}

/**
 * Calculate and cache the scale factors to adjust the height layer to fit the chosen map dimensions.
 */
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

/**
 * Transform a "bitmap coordinate" (posx, posy) from a specific heightmap layer to a TileIndex used to
 * access the main map.
 *
 * @note This may return INVALID_TILE for some inputs, because the map derived from a heightmap is slightly
 * smaller in both dimensions.
 *
 * @param heightmap_layer the heightmap layer posx and posy are associated with
 * @param posx X coordinate within heightmap_layer
 * @param posy Y coordinate within heightmap_layer
 * @returns TileIndex for the corresponding map tile, or INVALID_TILE if there isn't one
 */
TileIndex ExtendedHeightmap::TransformedTileXY(const HeightmapLayer *heightmap_layer, uint posx, uint posy)
{
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
	}

	// (posx, posy) coordinates use the lower left corner as (0, 0). The following code is an inversion
	// of the logic in ApplyHeightLayer() so we want to work in terms of the internal bitmap
	// coordinates which have the upper left corner as (0, 0).
	const uint img_col = posx;
	const uint img_row = heightmap_layer->height - 1 - posy;

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
